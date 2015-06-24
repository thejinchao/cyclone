/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CRYPT_DH_EXCHANGE_H_
#define _CYCLONE_CRYPT_DH_EXCHANGE_H_

namespace cyclone
{

//Diffie-Hellman key exchange 
#define DH_KEY_LENGTH	(16)

typedef union _dhkey_t {
struct _dq {
		uint64_t low;
		uint64_t high;
	} dq;
	uint8_t bytes[DH_KEY_LENGTH];
} dhkey_t;


void DH_generate_key_pair(dhkey_t& public_key, dhkey_t& private_key);

void DH_generate_key_secret(dhkey_t& secret_key, const dhkey_t& my_private, const dhkey_t& another_public);


}

#endif
