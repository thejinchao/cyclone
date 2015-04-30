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
	typedef char KEY[BLOCK_SIZE];

	//Construct, and expand a user-supplied key material into a session key.
	Rijndael(const KEY key);
	~Rijndael();

public:
	//Encrypt memory, use CBC mode
	//@remark In CBC Mode a ciphertext block is obtained by first xoring the
	//plaintext block with the previous ciphertext block, and encrypting the resulting value.
	//@remark size should be > 0 and multiple of m_blockSize
	void encrypt(const char* in, size_t size, char* result);

	//Decrypt memory, use CBC mode
	//@remark size should be > 0 and multiple of m_blockSize
	void decrypt(const char* in, size_t size, char* result);

private:
	//Convenience method to encrypt exactly one block of plaintext, assuming
	//Rijndael's default block size (128-bit).
	void _encryptBlock(const char* in, char* result);

	//Convenience method to decrypt exactly one block of plaintext, assuming
	//Rijndael's default block size (128-bit).
	void _decryptBlock(const char* in, char* result);

	//Auxiliary Function
	void _xor(char* buff, const char* chain);

private:
	enum {ROUNDS=10, BC=4, KC=4};
	//Encryption (m_Ke) round key
	int m_Ke[ROUNDS + 1][BC];
	//Decryption (m_Kd) round key
	int m_Kd[ROUNDS + 1][BC];
};

}

#endif
