#include <cy_core.h>
#include <cy_crypt.h>
#include <gtest/gtest.h>

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST(Adler32, Basic)
{
	EXPECT_EQ(1ul, adler32(0, 0, 0));
	EXPECT_EQ(1ul, adler32(0xFFFFFFFFul, 0, 0));

	const char* hello = "Hello,World!";
	uint32_t adler = adler32(0, 0, 0);
	adler = adler32(adler, (const uint8_t*)hello, strlen(hello));
	EXPECT_EQ(0x1c9d044aul, adler);

	const char* force = "May the Force be with you";
	adler = adler32(0, 0, 0);
	adler = adler32(adler, (const uint8_t*)force, strlen(force));
	EXPECT_EQ(0x6fe408d8ul, adler);
	
	const uint8_t data_buf[] = {
		0x80,0x8a,0xdc,0x82,0xec,0x0b,0x42,0xd1,0xb8,0xb8,0x4c,0xc8,0xdb,0x7a,0xcb,0x3e,
		0xe0,0x7d,0xca,0x65,0x3b,0x36,0x7d,0xf4,0xdd,0xa5,0x74,0x85,0x06,0xd7,0x14,0x3b,
		0x5b,0xb0,0x48,0xa9,0x38,0xe7,0x74,0xef,0x47,0x52,0xab,0x26,0x52,0x64,0x21,0xff,
		0x55,0xf4,0xe3,0xa6,0xd8,0x3f,0xc5,0xed,0x7b,0x31,0x9c,0xa6,0xd3,0xe0,0xae,0x50
	};
	size_t data_length = sizeof(data_buf);

	uint32_t adler1 = adler32(0, 0, 0);
	adler1 = adler32(adler1, data_buf, data_length);
	EXPECT_EQ(0x75c12362ul, adler1);

	size_t first = 33;
	uint32_t adler2 = adler32(0, 0, 0);
	adler2 = adler32(adler2, data_buf, first);
	adler2 = adler32(adler2, data_buf+ first, data_length- first);
	EXPECT_EQ(0x75c12362ul, adler2);
}

//-------------------------------------------------------------------------------------
TEST(Adler32, Random)
{
	const size_t buf_cap = 257;
	uint8_t random_buf[buf_cap] = { 0 };

	const size_t test_counts = 100;
	for (size_t i = 0; i < test_counts; i++) {
		
		size_t buf_size = buf_cap - rand() % 32;
		//fill random data
		for (size_t j = 0; j < buf_cap; j++) {
			random_buf[j] = (uint8_t)(rand() & 0xFF);
		}

		//adler total
		uint32_t adler1 = adler32(0, 0, 0);
		adler1 = adler32(adler1, random_buf, buf_size);

		size_t first_part = (size_t)rand()%(buf_size - 1) + 1;
		uint32_t adler2 = adler32(0, 0, 0);
		adler2 = adler32(adler2, random_buf, first_part);
		adler2 = adler32(adler2, random_buf + first_part, buf_size - first_part);

		EXPECT_EQ(adler1, adler2);
	}
}

//-------------------------------------------------------------------------------------
TEST(DHExchange, Random)
{
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

		EXPECT_EQ(alice_secret.dq.low, bob_secret.dq.low);
		EXPECT_EQ(alice_secret.dq.high, bob_secret.dq.high);
	}

}

//-------------------------------------------------------------------------------------
TEST(XorShift128, Basic)
{
	XorShift128 seed;
	seed.seed0 = 0xFACEDEADDEADFACEull;
	seed.seed1 = 0x1234567812345678ull;

	EXPECT_EQ(0xd049deb4f4d8c4dcULL, seed.next());
	EXPECT_EQ(0x4e3e5b9bd2800ea2ULL, seed.next());
	EXPECT_EQ(0xc0752ca2482a91f1ULL, seed.next());
	EXPECT_EQ(0x3f5fd1b17d136ae9ULL, seed.next());
	EXPECT_EQ(0xae06c714838dcd21ULL, seed.next());
	EXPECT_EQ(0x45e5977a2fc13093ULL, seed.next());
	EXPECT_EQ(0x12204cefa3234abbULL, seed.next());
	EXPECT_EQ(0x0f6254cd56eee447ULL, seed.next());
	EXPECT_EQ(0x6727c3095f6dcfc7ULL, seed.next());
	EXPECT_EQ(0x00f8af6ebadb4ec3ULL, seed.next());

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

	EXPECT_EQ(0, memcmp(buf, encrypt_text, text_len));

	seed.seed0 = 0xFACEDEADDEADFACEull;
	seed.seed1 = 0x1234567812345678ull;
	xorshift128((uint8_t*)buf, text_len, seed);

	ASSERT_STREQ(plain_text, buf);

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
		ASSERT_STREQ(plain_text, buf);
	}

}
