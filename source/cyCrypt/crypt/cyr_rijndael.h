/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CRYPT_AES_H_
#define _CYCLONE_CRYPT_AES_H_

#include <cyclone_config.h>

namespace cyclone
{

class Rijndael
{
public:
	enum { BLOCK_SIZE = 16 };
	typedef uint8_t KEY[BLOCK_SIZE];

	//Construct, and expand a user-supplied key material into a session key.
	Rijndael(const KEY key);
	~Rijndael();

public:
	//Encrypt memory, use CBC mode
	//@remark In CBC Mode a ciphertext block is obtained by first xoring the
	//plaintext block with the previous ciphertext block, and encrypting the resulting value.
	//@remark size should be > 0 and multiple of m_blockSize
	void encrypt(uint8_t* buff, size_t size);

	//Decrypt memory, use CBC mode
	//@remark size should be > 0 and multiple of m_blockSize
	void decrypt(uint8_t* buff, size_t size);

private:
	//Convenience method to encrypt exactly one block of plaintext, assuming
	//Rijndael's default block size (128-bit).
	void _encryptBlock(const uint8_t* in, uint8_t* result);

	//Convenience method to decrypt exactly one block of plaintext, assuming
	//Rijndael's default block size (128-bit).
	void _decryptBlock(const uint8_t* in, uint8_t* result);

	//Auxiliary Function
	void _xor(uint8_t* buff, const uint8_t* chain);

private:
	enum {ROUNDS=10, BC=4, KC=4};
	//Encryption (m_Ke) round key
	uint32_t m_Ke[ROUNDS + 1][BC];
	//Decryption (m_Kd) round key
	uint32_t m_Kd[ROUNDS + 1][BC];
};

}

#endif
