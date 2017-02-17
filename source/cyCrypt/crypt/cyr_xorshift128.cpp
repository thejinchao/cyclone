/*
Copyright(C) thecodeway.com
*/
#include <cy_crypt.h>
#include "cyr_xorshift128.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
void xorshift128(uint8_t* buf, size_t byte_length, XorShift128& seed)
{
	//xor per 8 bytes
	size_t i64_counts = (byte_length >> 3);// +((byte_length & 7) ? 1 : 0);
	size_t tail_bytes = byte_length & 7;

	uint64_t* input = (uint64_t*)buf;
	for (size_t i = 0; i < i64_counts; i++, input++) {
		(*input) ^= seed.next();
	}
	
	if (tail_bytes == 0) return;
	uint64_t last_seed = seed.next();

	for (size_t t = 0; t < tail_bytes; t++) {
		*(((uint8_t*)input) + t) ^= (uint8_t)((last_seed >> (t<<3)) & 0xFF);
	}
}

}
