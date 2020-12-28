// Copyright (C) 2020 Ash Logan <ash@heyquark.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#pragma once

//lil' utility to hide some ugly reading bits
template <typename T>
static void is_read_advance(T& val, std::istream& is) {
	is.read((char*)&val, sizeof(val));
}
template <typename T>
static void os_write_advance(T& val, std::ostream& os) {
	os.write((const char*)&val, sizeof(val));
}

template <typename T, typename U>
static constexpr T alignup(T val, U align) {
	auto mask = (align - 1);
	if ((val & mask) == 0) return val;
	return (val + align) & ~mask;
}
