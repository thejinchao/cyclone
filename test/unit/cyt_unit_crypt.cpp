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
	adler = adler32(adler, hello, strlen(hello));
	EXPECT_EQ(0x1c9d044aul, adler);

	const char* force = "May the Force be with you";
	adler = adler32(0, 0, 0);
	adler = adler32(adler, force, strlen(force));
	EXPECT_EQ(0x6fe408d8ul, adler);
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
