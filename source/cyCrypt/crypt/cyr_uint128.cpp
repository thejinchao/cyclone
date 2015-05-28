/*
Copyright(C) thecodeway.com
*/
#include <cy_crypt.h>
#include "cyr_uint128.h"
#include <math.h>

namespace cyclone
{

//-------------------------------------------------------------------------------------
void uint128_t::make_rand(void)
{
	for (size_t i = 0; i < sizeof(uint64_t); i++)
	{
		low |= ((uint64_t)(::rand() & 0xFF)) << (i * 8);
		high |= ((uint64_t)(::rand() & 0xFF)) << (i * 8);
	}
}

}
