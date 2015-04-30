/*
Copyright(C) thecodeway.com
*/
#include <cy_crypt.h>
#include "cyr_dhexchange.h"

namespace cyclone
{

/*
copy from skynet @cloudwu
https://github.com/cloudwu/skynet
*/

// The biggest 64bit prime
#define P (0xffffffffffffffc5ull)
#define G (5)

// The biggest 128bit prime is 2^128-159=0xffffffffffffffffffffffffffffff61

//-------------------------------------------------------------------------------------
static inline uint64_t 
mul_mod_p(uint64_t a, uint64_t b) {
	uint64_t m = 0;
	while (b) {
		if (b & 1) {
			uint64_t t = P - a;
			if (m >= t) {
				m -= t;
			}
			else {
				m += a;
			}
		}
		if (a >= P - a) {
			a = a * 2 - P;
		}
		else {
			a = a * 2;
		}
		b >>= 1;
	}
	return m;
}

//-------------------------------------------------------------------------------------
static inline uint64_t
pow_mod_p(uint64_t a, uint64_t b) {
	if (b == 1) {
		return a;
	}
	uint64_t t = pow_mod_p(a, b >> 1);
	t = mul_mod_p(t, t);
	if (b % 2) {
		t = mul_mod_p(t, a);
	}
	return t;
}

//-------------------------------------------------------------------------------------
// calc a^b % P
static uint64_t
powmodp(uint64_t a, uint64_t b) {
	if (a > P)
		a %= P;
	return pow_mod_p(a, b);
}

//-------------------------------------------------------------------------------------
DHExchange::DHExchange()
	: m_privateKey(0)
	, m_publicKey(0)
	, m_pairKey(0)
{
	//create random private key(a)
	for (int i = 0; i < sizeof(uint64_t); i++)
	{
		m_privateKey |= ((uint64_t)(rand() & 0xFF)) << (i * 8);
	}

	//public key = G^a mod P
	m_publicKey = powmodp(G, m_privateKey);
}

//-------------------------------------------------------------------------------------
DHExchange::~DHExchange()
{

}

//-------------------------------------------------------------------------------------
uint64_t DHExchange::getPairKey(uint64_t anotherKey)
{
	//pair key = B^a mod p;
	m_pairKey = powmodp(anotherKey, m_privateKey);
	return m_pairKey;
}


}

