// Copyright (C) 2016  CBH <maodatou88@163.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "be2_val.hpp"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <string.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <zlib.h>
#include "elf.h"

template <typename T, typename U>
static constexpr T alignup(T val, U align) {
	auto mask = ~(align - 1);
	if ((val & mask) == 0) return val;
	return (val + align) & mask;
}

template <typename InputIterator>
uint32_t crc32_rpx(uint32_t crc, InputIterator first, InputIterator last);

typedef struct {
	Elf32_Ehdr ehdr;
	typedef struct {
		Elf32_Shdr hdr;
		std::vector<uint8_t> data;
		uint32_t crc32;
	} Section;
	std::vector<Section> sections;
} Elf32;

//lil' utility to hide some ugly reading bits
template <typename T>
void is_read_advance(T& val, std::istream& is) {
	is.read((char*)&val, sizeof(val));
}
template <typename T>
void os_write_advance(T& val, std::ostream& os) {
	os.write((const char*)&val, sizeof(val));
}

void writeelf(const Elf32& elf, std::ostream& os) {
	//write elf header out
	os.write((char*)&elf.ehdr, sizeof(elf.ehdr));
	//pad to 0x40
	/*auto pad_size = 0x40 - sizeof(elf.ehdr);
	char pad_bytes[pad_size] = { 0 };
	os.write(pad_bytes, pad_size);*/

	auto shdr_pad = elf.ehdr.e_shentsize - sizeof(elf.sections[0].hdr);

	os.seekp(elf.ehdr.e_shoff.value());
	for (const auto& section : elf.sections) {
		os_write_advance(section.hdr, os);

		if (shdr_pad) os.seekp(shdr_pad, std::ios_base::cur);
	}

	for (const auto& section : elf.sections) {
		os.seekp(section.hdr.sh_offset.value());
		os.write((const char*)section.data.data(), section.data.size());
	}
}

std::optional<Elf32> decompress(std::istream& is)
{
	Elf32 elf;
	is_read_advance(elf.ehdr, is);
	if (memcmp(elf.ehdr.e_ident, "\x7f""ELF", 4) != 0) {
		printf("e_ident bad!\n");
		return std::nullopt;
	}
	if (elf.ehdr.e_type != 0xFE01) {
		printf("e_type bad!\n");
		return std::nullopt;
	}

	//allocate space for section headers
	elf.sections.resize(elf.ehdr.e_shnum);
	std::cout << "shnum " << elf.ehdr.e_shnum << std::endl;
	//get padding (if any) for section header entries
	auto shdr_pad = elf.ehdr.e_shentsize - sizeof(elf.sections[0].hdr);
	//seek to first section header
	is.seekg(elf.ehdr.e_shoff.value());
	//go!
	for (auto& section : elf.sections) {
		//read in section header
		is_read_advance(section.hdr, is);

		//if the section headers are padded advance the stream past it
		if (shdr_pad) is.seekg(shdr_pad, std::ios_base::cur);
	}

	//note the OG wiiurpxtool sorted the section headers here, I'm not bothering

	//some variables to keep track of the current file offset
	auto data_start = elf.ehdr.e_shoff + elf.ehdr.e_shnum * elf.ehdr.e_shentsize;
	auto file_offset = data_start;

	//decompress sections
	for (auto& section : elf.sections) {
		auto& shdr = section.hdr;
		if (!shdr.sh_offset) continue;

		is.seekg(shdr.sh_offset.value());

		if (shdr.sh_flags & SHF_RPL_ZLIB) {
			//read in uncompressed size
			be2_val<uint32_t> uncompressed_sz;
			is_read_advance(uncompressed_sz, is);
			//calc compressed size
			auto compressed_sz = shdr.sh_size - sizeof(uncompressed_sz);

			std::cout << "section at " << std::hex << shdr.sh_addr << " size " << compressed_sz << " uncompresses to " << uncompressed_sz << std::endl;

			z_stream zstream = { 0 };
			inflateInit(&zstream);

			auto remaining_sz = compressed_sz;
			std::array<uint8_t, CHUNK> compressed_chunk;
			std::array<uint8_t, CHUNK> uncompressed_chunk;
			section.data.reserve(uncompressed_sz);

			while (remaining_sz) {
				auto chunk_sz = compressed_chunk.size();
				if (remaining_sz < chunk_sz) {
					chunk_sz = remaining_sz;
				}

				//read in compressed data
				is.read((char*)compressed_chunk.data(), chunk_sz);
				//get bytes actually read successfully
				auto compressed_chunk_sz = is.gcount();
				//pass to zlib
				zstream.avail_in = compressed_chunk_sz;
				zstream.next_in = (Bytef*)compressed_chunk.data();

				//decompress chunk until zlib is happy
				int zret;
				do {
					//reset uncompressed chunk buffer
					zstream.avail_out = uncompressed_chunk.size();
					zstream.next_out = (Bytef*)uncompressed_chunk.data();
					//decompress!
					zret = inflate(&zstream, Z_NO_FLUSH);

					//get size of decompressed data
					auto uncompressed_chunk_sz = uncompressed_chunk.size() - zstream.avail_out;
					//copy uncomressed data into section.data
					std::copy(
						uncompressed_chunk.cbegin(),
						std::next(uncompressed_chunk.cbegin(), uncompressed_chunk_sz),
						std::back_inserter(section.data)
					);
					//update running crc
					section.crc32 = crc32_rpx(
						section.crc32,
						uncompressed_chunk.cbegin(),
						std::next(uncompressed_chunk.cbegin(), uncompressed_chunk_sz)
					);
				} while (zstream.avail_out == 0 && zret == Z_OK);

				remaining_sz -= compressed_chunk_sz;
				printf("did chunk sz %x, %x bytes remaining, output is %x bytes, crc %08x\n", compressed_chunk_sz, remaining_sz, section.data.size(), section.crc32);
			}

			inflateEnd(&zstream);
			//we decompressed this section, so clear the flag
			shdr.sh_flags &= ~SHF_RPL_ZLIB;

			//update other things as needed
			shdr.sh_size = uncompressed_sz;

			//I'd like to use addralign, but the OG tool uses 0x40 so we shall to
			file_offset = alignup(file_offset, 0x40);
			shdr.sh_offset = file_offset;
			file_offset += shdr.sh_size;
		} else {
			//allocate and read the uncompressed data
			section.data.resize(shdr.sh_size);
			is.read((char*)section.data.data(), section.data.size());
			//compute crc, and we're done!
			section.crc32 = crc32_rpx(
				section.crc32,
				section.data.cbegin(),
				section.data.cend()
			);
		}
	}

	return elf;
}

int main(int argc, char** argv) {
	printf("hi!\n");
	std::ifstream infile("test.rpx", std::ios::binary);
	auto elf = decompress(infile);

	std::ofstream outfile("test.d.rpx", std::ios::binary);
	writeelf(*elf, outfile);

	return 0;
}

/*int compress(FILE *in, FILE *out)
{
	Elf32_Ehdr ehdr;
	for(u32 i = 0; i < 0x10; i++)
		ehdr.e_ident[i] = getc(in);
	tempa_u32 = ehdr.e_ident[0]<<24|ehdr.e_ident[1]<<16|ehdr.e_ident[2]<<8|ehdr.e_ident[3];
	if(tempa_u32!=0x7F454C46) return -1;
	fread16_BE(ehdr.e_type, in);
	if(ehdr.e_type != 0xFE01) return -1;
	fread16_BE(ehdr.e_machine, in);
	fread32_BE(ehdr.e_version, in);
	fread32_BE(ehdr.e_entry, in);
	fread32_BE(ehdr.e_phoff, in);
	fread32_BE(ehdr.e_shoff, in);
	fread32_BE(ehdr.e_flags, in);
	fread16_BE(ehdr.e_ehsize, in);
	fread16_BE(ehdr.e_phentsize, in);
	fread16_BE(ehdr.e_phnum, in);
	fread16_BE(ehdr.e_shentsize, in);
	fread16_BE(ehdr.e_shnum, in);
	fread16_BE(ehdr.e_shstrndx, in);
	ulg shdr_data_elf_offset = ehdr.e_shoff + ehdr.e_shnum * ehdr.e_shentsize;
	fwrite(ehdr.e_ident, sizeof(ehdr.e_ident), 1, out);
	fwrite16_BE(ehdr.e_type, out);
	fwrite16_BE(ehdr.e_machine, out);
	fwrite32_BE(ehdr.e_version, out);
	fwrite32_BE(ehdr.e_entry, out);
	fwrite32_BE(ehdr.e_phoff, out);
	fwrite32_BE(ehdr.e_shoff, out);
	fwrite32_BE(ehdr.e_flags, out);
	fwrite16_BE(ehdr.e_ehsize, out);
	fwrite16_BE(ehdr.e_phentsize, out);
	fwrite16_BE(ehdr.e_phnum, out);
	fwrite16_BE(ehdr.e_shentsize, out);
	fwrite16_BE(ehdr.e_shnum, out);
	fwrite16_BE(ehdr.e_shstrndx, out);
	fwrite32_BE(0x00000000, out);
	fwrite32_BE(0x00000000, out);
	fwrite32_BE(0x00000000, out);
	ulg crc_data_offset = 0;
	u32 *crcs = new u32[ehdr.e_shnum];
	vector< Elf32_Shdr_Sort > shdr_table_index;
	Elf32_Shdr *shdr_table = new Elf32_Shdr[ehdr.e_shnum];
	while(ftell(out)<shdr_data_elf_offset) putc(0, out);
	fseek(in, ehdr.e_shoff, 0);
	for (u32 i=0; i<ehdr.e_shnum; i++)
	{
		crcs[i] = 0;
		fread32_BE(shdr_table[i].sh_name, in);
		fread32_BE(shdr_table[i].sh_type, in);
		fread32_BE(shdr_table[i].sh_flags, in);
		fread32_BE(shdr_table[i].sh_addr, in);
		fread32_BE(shdr_table[i].sh_offset, in);
		fread32_BE(shdr_table[i].sh_size, in);
		fread32_BE(shdr_table[i].sh_link, in);
		fread32_BE(shdr_table[i].sh_info, in);
		fread32_BE(shdr_table[i].sh_addralign, in);
		fread32_BE(shdr_table[i].sh_entsize, in);
		if (shdr_table[i].sh_offset != 0)
		{
			Elf32_Shdr_Sort shdr_index;
			shdr_index.index = i;
			shdr_index.sh_offset = shdr_table[i].sh_offset;
			shdr_table_index.push_back(shdr_index);
		}
	}
	sort(shdr_table_index.begin(), shdr_table_index.end(), SortFunc);
	for(vector<Elf32_Shdr_Sort>::iterator shdr_index=shdr_table_index.begin();shdr_index!=shdr_table_index.end();shdr_index++)
	{
		pos = shdr_table[shdr_index->index].sh_offset; fseek(in, pos, 0);
		shdr_table[shdr_index->index].sh_offset = ftell(out);
		if (((shdr_table[shdr_index->index].sh_type & SHT_RPL_FILEINFO) == SHT_RPL_FILEINFO)||
			((shdr_table[shdr_index->index].sh_type & SHT_RPL_CRCS) == SHT_RPL_CRCS)||
			((shdr_table[shdr_index->index].sh_flags & SHF_RPL_ZLIB) == SHF_RPL_ZLIB))
		{
			u32 data_size = shdr_table[shdr_index->index].sh_size;
			u32 block_size = CHUNK;
			while(data_size>0)
			{
				u8 data[CHUNK];
				block_size = CHUNK;
				if(data_size<block_size)
					block_size = data_size;
				data_size -= block_size;
				fread(data, 1, block_size, in);
				fwrite(data, 1, block_size, out);
				crcs[shdr_index->index] = crc32_rpx(crcs[shdr_index->index], data, block_size);
			}
		}
		else
		{
			u32 data_size = shdr_table[shdr_index->index].sh_size;
			u32 block_size = CHUNK;
			u32 have;
			z_stream strm;
			u8 buff_in[CHUNK];
			u8 buff_out[CHUNK];
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			strm.avail_in = 0;
			strm.next_in = Z_NULL;
			if(data_size<CHUNK)
			{
				block_size = data_size;
				deflateInit(&strm, LEVEL);
				strm.avail_in = fread(buff_in, 1, block_size, in);
				crcs[shdr_index->index] = crc32_rpx(crcs[shdr_index->index], buff_in, block_size);
				strm.next_in = buff_in;
				strm.avail_out = CHUNK;
				strm.next_out = buff_out;
				deflate(&strm, Z_FINISH);
				have = CHUNK - strm.avail_out;
				if(have+4 < block_size)
				{
					fwrite32_BE(data_size, out);
					fwrite(buff_out, 1, have, out);
					shdr_table[shdr_index->index].sh_size = have+4;
					shdr_table[shdr_index->index].sh_flags |= SHF_RPL_ZLIB;
				}
				else
					fwrite(buff_in, 1, block_size, out);
				deflateEnd(&strm);
			}
			else
			{
				s32 flush = Z_NO_FLUSH;
				u32 compress_size = 4;
				fwrite32_BE(data_size, out);
				deflateInit(&strm, LEVEL);
				while(data_size>0)
				{
					block_size = CHUNK;
					flush = Z_NO_FLUSH;
					if(data_size <= block_size)
					{
						block_size = data_size;
						flush = Z_FINISH;
					}
					data_size -= block_size;
					strm.avail_in = fread(buff_in, 1, block_size, in);
					crcs[shdr_index->index] = crc32_rpx(crcs[shdr_index->index], buff_in, block_size);
					strm.next_in = buff_in;
					do{
						strm.avail_out = CHUNK;
						strm.next_out = buff_out;
						deflate(&strm, flush);
						have = CHUNK - strm.avail_out;
						fwrite(buff_out, 1, have, out);
						compress_size += have;
					}while(strm.avail_out == 0);
				}
				deflateEnd(&strm);
				shdr_table[shdr_index->index].sh_size = compress_size;
				shdr_table[shdr_index->index].sh_flags |= SHF_RPL_ZLIB;
			}
		}
		while(ftell(out)%0x40!=0) putc(0, out);
		if((shdr_table[shdr_index->index].sh_type & SHT_RPL_CRCS) == SHT_RPL_CRCS)
		{
			crcs[shdr_index->index] = 0;
			crc_data_offset = shdr_table[shdr_index->index].sh_offset;
		}
	}
	fseek(out, ehdr.e_shoff, 0);
	for (u32 i=0; i<ehdr.e_shnum; i++)
	{
		fwrite32_BE(shdr_table[i].sh_name, out);
		fwrite32_BE(shdr_table[i].sh_type, out);
		fwrite32_BE(shdr_table[i].sh_flags, out);
		fwrite32_BE(shdr_table[i].sh_addr, out);
		fwrite32_BE(shdr_table[i].sh_offset, out);
		fwrite32_BE(shdr_table[i].sh_size, out);
		fwrite32_BE(shdr_table[i].sh_link, out);
		fwrite32_BE(shdr_table[i].sh_info, out);
		fwrite32_BE(shdr_table[i].sh_addralign, out);
		fwrite32_BE(shdr_table[i].sh_entsize, out);
	}

	fseek(out, crc_data_offset, 0);
	for (u32 i=0; i<ehdr.e_shnum; i++)
		fwrite32_BE(crcs[i], out);
	delete[]crcs;
	delete[]shdr_table;
	shdr_table_index.clear();
	return 0;
}*/

template <typename InputIterator>
uint32_t crc32_rpx(uint32_t crc, InputIterator first, InputIterator last) {
	u32 crc_table[256];
	for(u32 i=0; i<256; i++)
	{
		u32 c = i;
		for(u32 j=0; j<8; j++)
		{
			if(c & 1)
				c = 0xedb88320L^(c>>1);
			else
				c = c>>1;
		}
		crc_table[i] = c;
	}
	return ~std::accumulate(first, last, ~crc, [&](uint32_t crc, uint8_t val) {
		return (crc >> 8) ^ crc_table[(crc^val) & 0xFF];
	});
}
