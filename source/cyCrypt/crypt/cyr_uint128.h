/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CRYPT_UINT128_H_
#define _CYCLONE_CRYPT_UINT128_H_

#include <cyclone_config.h>

namespace cyclone
{

struct uint128_t {
	uint64_t low;
	uint64_t high;

	uint128_t() : low(0), high(0) {}
	uint128_t(uint64_t _low) : low(_low), high(0) {}
	uint128_t(const uint128_t& other) : low(other.low), high(other.high) {}
	uint128_t(uint64_t _high, uint64_t _low) {
		low = _low;
		high = _high;
	}

	void INLINE set(uint64_t _high, uint64_t _low) {
		low = _low;
		high = _high;
	}

	void make_rand(void);

	bool INLINE is_zero(void) const {
		return high == 0 && low == 0;
	}

	bool INLINE is_odd(void) const {
		return (low & 1) != 0;
	}

	void shift_l(void) {
		uint64_t t = (low >> 63)& 1;
		high = (high << 1) | t;
		low = low << 1;
	}

	void shift_r(void) {
		uint64_t t = (high & 1) << 63;
		high = high >> 1;
		low = (low >> 1) | t;
	}
};


bool INLINE operator == (const uint128_t& a, const uint128_t& b) { 
	return a.low == b.low && a.high == b.high; 
}

bool INLINE operator < (const uint128_t& a, const uint128_t& b) {
	if (a.high>b.high) return false;
	else if (a.high == b.high) {
		return a.low < b.low;
	}
	else return true;
}

bool INLINE operator <= (const uint128_t& a, const uint128_t& b) {
	if (a.high>b.high) return false;
	else if (a.high == b.high) {
		return a.low <= b.low;
	}
	else return true;
}

bool INLINE operator > (const uint128_t& a, const uint128_t& b) {
	if (a.high<b.high) return false;
	else if (a.high == b.high) {
		return a.low > b.low;
	}
	else return true;
}

bool INLINE operator >= (const uint128_t& a, const uint128_t& b) {
	if (a.high<b.high) return false;
	else if (a.high == b.high) {
		return a.low >= b.low;
	}
	else return true;
}

uint128_t INLINE operator + (const uint128_t& a, const uint64_t& b) {
	uint128_t r;
	uint64_t overflow = 0;
	uint64_t low = a.low + b;
	if (low < a.low || low < b) {
		overflow = 1;
	}

	uint64_t high = a.high + overflow;
	r.high = high;
	r.low = low;
	return r;
}

uint128_t INLINE operator + (const uint128_t& a, const uint128_t& b) {
	uint128_t r;
	uint64_t overflow=0;
	uint64_t low = a.low + b.low;
	if (low < a.low || low < b.low) {
		overflow = 1;
	}

	uint64_t high = a.high + b.high + overflow;
	r.high = high;
	r.low = low;
	return r;
}

uint128_t INLINE operator - (const uint128_t& a, const uint128_t& b) {
	uint128_t r;
	if (a <= b) return r;
	uint128_t invert_b = b;
	invert_b.low = ~invert_b.low;
	invert_b.high = ~invert_b.high;
	invert_b = invert_b + 1;
	return a + invert_b;
}

}
#endif

