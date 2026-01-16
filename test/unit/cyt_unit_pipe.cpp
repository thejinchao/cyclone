#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST_CASE("Pipe basic test", "[Pipe][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	Pipe pipe;

	const char* plain_text = "Hello,World!";
	const size_t text_len = strlen(plain_text);

	const size_t readbuf_len = 1024;
	char read_buf[readbuf_len] = { 0 };

	ssize_t read_size = pipe.read(read_buf, readbuf_len);
	REQUIRE_TRUE(socket_api::is_lasterror_WOULDBLOCK());
	REQUIRE_EQ(SOCKET_ERROR, read_size);

	ssize_t write_size = pipe.write(plain_text, text_len);
	REQUIRE_EQ(text_len, (size_t)write_size);

	read_size = pipe.read(read_buf, readbuf_len);
	REQUIRE_EQ(text_len, (size_t)read_size);
	REQUIRE_STREQ(plain_text, read_buf);

	read_size = pipe.read(read_buf, readbuf_len);
	REQUIRE_TRUE(socket_api::is_lasterror_WOULDBLOCK());
	REQUIRE_EQ(SOCKET_ERROR, read_size);
}

//-------------------------------------------------------------------------------------
TEST_CASE("Pipe overflow test", "[Pipe][Overflow]")
{
	PRINT_CURRENT_TEST_NAME();

	Pipe pipe;

	XorShift128 rndPush, rndPop;
	rndPush.make();
	rndPop.seed0 = rndPush.seed0;
	rndPop.seed1 = rndPush.seed1;

	const size_t snd_block_size = 1024;
	assert(snd_block_size % sizeof(uint64_t) == 0);

	char snd_block[snd_block_size] = { 0 };
	size_t rnd_counts = snd_block_size / sizeof(uint64_t);

	size_t total_snd_size = 0;
	for (;;) {
		for (size_t i = 0; i < rnd_counts; i++) {
			uint64_t rnd = rndPush.next();
			((uint64_t*)snd_block)[i] = rnd;
		}

		ssize_t write_size = pipe.write(snd_block, snd_block_size);
		if (write_size <= 0) {
			REQUIRE_TRUE(socket_api::is_lasterror_WOULDBLOCK());
			break;
		}
		total_snd_size += (size_t)write_size;
	}

	size_t total_rcv_size = 0;
	for (;;) {
		uint64_t read_data;
		ssize_t read_size = pipe.read((char*)&read_data, sizeof(read_data));
		if (read_size <= 0) {
			REQUIRE_TRUE(socket_api::is_lasterror_WOULDBLOCK());
			break;
		}
		total_rcv_size += (size_t)read_size;
		REQUIRE_EQ(read_data, rndPop.next());
	}

	REQUIRE_EQ(total_snd_size, total_rcv_size);
}

//-------------------------------------------------------------------------------------
struct ThreadData
{
	Pipe pipe;
	XorShift128 rnd;
	size_t total_size;
};

//-------------------------------------------------------------------------------------
void _push_function(void* param)
{
	ThreadData* data = (ThreadData*)param;
	XorShift128 rnd = data->rnd;
	RingBuf sndBuf;

	size_t total_snd_size = 0;

	for (;;) {
		while (sndBuf.get_free_size() >= sizeof(uint64_t) && total_snd_size<(data->total_size)) {
			uint64_t next_snd = rnd.next();
			sndBuf.memcpy_into(&next_snd, sizeof(next_snd));
			total_snd_size += sizeof(next_snd);
		}

		if (sndBuf.empty()) break;

		ssize_t write_size = sndBuf.write_socket(data->pipe.get_write_port());
		if (write_size <= 0) {
			sys_api::thread_yield();
			continue;
		}
	}
	REQUIRE_EQ(total_snd_size, data->total_size);
}

//-------------------------------------------------------------------------------------
void _pop_function(void* param)
{
	ThreadData* data = (ThreadData*)param;
	XorShift128 rnd = data->rnd;
	RingBuf rcvBuf;

	size_t total_rcv_size = 0;
	uint64_t read_data;

	while (total_rcv_size<(data->total_size)) {
		ssize_t read_size = rcvBuf.read_socket(data->pipe.get_read_port(), false);
		while (rcvBuf.size() >= sizeof(uint64_t)) {
			REQUIRE_EQ(sizeof(uint64_t), rcvBuf.memcpy_out(&read_data, sizeof(uint64_t)));
			REQUIRE_EQ(rnd.next(), read_data);
			total_rcv_size += sizeof(uint64_t);
		}

		if (read_size <= 0) {
			sys_api::thread_yield();
		}
	}
	REQUIRE_TRUE(rcvBuf.empty());
	REQUIRE_EQ(total_rcv_size, data->total_size);
	REQUIRE_EQ(SOCKET_ERROR, (data->pipe).read((char*)&read_data, sizeof(read_data)));
	REQUIRE_TRUE(socket_api::is_lasterror_WOULDBLOCK());
	REQUIRE_EQ(RingBuf::kDefaultCapacity, rcvBuf.capacity());
}

//-------------------------------------------------------------------------------------
TEST_CASE("Pipe multi thread test", "[Pipe][MultiThread]")
{
	PRINT_CURRENT_TEST_NAME();

	ThreadData data;
	data.rnd.make();
	data.total_size = 1024 * 1024*10; //1MB

	thread_t pop_thread = sys_api::thread_create(_pop_function, &data, "pop");
	thread_t push_thread = sys_api::thread_create(_push_function, &data, "push");

	//join
	sys_api::thread_join(pop_thread);
	sys_api::thread_join(push_thread);
}
