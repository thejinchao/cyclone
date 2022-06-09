/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{

class Rijndael
{
public:
	enum { BLOCK_SIZE = 16 };
	typedef uint8_t BLOCK[BLOCK_SIZE];

	//Default Initial Vector
	static const BLOCK DefaultIV;

	//Construct, and expand a user-supplied key material into a session key.
	Rijndael(const BLOCK key);
	~Rijndael();

public:
	//Encrypt memory, use CBC mode
	//@remark In CBC Mode a ciphertext block is obtained by first xoring the
	//plaintext block with the previous ciphertext block, and encrypting the resulting value.
	//@remark size should be > 0 and multiple of m_blockSize
	void encrypt(const uint8_t* input, uint8_t* output, size_t size, BLOCK iv=nullptr);

	//Decrypt memory, use CBC mode
	//@remark size should be > 0 and multiple of m_blockSize
	void decrypt(const uint8_t* input, uint8_t* output, size_t size, BLOCK iv = nullptr);

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
