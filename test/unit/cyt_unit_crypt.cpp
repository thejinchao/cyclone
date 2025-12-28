#include <cy_core.h>
#include <cy_crypt.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
void _fillRandom(uint8_t* mem, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		mem[i] = (uint8_t)(rand() & 0xFF);
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("Basic test for Adler32", "[Adler32]")
{
	PRINT_CURRENT_TEST_NAME();

	REQUIRE_EQ(INITIAL_ADLER, adler32(0, 0, 0));
	REQUIRE_EQ(INITIAL_ADLER, adler32(0xFFFFFFFFul, 0, 0));

	const char* hello = "Hello,World!";
	uint32_t adler = adler32(INITIAL_ADLER, (const uint8_t*)hello, strlen(hello));
	REQUIRE_EQ(0x1c9d044aul, adler);

	const char* force = "May the Force be with you";
	adler = adler32(INITIAL_ADLER, (const uint8_t*)force, strlen(force));
	REQUIRE_EQ(0x6fe408d8ul, adler);
	
	const uint8_t data_buf[] = {
		0x80,0x8a,0xdc,0x82,0xec,0x0b,0x42,0xd1,0xb8,0xb8,0x4c,0xc8,0xdb,0x7a,0xcb,0x3e,
		0xe0,0x7d,0xca,0x65,0x3b,0x36,0x7d,0xf4,0xdd,0xa5,0x74,0x85,0x06,0xd7,0x14,0x3b,
		0x5b,0xb0,0x48,0xa9,0x38,0xe7,0x74,0xef,0x47,0x52,0xab,0x26,0x52,0x64,0x21,0xff,
		0x55,0xf4,0xe3,0xa6,0xd8,0x3f,0xc5,0xed,0x7b,0x31,0x9c,0xa6,0xd3,0xe0,0xae,0x50
	};
	size_t data_length = sizeof(data_buf);

	uint32_t adler1 = adler32(INITIAL_ADLER, data_buf, data_length);
	REQUIRE_EQ(0x75c12362ul, adler1);

	size_t first = 33;
	uint32_t adler2 = adler32(INITIAL_ADLER, data_buf, first);
	adler2 = adler32(adler2, data_buf+ first, data_length- first);
	REQUIRE_EQ(0x75c12362ul, adler2);
}

//-------------------------------------------------------------------------------------
TEST_CASE("Random test for Adler32", "[Adler32]")
{
	PRINT_CURRENT_TEST_NAME();

	const size_t buf_cap = 257;
	uint8_t random_buf[buf_cap] = { 0 };

	const size_t test_counts = 100;
	for (size_t i = 0; i < test_counts; i++) {
		
		size_t buf_size = buf_cap - (size_t)(rand() % 32);
		//fill random data
		for (size_t j = 0; j < buf_cap; j++) {
			random_buf[j] = (uint8_t)(rand() & 0xFF);
		}

		//adler total
		uint32_t adler1 = adler32(INITIAL_ADLER, random_buf, buf_size);

		size_t first_part = (size_t)rand()%(buf_size - 1) + 1;
		uint32_t adler2 = adler32(INITIAL_ADLER, random_buf, first_part);
		adler2 = adler32(adler2, random_buf + first_part, buf_size - first_part);

		REQUIRE_EQ(adler1, adler2);
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("Random test for DHExchange", "[DHExchange]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t TEST_COUNTS = 20;
	for (int32_t i = 0; i < TEST_COUNTS; i++) {

		//Alice Generate private key and public key
		dhkey_t alice_private, alice_public;
		DH_generate_key_pair(alice_public, alice_private);

		//Bob Generate private key and public key
		dhkey_t bob_private, bob_public;
		DH_generate_key_pair(bob_public, bob_private);

		//Alice Generate secret key
		dhkey_t alice_secret;
		DH_generate_key_secret(alice_secret, alice_private, bob_public);

		//Bob Generate secret key
		dhkey_t bob_secret;
		DH_generate_key_secret(bob_secret, bob_private, alice_public);

		REQUIRE_EQ(alice_secret.dq.low, bob_secret.dq.low);
		REQUIRE_EQ(alice_secret.dq.high, bob_secret.dq.high);
	}

}

//-------------------------------------------------------------------------------------
TEST_CASE("Basic test for XorShift128", "[XorShift128]")
{
	PRINT_CURRENT_TEST_NAME();

	XorShift128 seed;
	seed.seed0 = 0xFACEDEADDEADFACEull;
	seed.seed1 = 0x1234567812345678ull;

	REQUIRE_EQ(0xd049deb4f4d8c4dcULL, seed.next());
	REQUIRE_EQ(0x4e3e5b9bd2800ea2ULL, seed.next());
	REQUIRE_EQ(0xc0752ca2482a91f1ULL, seed.next());
	REQUIRE_EQ(0x3f5fd1b17d136ae9ULL, seed.next());
	REQUIRE_EQ(0xae06c714838dcd21ULL, seed.next());
	REQUIRE_EQ(0x45e5977a2fc13093ULL, seed.next());
	REQUIRE_EQ(0x12204cefa3234abbULL, seed.next());
	REQUIRE_EQ(0x0f6254cd56eee447ULL, seed.next());
	REQUIRE_EQ(0x6727c3095f6dcfc7ULL, seed.next());
	REQUIRE_EQ(0x00f8af6ebadb4ec3ULL, seed.next());

	//test string decode and encode
	const char* plain_text = "And God said, Let there be light: and there was light.";
	const uint8_t encrypt_text[] = {
		0x9d,0xaa,0xbc,0xd4,0xf3,0xb1,0x2d,0xf0,0xd1,0x6f,0xe9,0xb6,0xb7,0x7b,0x72,0x2b,
		0x85,0xb1,0x5e,0x20,0xc7,0x5e,0x10,0xe0,0x8b,0x0f,0x33,0x11,0xd8,0xb6,0x37,0x4b,
		0x1b,0xed,0xec,0xed,0x70,0xe7,0x72,0xc6,0xf6,0x42,0xa4,0x0f,0x0d,0xf6,0x96,0x65,
		0xd7,0x23,0x44,0xcb,0x9b,0x62
	};

	size_t text_len = strlen(plain_text);

	char buf[128] = { 0 };
	strncpy(buf, plain_text, text_len);

	//encrypt with special seed
	seed.seed0 = 0xFACEDEADDEADFACEull;
	seed.seed1 = 0x1234567812345678ull;
	xorshift128((uint8_t*)buf, text_len, seed);

	REQUIRE_EQ(0, memcmp(buf, encrypt_text, text_len));

	seed.seed0 = 0xFACEDEADDEADFACEull;
	seed.seed1 = 0x1234567812345678ull;
	xorshift128((uint8_t*)buf, text_len, seed);

	REQUIRE_THAT(plain_text, Catch::Matchers::Equals(buf));

	//encrypt with random seed
	const int32_t TEST_COUNTS = 20;
	for (int32_t i = 0; i < TEST_COUNTS; i++) {
		XorShift128 seed_encrypt, seed_decrypt;
		seed_encrypt.make();
		seed_decrypt.seed0 = seed_encrypt.seed0;
		seed_decrypt.seed1 = seed_encrypt.seed1;

		strncpy(buf, plain_text, text_len);
		xorshift128((uint8_t*)buf, text_len, seed_encrypt);
		xorshift128((uint8_t*)buf, text_len, seed_decrypt);
		REQUIRE_THAT(plain_text, Catch::Matchers::Equals(buf));
	}

}

//-------------------------------------------------------------------------------------
TEST_CASE("Basic test for Rijndael", "[Rijndael]")
{
	PRINT_CURRENT_TEST_NAME();

	Rijndael::BLOCK key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	Rijndael aes(key);

	const char* plain_text = "And God called the light Day,  and the darkness he called Night.";
	const uint8_t encrypt_text[] = {
		0xe7, 0x05, 0x0e, 0xdf, 0x2e, 0x5d, 0x97, 0x62, 0x36, 0xe9, 0x17, 0xb1, 0xc1, 0x73, 0xde, 0xca,
		0xa2, 0x4b, 0x50, 0x4c, 0x02, 0x49, 0xea, 0xbd, 0x26, 0x25, 0x76, 0x92, 0x7a, 0xcf, 0x68, 0xee,
		0xa7, 0xa6, 0xc3, 0x75, 0xa7, 0x32, 0x13, 0x74, 0x31, 0x0f, 0xa9, 0xca, 0x0e, 0x5e, 0xab, 0x99,
		0xc5, 0x31, 0xc0, 0xe4, 0x26, 0x9c, 0x26, 0x92, 0x1a, 0xf4, 0xd0, 0xd0, 0xef, 0xa8, 0x7b, 0x23 };
	const Rijndael::BLOCK iv_check = { 0xc5, 0x31, 0xc0, 0xe4, 0x26, 0x9c, 0x26, 0x92, 0x1a, 0xf4, 0xd0, 0xd0, 0xef, 0xa8, 0x7b, 0x23 };

	size_t text_len = strlen(plain_text);
	REQUIRE_EQ(text_len%Rijndael::BLOCK_SIZE, 0ull);

	uint8_t buf1[128] = { 0 }, buf2[128] = { 0 };
	memcpy(buf1, plain_text, text_len);

	//Encrypt once
	aes.encrypt((const uint8_t*)plain_text, buf1, text_len);
	REQUIRE_EQ(0, memcmp(buf1, encrypt_text, text_len));

	//Decrypt once
	aes.decrypt(buf1, buf2, text_len);
	REQUIRE_EQ(0, memcmp(buf2, plain_text, text_len));

	//Encrypt/Decrypt part
	Rijndael::BLOCK iv_buf;

	memcpy(iv_buf, Rijndael::DefaultIV, Rijndael::BLOCK_SIZE);
	for (size_t i = 0; i < text_len; i += Rijndael::BLOCK_SIZE) {
		aes.encrypt((const uint8_t*)plain_text + i, buf1 + i, Rijndael::BLOCK_SIZE, iv_buf);
	}
	REQUIRE_EQ(0, memcmp(encrypt_text, buf1, text_len));
	REQUIRE_EQ(0, memcmp(iv_check, iv_buf, Rijndael::BLOCK_SIZE));
	memset(buf1, 0, text_len);

	memcpy(iv_buf, Rijndael::DefaultIV, Rijndael::BLOCK_SIZE);
	for (size_t i = 0; i < text_len; i += Rijndael::BLOCK_SIZE) {
		aes.decrypt(encrypt_text + i, buf1 + i, Rijndael::BLOCK_SIZE, iv_buf);
	}
	REQUIRE_EQ(0, memcmp(plain_text, buf1, text_len));
	REQUIRE_EQ(0, memcmp(iv_check, iv_buf, Rijndael::BLOCK_SIZE));
	memset(buf1, 0, text_len);

	//Encrypt self
	memcpy(buf1, plain_text, text_len);
	aes.encrypt(buf1, buf1, text_len);
	REQUIRE_EQ(0, memcmp(encrypt_text, buf1, text_len));
	memset(buf1, 0, text_len);

	//Decrypt self
	memcpy(buf1, encrypt_text, text_len);
	aes.decrypt(buf1, buf1, text_len);
	REQUIRE_EQ(0, memcmp(plain_text, buf1, text_len));
	memset(buf1, 0, text_len);


	//encrypt with random seed
	const int32_t TEST_COUNTS = 20;
	for (int32_t i = 0; i < TEST_COUNTS; i++) {
		Rijndael::BLOCK key_random;
		_fillRandom(key_random, Rijndael::BLOCK_SIZE);
		Rijndael aes2(key_random);

		const size_t buf_size = 128;
		_fillRandom(buf1, buf_size);

		aes2.encrypt(buf1, buf2, buf_size);
		aes2.decrypt(buf2, buf2, buf_size);
		REQUIRE_EQ(0, memcmp(buf1, buf2, buf_size));
	}
}

}
