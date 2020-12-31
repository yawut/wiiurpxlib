// Copyright (C) 2016  CBH <maodatou88@163.com>, 2020 Ash Logan <ash@heyquark.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "rpx.hpp"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <forward_list>
#include <cstdint>
#include <string.h>
#include <vector>
#include <span>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <zlib.h>
#include "util.hpp"
#include "crc32.hpp"

#define CHUNK 16384
#define ZLIB_LEVEL 6

using namespace rpx;

void rpx::writerpx(const rpx& elf, std::ostream& os) {
	//write elf header out
	os.write((char*)&elf.ehdr, sizeof(elf.ehdr));

	auto shdr_pad = elf.ehdr.e_shentsize - sizeof(elf.sections[0].hdr);

	os.seekp(elf.ehdr.e_shoff.value());
	for (const auto& section : elf.sections) {
		os_write_advance(section.hdr, os);

		if (shdr_pad) os.seekp(shdr_pad, std::ios_base::cur);
	}

	//these variables are a bit weird but it's optimisation I swear
	uint32_t file_offset;
	uint32_t size;
	for (auto section_index : elf.section_file_order) {
		const auto& section = elf.sections[section_index];

		file_offset = section.hdr.sh_offset.value();
		os.seekp(file_offset);
		size = section.data.size();
		os.write((const char*)section.data.data(), size);
	}
	file_offset += size;
	//if file length is not aligned to 0x40...
	if (file_offset & (0x40 - 1)) {
		//pad end of file
		os.seekp(alignup(file_offset, 0x40) - 1);
		os.put(0x00);
	}
}

size_t rpx::writerpxsize(const rpx& rpx) {
	auto& last_section = rpx.sections[rpx.section_file_order.back()];
	size_t length = last_section.hdr.sh_offset.value() + last_section.hdr.sh_size.value();

	if (length & (0x40 - 1)) {
		length = alignup(length, 0x40);
	}

	return length;
}

std::optional<rpx::rpx> rpx::readrpx(std::istream& is) {
	rpx elf;
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

	//sort by file offset, so we always seek forwards and maintain file order
	elf.section_file_order.resize(elf.sections.size());
	std::iota(elf.section_file_order.begin(), elf.section_file_order.end(), 0);
	std::sort(elf.section_file_order.begin(), elf.section_file_order.end(),
		[=] (const auto& a, const auto& b) {
			return elf.sections[a].hdr.sh_offset < elf.sections[b].hdr.sh_offset;
		}
	);

	//read section data
	for (auto& section_index : elf.section_file_order) {
		auto& section = elf.sections[section_index];
		auto& shdr = section.hdr;
		if (!shdr.sh_offset) continue;

		is.seekg(shdr.sh_offset.value());

		//allocate and read the uncompressed data
		section.data.resize(shdr.sh_size);
		is.read((char*)section.data.data(), section.data.size());
	}

	return elf;
}

void rpx::relink(rpx& elf) {
	//some variables to keep track of the current file offset
	auto data_start = elf.ehdr.e_shoff + elf.ehdr.e_shnum * elf.ehdr.e_shentsize;
	auto file_offset = data_start;

	//find the crc32s
	auto crc_section = std::find_if(elf.sections.begin(), elf.sections.end(), [](rpx::Section& s){
		return s.hdr.sh_type == SHT_RPL_CRCS;
	});
	crc_section->crc32 = 0;
	//spans: keeping all the jank in one place since 2020
	std::span<be2_val<uint32_t>> crcs(
		(be2_val<uint32_t>*)crc_section->data.data(),
		crc_section->data.size() / sizeof(be2_val<uint32_t>)
	);

	bool first_section = true;
	for (auto section_index : elf.section_file_order) {
		auto& section = elf.sections[section_index];
		auto& shdr = section.hdr;

		crcs[section_index] = section.crc32;

		if (!shdr.sh_offset) continue;

		shdr.sh_size = (uint32_t)section.data.size();

		//this bit is replicating some interesting quirks of the original tool
		//I don't like it but I'm not one to argue too much
		if (!first_section) {
			file_offset = alignup(file_offset, 0x40);
		} else first_section = false;

		shdr.sh_offset = file_offset;
		file_offset += shdr.sh_size;
	}
}

void rpx::decompress(rpx& elf) {
	//decompress sections
	for (auto& section : elf.sections) {
		auto& shdr = section.hdr;
		if (!shdr.sh_offset) continue;

		if (shdr.sh_flags & SHF_RPL_ZLIB) {
			//read in uncompressed size
			be2_val<uint32_t> uncompressed_sz;
			memcpy(&uncompressed_sz, section.data.data(), sizeof(uncompressed_sz));
			//calc compressed size

			z_stream zstream = { 0 };
			inflateInit(&zstream);

			//pass to zlib
			zstream.avail_in = section.data.size() - sizeof(uncompressed_sz);
			zstream.next_in = (Bytef*)section.data.data() + sizeof(uncompressed_sz);

			std::vector<uint8_t> uncompressed_data;
			uncompressed_data.resize(uncompressed_sz);

			//reset uncompressed chunk buffer
			zstream.avail_out = uncompressed_data.size();
			zstream.next_out = (Bytef*)uncompressed_data.data();

			//decompress!
			int zret = inflate(&zstream, Z_FINISH);

			inflateEnd(&zstream);

			section.data = std::move(uncompressed_data);

			//we decompressed this section, so clear the flag
			shdr.sh_flags &= ~SHF_RPL_ZLIB;
			shdr.sh_size = (uint32_t)section.data.size();
		}
		//compute crc
		section.crc32 = crc32_rpx(
			0,
			section.data.cbegin(),
			section.data.cend()
		);
	}

	//relink elf to adjust file offsets
	relink(elf);
}

void rpx::compress(rpx& elf) {
	for (auto& section : elf.sections) {
		auto& shdr = section.hdr;
		if (!shdr.sh_offset) continue;

		//compute crc
		section.crc32 = crc32_rpx(
			0,
			section.data.cbegin(),
			section.data.cend()
		);

		if (shdr.sh_type == SHT_RPL_FILEINFO || shdr.sh_type == SHT_RPL_CRCS ||
			shdr.sh_flags & SHF_RPL_ZLIB) continue;

		be2_val<uint32_t> uncompressed_sz = (uint32_t)section.data.size();

		z_stream zstream = { 0 };
		deflateInit(&zstream, ZLIB_LEVEL);

		//pass to zlib
		zstream.avail_in = section.data.size();
		zstream.next_in = (Bytef*)section.data.data();

		std::vector<uint8_t> compressed_data;
		compressed_data.resize(deflateBound(&zstream, zstream.avail_in) + sizeof(uncompressed_sz));

		zstream.avail_out = compressed_data.size() - sizeof(uncompressed_sz);
		zstream.next_out = (Bytef*)compressed_data.data() + sizeof(uncompressed_sz);

		//given deflateBound, this is guaranteed to succeed
		int zret = deflate(&zstream, Z_FINISH);

		compressed_data.resize(zstream.total_out + sizeof(uncompressed_sz));
		memcpy(compressed_data.data(), &uncompressed_sz, sizeof(uncompressed_sz));

		deflateEnd(&zstream);

		//not really sure how the original tool does this, but it sure does
		if (compressed_data.size() >= section.data.size()) continue;

		compressed_data.shrink_to_fit();
		section.data = std::move(compressed_data);

		//we compressed this section, so update the flag
		shdr.sh_flags |= SHF_RPL_ZLIB;
		shdr.sh_size = (uint32_t)section.data.size();
	}

	relink(elf);
}
