/*
Copyright(C) thecodeway.com
*/
#pragma once

namespace cyclone
{

struct XorShift128 {
	uint64_t seed0;
	uint64_t seed1;

	void make(void);

	uint64_t next(void) {
		uint64_t x = seed0;
		uint64_t y = seed1;

		seed0 = y;
		x ^= x << 23; // a
		x ^= x >> 17; // b
		x ^= y ^ (y >> 26); // c
		seed1 = x;
		return x + y;
	}
};

void xorshift128(uint8_t* buf, size_t byte_length, XorShift128& seed);

}
