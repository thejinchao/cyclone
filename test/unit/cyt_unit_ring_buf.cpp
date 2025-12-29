#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
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
#define CHECK_RINGBUF_EMPTY(rb, c) \
	REQUIRE_EQ(0ul, rb.size()); \
	REQUIRE_EQ((size_t)(c), rb.capacity()); \
	REQUIRE_EQ((size_t)(c), rb.get_free_size()); \
	REQUIRE_TRUE(rb.empty()); \
	REQUIRE_FALSE(rb.full());

//-------------------------------------------------------------------------------------
#define CHECK_RINGBUF_SIZE(rb, s, c) \
	REQUIRE_EQ((size_t)(s), rb.size()); \
	REQUIRE_EQ((size_t)(c), rb.capacity()); \
	REQUIRE_EQ((size_t)((c) - (s)), rb.get_free_size()); \
	if ((s) == 0) \
		REQUIRE_TRUE(rb.empty()); \
	else \
		REQUIRE_FALSE(rb.empty()); \
	if ((size_t)(s) == rb.capacity()) \
		REQUIRE_TRUE(rb.full()); \
	else \
		REQUIRE_FALSE(rb.full()); 

//-------------------------------------------------------------------------------------
TEST_CASE("RingBuf basic test", "[RingBuf][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	const char* text_pattern = "Hello,World!";
	const size_t text_length = strlen(text_pattern);

	const size_t buffer_size = RingBuf::kDefaultCapacity * 4;
	const uint8_t* buffer1;
	uint8_t* buffer2;
	{
		uint8_t* _buffer1 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		buffer2 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		_fillRandom(_buffer1, buffer_size);
		_fillRandom(buffer2, buffer_size);
		buffer1 = _buffer1;
	}

	// Initial conditions
	{
		RingBuf rb;
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	// Different sizes 
	{
		RingBuf rb(24);
		CHECK_RINGBUF_EMPTY(rb, 24);
	}

	// memcpy_into with zero count
	{
		RingBuf rb;
		rb.memcpy_into(text_pattern, 0);
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	// memcpy_into a few bytes of data AND reset
	{
		RingBuf rb;
		rb.memcpy_into(text_pattern, text_length);
		CHECK_RINGBUF_SIZE(rb, text_length, RingBuf::kDefaultCapacity);

		rb.reset();

		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	// memcpy_into a few bytes twice
	{
		RingBuf rb;
		rb.memcpy_into(text_pattern, text_length);
		rb.memcpy_into(text_pattern, text_length);

		CHECK_RINGBUF_SIZE(rb, text_length * 2, RingBuf::kDefaultCapacity);
	}

	//memcpy_into full capacity AND reset
	{
		RingBuf rb;
		rb.memcpy_into(buffer1, RingBuf::kDefaultCapacity);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity, RingBuf::kDefaultCapacity);

		rb.reset();

		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	//memcpy_into twice to full capacity
	{
		RingBuf rb;
		rb.memcpy_into(buffer1, RingBuf::kDefaultCapacity-2);
		rb.memcpy_into(buffer1 +(RingBuf::kDefaultCapacity - 2), 2);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity, RingBuf::kDefaultCapacity);
	}

	//memcpy_into, overflow by 1 byte
	{
		RingBuf rb;
		rb.memcpy_into(buffer1, RingBuf::kDefaultCapacity + 1);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity + 1, (RingBuf::kDefaultCapacity + 1) * 2 - 1);
	}

	//memcpy_into twice, overflow by 1 byte on second copy
	{
		RingBuf rb;
		rb.memcpy_into(buffer1, 1);
		rb.memcpy_into(buffer1+1, RingBuf::kDefaultCapacity);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity+1, (RingBuf::kDefaultCapacity + 1) * 2 - 1);
	}

	//memcpy_out with zero count
	{
		RingBuf rb;
		rb.memcpy_into(text_pattern, text_length);
		REQUIRE_EQ(0ul, rb.memcpy_out(0, 0));

		CHECK_RINGBUF_SIZE(rb, text_length, RingBuf::kDefaultCapacity);
	}

	//memcpy_out a few bytes of data
	{
		RingBuf rb;
		rb.memcpy_into(text_pattern, text_length);

		const size_t READ_SIZE = 8;

		REQUIRE_EQ(READ_SIZE, rb.memcpy_out(buffer2, READ_SIZE));

		CHECK_RINGBUF_SIZE(rb, text_length - READ_SIZE, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(0, memcmp(buffer2, text_pattern, READ_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//memcpy_out all data
	{
		RingBuf rb;
		rb.memcpy_into(text_pattern, text_length);

		const size_t READ_SIZE = text_length;

		REQUIRE_EQ(READ_SIZE, rb.memcpy_out(buffer2, READ_SIZE));
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(0, memcmp(buffer2, text_pattern, text_length));
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and memcpy_out
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE*3 < RingBuf::kDefaultCapacity);

		RingBuf rb;
		rb.memcpy_into(buffer1, RingBuf::kDefaultCapacity- TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb.memcpy_into(buffer1 + RingBuf::kDefaultCapacity- TEST_WRAP_SIZE, TEST_WRAP_SIZE*2);

		REQUIRE_EQ(0, memcmp(buffer2, buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));

		CHECK_RINGBUF_SIZE(rb, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(TEST_WRAP_SIZE * 3, rb.memcpy_out(buffer2, TEST_WRAP_SIZE * 3));
		REQUIRE_EQ(0, memcmp(buffer2, buffer1+ RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE * 3));

		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and reset
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 3 < RingBuf::kDefaultCapacity);

		RingBuf rb;
		rb.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 2);

		rb.reset();
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and overflow
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE*3 < RingBuf::kDefaultCapacity);

		RingBuf rb;
		rb.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 2);

		//overflow
		rb.memcpy_into(buffer1 + RingBuf::kDefaultCapacity + TEST_WRAP_SIZE, RingBuf::kDefaultCapacity);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity + TEST_WRAP_SIZE * 3, (RingBuf::kDefaultCapacity + 1) * 2 - 1);

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(RingBuf::kDefaultCapacity + TEST_WRAP_SIZE * 3, rb.memcpy_out(buffer2, rb.size()));
		REQUIRE_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.size()));
	}

	_fillRandom(buffer2, buffer_size);

	//moveto
	{
		const size_t COPY_SIZE = 8;

		RingBuf rb1, rb2;
		rb1.memcpy_into(text_pattern, text_length);
		REQUIRE_EQ(COPY_SIZE, rb1.moveto(rb2, COPY_SIZE));

		CHECK_RINGBUF_SIZE(rb1, text_length- COPY_SIZE, RingBuf::kDefaultCapacity);
		CHECK_RINGBUF_SIZE(rb2, COPY_SIZE, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);

		REQUIRE_EQ(COPY_SIZE, rb2.memcpy_out(buffer2, COPY_SIZE));
		REQUIRE_EQ(0, memcmp(buffer2, text_pattern, COPY_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and moveto
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);
		const size_t COPY_SIZE = TEST_WRAP_SIZE*3;

		RingBuf rb1, rb2;
		rb1.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		REQUIRE_EQ(COPY_SIZE, rb1.moveto(rb2, COPY_SIZE));

		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE, RingBuf::kDefaultCapacity);
		CHECK_RINGBUF_SIZE(rb2, COPY_SIZE, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);

		REQUIRE_EQ(COPY_SIZE, rb2.memcpy_out(buffer2, COPY_SIZE));
		REQUIRE_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb2.size()));
	}

	_fillRandom(buffer2, buffer_size);

	//peek
	{
		const size_t PEEK_SIZE = 8;

		RingBuf rb1;
		rb1.memcpy_into(text_pattern, text_length);

		REQUIRE_EQ(PEEK_SIZE, rb1.peek(0, buffer2, PEEK_SIZE));
		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(0, memcmp(buffer2, text_pattern, PEEK_SIZE));

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(PEEK_SIZE, rb1.peek(1, buffer2, PEEK_SIZE));

		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(0, memcmp(buffer2, text_pattern+1, PEEK_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and peek
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(TEST_WRAP_SIZE, rb1.peek(0, buffer2, TEST_WRAP_SIZE));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(0, memcmp(buffer2, buffer1+ RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE));

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(TEST_WRAP_SIZE * 2, rb1.peek(TEST_WRAP_SIZE, buffer2, TEST_WRAP_SIZE*2));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE*2));

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(TEST_WRAP_SIZE, rb1.peek(TEST_WRAP_SIZE*3, buffer2, TEST_WRAP_SIZE));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity + TEST_WRAP_SIZE, TEST_WRAP_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//discard
	{
		const size_t DISCARD_SIZE = 8;

		RingBuf rb1;
		rb1.memcpy_into(text_pattern, text_length);

		REQUIRE_EQ(DISCARD_SIZE, rb1.discard(DISCARD_SIZE));
		CHECK_RINGBUF_SIZE(rb1, text_length-DISCARD_SIZE, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(text_length - DISCARD_SIZE, rb1.memcpy_out(buffer2, text_length));
		REQUIRE_EQ(0, memcmp(buffer2, text_pattern+ DISCARD_SIZE, text_length - DISCARD_SIZE));

		CHECK_RINGBUF_EMPTY(rb1, RingBuf::kDefaultCapacity);
		rb1.memcpy_into(text_pattern, text_length);
		REQUIRE_EQ(text_length, rb1.discard(RingBuf::kDefaultCapacity));
		CHECK_RINGBUF_EMPTY(rb1, RingBuf::kDefaultCapacity);
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and discard
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		REQUIRE_EQ(TEST_WRAP_SIZE * 3, rb1.discard(TEST_WRAP_SIZE * 3));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);
		REQUIRE_EQ(TEST_WRAP_SIZE, rb1.memcpy_out(buffer2, TEST_WRAP_SIZE));
		REQUIRE_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity + TEST_WRAP_SIZE, TEST_WRAP_SIZE));
	}

	//checksum
	{
		RingBuf rb1;
		rb1.memcpy_into(text_pattern, text_length);

		REQUIRE_EQ(INITIAL_ADLER, rb1.checksum(text_length, 0));
		REQUIRE_EQ(INITIAL_ADLER, rb1.checksum(text_length, 1));
		REQUIRE_EQ(INITIAL_ADLER, rb1.checksum(0, text_length+1));
		REQUIRE_EQ(INITIAL_ADLER, rb1.checksum(0,0));

		REQUIRE_EQ(0x1c9d044aul, rb1.checksum(0, text_length));
		REQUIRE_EQ(0x0d0c02e7ul, rb1.checksum(0, 8));
		REQUIRE_EQ(0x0ddc0311ul, rb1.checksum(1, 8));

		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);
	}

	//make wrap condition and checksum
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpy_out(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		REQUIRE_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE), rb1.checksum(0, TEST_WRAP_SIZE));
		REQUIRE_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE*3), rb1.checksum(0, TEST_WRAP_SIZE*3));
		REQUIRE_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity, TEST_WRAP_SIZE), rb1.checksum(TEST_WRAP_SIZE*2, TEST_WRAP_SIZE));
		REQUIRE_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity+ TEST_WRAP_SIZE, TEST_WRAP_SIZE), rb1.checksum(TEST_WRAP_SIZE * 3, TEST_WRAP_SIZE));
	}

	//normalize
	{
		RingBuf rb1;
		rb1.memcpy_into(text_pattern, text_length);

		REQUIRE_EQ(0, memcmp(text_pattern, rb1.normalize(), text_length));
		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);

		rb1.discard(1);
		REQUIRE_EQ(0, memcmp(text_pattern+1, rb1.normalize(), text_length-1));
		CHECK_RINGBUF_SIZE(rb1, text_length-1, RingBuf::kDefaultCapacity);
	}

	//make wrap condition and normalize
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 2);
		REQUIRE_EQ(0, memcmp(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE*2, rb1.normalize(), TEST_WRAP_SIZE*3));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);

		rb1.reset();
		rb1.memcpy_into(buffer1, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, rb1.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE));
		rb1.memcpy_into(buffer1 + RingBuf::kDefaultCapacity, TEST_WRAP_SIZE * 2);
		REQUIRE_EQ(0, memcmp(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, rb1.normalize(), TEST_WRAP_SIZE * 3));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);
	}

	//search
	{
		const size_t BUF1_SIZE = 32;
		char temp_buffer1[BUF1_SIZE] = { 0 };

		const ssize_t A_POS = 10;
		temp_buffer1[A_POS] = 'A';

		RingBuf rb1;
		REQUIRE_LT(rb1.search(0, 'A'), 0);

		rb1.memcpy_into(temp_buffer1, BUF1_SIZE);
		REQUIRE_EQ(rb1.search(0, 'A'), A_POS);

		for (size_t i = 0; i <= A_POS; i++) {
			REQUIRE_EQ(rb1.search(i, 'A'), A_POS);
		}

		for (size_t i = A_POS + 1; i <= rb1.capacity(); i++) {
			REQUIRE_LT(rb1.search(i, 'A'), 0);
		}

		const ssize_t DISCARD_SIZE = 3;
		rb1.discard(DISCARD_SIZE);

		for (size_t i = 0; i <= A_POS- DISCARD_SIZE; i++) {
			REQUIRE_EQ(rb1.search(i, 'A'), A_POS- DISCARD_SIZE);
		}

		for (size_t i = A_POS- DISCARD_SIZE + 1; i <= rb1.capacity(); i++) {
			REQUIRE_LT(rb1.search(i, 'A'), 0);
		}
	}

	//make wrap condition and search
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		//zero memory
		memset(buffer2, 0, buffer_size);

		RingBuf rb1;
		rb1.memcpy_into(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		REQUIRE_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));

		const ssize_t A_POS = 40;
		const ssize_t B_POS = 80;
		
		buffer2[A_POS - TEST_WRAP_SIZE] = 'A';
		buffer2[B_POS - TEST_WRAP_SIZE] = 'B';

		rb1.memcpy_into(buffer2, TEST_WRAP_SIZE * 2);
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);

		for (size_t i = 0; i < rb1.capacity(); i++) {
			if(i<=A_POS)
				REQUIRE_EQ(rb1.search(i, 'A'), A_POS);
			else
				REQUIRE_LT(rb1.search(i, 'A'), 0);

			if (i <= B_POS)
				REQUIRE_EQ(rb1.search(i, 'B'), B_POS);
			else
				REQUIRE_LT(rb1.search(i, 'B'), 0);
		}

	}
}


//-------------------------------------------------------------------------------------
TEST_CASE("RingBuf socket test", "[RingBuf][Socket]")
{
	PRINT_CURRENT_TEST_NAME();

	const char* text_pattern = "Hello,World!";
	const size_t text_length = strlen(text_pattern);

	const size_t buffer_size = RingBuf::kDefaultCapacity * 4;
	const uint8_t* buffer1;
	uint8_t* buffer2;
	{
		uint8_t* _buffer1 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		buffer2 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		_fillRandom(_buffer1, buffer_size);
		_fillRandom(buffer2, buffer_size);
		buffer1 = _buffer1;
	}

	//write_socket/read_socket
	{
		RingBuf rb_snd;
		Pipe pipe;

		//send few bytes
		rb_snd.memcpy_into(text_pattern, text_length);
		REQUIRE_EQ(text_length, (size_t)rb_snd.write_socket(pipe.get_write_port()));
		CHECK_RINGBUF_EMPTY(rb_snd, RingBuf::kDefaultCapacity);

		RingBuf rb_rcv;
		REQUIRE_EQ(text_length, (size_t)rb_rcv.read_socket(pipe.get_read_port()));
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);
		REQUIRE_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_EMPTY(rb_rcv, RingBuf::kDefaultCapacity);

		uint32_t read_buf;
		REQUIRE_EQ(SOCKET_ERROR, pipe.read((char*)&read_buf, sizeof(read_buf)));
		REQUIRE_TRUE(socket_api::is_lasterror_WOULDBLOCK());

		//send and receive again
		rb_snd.memcpy_into(text_pattern, text_length);
		rb_snd.memcpy_into(text_pattern, text_length);
		rb_snd.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_snd, text_length, RingBuf::kDefaultCapacity);

		rb_rcv.memcpy_into(text_pattern, text_length);
		rb_rcv.memcpy_into(text_pattern, text_length);
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(text_length, (size_t)rb_snd.write_socket(pipe.get_write_port()));
		REQUIRE_EQ(text_length, (size_t)rb_rcv.read_socket(pipe.get_read_port()));
		CHECK_RINGBUF_EMPTY(rb_snd, RingBuf::kDefaultCapacity);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length*2, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		REQUIRE_EQ(0, memcmp(rb_rcv.normalize()+text_length, text_pattern, text_length));
	}

	//read_socket to full
	{
		Pipe pipe;

		RingBuf rb_rcv;
		rb_rcv.memcpy_into(text_pattern, text_length);
		rb_rcv.memcpy_into(text_pattern, text_length);
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(RingBuf::kDefaultCapacity, pipe.write((const char*)buffer1, RingBuf::kDefaultCapacity));

		REQUIRE_EQ(RingBuf::kDefaultCapacity-text_length, (size_t)rb_rcv.read_socket(pipe.get_read_port(), false));
		CHECK_RINGBUF_SIZE(rb_rcv, RingBuf::kDefaultCapacity, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		REQUIRE_EQ(0, memcmp(rb_rcv.normalize() + text_length, buffer1, RingBuf::kDefaultCapacity-text_length));
	}

	//read_socket and expand
	{
		Pipe pipe;

		RingBuf rb_rcv;
		rb_rcv.memcpy_into(text_pattern, text_length);
		rb_rcv.memcpy_into(text_pattern, text_length);
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(RingBuf::kDefaultCapacity, pipe.write((const char*)buffer1, RingBuf::kDefaultCapacity));

		REQUIRE_EQ(RingBuf::kDefaultCapacity, rb_rcv.read_socket(pipe.get_read_port()));
		CHECK_RINGBUF_SIZE(rb_rcv, text_length + RingBuf::kDefaultCapacity, (RingBuf::kDefaultCapacity + 1) * 2 - 1);

		REQUIRE_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		REQUIRE_EQ(0, memcmp(rb_rcv.normalize() + text_length, buffer1, RingBuf::kDefaultCapacity));
	}

	//make wrap condition and write_socket
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 3 < RingBuf::kDefaultCapacity);

		RingBuf rb_snd;
		rb_snd.memcpy_into(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		rb_snd.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2);
		rb_snd.memcpy_into(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);
		CHECK_RINGBUF_SIZE(rb_snd, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);

		Pipe pipe;
		REQUIRE_EQ(TEST_WRAP_SIZE * 4, (size_t)rb_snd.write_socket(pipe.get_write_port()));
		CHECK_RINGBUF_EMPTY(rb_snd, RingBuf::kDefaultCapacity);

		RingBuf rb_rcv;
		REQUIRE_EQ(TEST_WRAP_SIZE * 4, (size_t)rb_rcv.read_socket(pipe.get_read_port()));
		CHECK_RINGBUF_SIZE(rb_rcv, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);

		REQUIRE_EQ(0, memcmp(rb_rcv.normalize(), buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE * 4));
	}
}

}
