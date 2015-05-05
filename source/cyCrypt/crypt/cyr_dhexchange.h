/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CRYPT_DH_EXCHANGE_H_
#define _CYCLONE_CRYPT_DH_EXCHANGE_H_

#include "cyr_uint128.h"

namespace cyclone
{

//Diffie-Hellman key exchange 
class DHExchange
{
public:
	uint128_t getPublicKey(void) const { return m_publicKey; }
	uint128_t getPairKey(const uint128_t& anotherKey);

private:
	// P =  2^128-159 = 0xffffffffffffffffffffffffffffff61 (The biggest 64bit prime)
	// INVERT_P = ~P+1 = 159
	// G = 5
	static uint128_t P, INVERT_P, G;

	// r = a^b % P
	void _powmodp(uint128_t& r, uint128_t a, uint128_t b);
	// r = a^b % P (reduce)
	void _powmodp_r(uint128_t& r, const uint128_t& a, const uint128_t& b);
	// r = a*b % P
	void _mulmodp(uint128_t& r, uint128_t a, uint128_t b);

private:
	uint128_t m_privateKey;
	uint128_t m_publicKey;
	uint128_t m_pairKey;

public:
	DHExchange();
	~DHExchange();
};

}

#endif
