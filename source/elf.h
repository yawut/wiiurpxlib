// Copyright (C) 2016  CBH <maodatou88@163.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#pragma once

#include "be2_val.hpp"
#include <cstdint>

#define SHF_RPL_ZLIB	    0x08000000

#define SHT_PROGBITS        0x00000001
#define SHT_SYMTAB          0x00000002
#define SHT_STRTAB          0x00000003
#define SHT_RELA            0x00000004
#define SHT_NOBITS          0x00000008

#define SHT_RPL_EXPORTS     0x80000001
#define SHT_RPL_IMPORTS     0x80000002
#define SHT_RPL_CRCS        0x80000003
#define SHT_RPL_FILEINFO    0x80000004

#define CHUNK 16384
#define LEVEL 6

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef signed   char	s8;
typedef signed   short	s16;
typedef signed   int    s32;
typedef unsigned long	ulg;
typedef unsigned long long u64;
typedef signed long long   s64;

u8  tempa_u8,
    tempb_u8;

u16 tempa_u16,
    tempb_u16;

u32 tempa_u32,
    tempb_u32;

ulg pos;

typedef struct {
	uint8_t  e_ident[0x10];
	be2_val<uint16_t> e_type;
	be2_val<uint16_t> e_machine;
	be2_val<uint32_t> e_version;
	be2_val<uint32_t> e_entry;
	be2_val<uint32_t> e_phoff;
	be2_val<uint32_t> e_shoff;
	be2_val<uint32_t> e_flags;
	be2_val<uint16_t> e_ehsize;
	be2_val<uint16_t> e_phentsize;
	be2_val<uint16_t> e_phnum;
	be2_val<uint16_t> e_shentsize;
	be2_val<uint16_t> e_shnum;
	be2_val<uint16_t> e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	be2_val<uint32_t> sh_name;
	be2_val<uint32_t> sh_type;
	be2_val<uint32_t> sh_flags;
	be2_val<uint32_t> sh_addr;
	be2_val<uint32_t> sh_offset;
	be2_val<uint32_t> sh_size;
	be2_val<uint32_t> sh_link;
	be2_val<uint32_t> sh_info;
	be2_val<uint32_t> sh_addralign;
	be2_val<uint32_t> sh_entsize;
} Elf32_Shdr;

typedef struct {
	u32 index;
	u32 sh_offset;
} Elf32_Shdr_Sort;

typedef struct {
	u32 st_name;
	u32 st_value;
	u32 st_size;
	u8  st_info;
	u8  st_other;
	u16 st_shndx;
} Elf32_Sym;

typedef struct {
	u32 r_offset;
	u32 r_info;
	s32 r_addend;
} Elf32_Rela;

typedef struct {
	u32 magic_version;
	u32 mRegBytes_Text;
	u32 mRegBytes_TextAlign;
	u32 mRegBytes_Data;
	u32 mRegBytes_DataAlign;
	u32 mRegBytes_LoaderInfo;
	u32 mRegBytes_LoaderInfoAlign;
	u32 mRegBytes_Temp;
	u32 mTrampAdj;
	u32 mSDABase;
	u32 mSDA2Base;
	u32 mSizeCoreStacks;
	u32 mSrcFileNameOffset;
	u32 mFlags;
	u32 mSysHeapBytes;
	u32 mTagsOffset;
} Rpl_Fileinfo;
