#pragma once

#include <iterator>
#include <cstdint>

template <typename InputIterator>
static uint32_t crc32_rpx(uint32_t crc, InputIterator first, InputIterator last) {
	uint32_t crc_table[256];
	for(uint32_t i=0; i<256; i++)
	{
		uint32_t c = i;
		for(uint32_t j=0; j<8; j++)
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
