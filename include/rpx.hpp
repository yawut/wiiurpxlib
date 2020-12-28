// Copyright (C) 2020 Ash Logan <ash@heyquark.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#pragma once

#include "_rpx_elf.hpp"
#include <vector>
#include <cstdint>
#include <forward_list>
#include <optional>
#include <iostream>

namespace rpx {

typedef struct {
	Elf32_Ehdr ehdr;
	typedef struct {
		Elf32_Shdr hdr;
		std::vector<uint8_t> data;
		uint32_t crc32;
	} Section;
	std::vector<Section> sections;
	std::forward_list<size_t> section_file_order;
} rpx;

//reads a file into an rpx struct.
std::optional<rpx> readrpx(std::istream& is);
//writes an rpx struct back to a file.
void writerpx(const rpx& rpx, std::ostream& os);

//re-links the rpx, adjusting file offsets as needed.
//does not touch virtual addresses.
void relink(rpx& rpx);
//decompresses any zlib sections (SHF_RPL_ZLIB) in the rpx and relinks.
void decompress(rpx& rpx);
//compresses any eligible sections with zlib (SHF_RPL_ZLIB) and relinks.
void compress(rpx& rpx);

};
