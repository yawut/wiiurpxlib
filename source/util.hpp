#pragma once

//lil' utility to hide some ugly reading bits
template <typename T>
void is_read_advance(T& val, std::istream& is) {
	is.read((char*)&val, sizeof(val));
}
template <typename T>
void os_write_advance(T& val, std::ostream& os) {
	os.write((const char*)&val, sizeof(val));
}

template <typename T, typename U>
static constexpr T alignup(T val, U align) {
	auto mask = (align - 1);
	if ((val & mask) == 0) return val;
	return (val + align) & ~mask;
}
