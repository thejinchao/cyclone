/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CRYPT_DH_EXCHANGE_H_
#define _CYCLONE_CRYPT_DH_EXCHANGE_H_

#include <cyclone_config.h>

namespace cyclone
{

//Diffie-Hellman key exchange 
class DHExchange
{
public:
	uint64_t getPublicKey(void) { return m_publicKey; }
	uint64_t getPairKey(uint64_t anotherKey);

private:

private:
	uint64_t m_privateKey;
	uint64_t m_publicKey;
	uint64_t m_pairKey;

public:
	DHExchange();
	~DHExchange();
};

}

#endif
