#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
uint32_t* _makeHead(uint16_t size, uint16_t id, uint32_t& head_for_check)
{
	head_for_check = ((uint32_t)(socket_api::ntoh_16(id)) << 16) | ((uint32_t)(socket_api::ntoh_16(size)));
	return &head_for_check;
}

//-------------------------------------------------------------------------------------
#define PACKET_CHECK(packet_size, packet_id, head_size, check_buf) \
	REQUIRE_EQ(0, memcmp(packet.get_memory_buf(), _makeHead((packet_size), (packet_id), head), sizeof(uint32_t))); \
	REQUIRE_EQ(((head_size) + (packet_size)), packet.get_memory_size()); \
	REQUIRE_EQ((packet_id), packet.get_packet_id()); \
	REQUIRE_EQ((packet_size), packet.get_packet_size()); \
	REQUIRE_EQ(0, memcmp(packet.get_packet_content(), (check_buf), (packet_size)));

//-------------------------------------------------------------------------------------
#define PACKET_CHECK_WITH_RESERVED(packet_size, packet_id, head_size, check_buf, reserved, resize) \
	PACKET_CHECK(packet_size, packet_id, head_size, check_buf) \
	REQUIRE_EQ(0, memcmp(packet.get_memory_buf()+sizeof(uint32_t), (reserved), (resize))); 

//-------------------------------------------------------------------------------------
#define PACKET_CHECK_ZERO() \
	REQUIRE_EQ(nullptr, packet.get_memory_buf()); \
	REQUIRE_EQ(0ull, packet.get_memory_size()); \
	REQUIRE_EQ(0u, packet.get_packet_id()); \
	REQUIRE_EQ(0u, packet.get_packet_size()); \
	REQUIRE_EQ(nullptr, packet.get_packet_content()); 

//-------------------------------------------------------------------------------------
TEST_CASE("Basic test for Packet", "[Packet]")
{
	PRINT_CURRENT_TEST_NAME();

	const size_t HEAD_SIZE = 8;
	const uint16_t PACKET_ID = 0x1234;
	const uint32_t RESERVED = 0xFACEC00Du;

	uint32_t head = 0;

	Packet packet;
	PACKET_CHECK_ZERO();

	packet.build_from_memory(HEAD_SIZE, PACKET_ID, 0, nullptr, 0, nullptr);
	REQUIRE_EQ(0, memcmp(packet.get_memory_buf(), _makeHead(0, PACKET_ID, head), sizeof(uint32_t)));
	REQUIRE_EQ(HEAD_SIZE, packet.get_memory_size());
	REQUIRE_EQ(PACKET_ID, packet.get_packet_id());
	REQUIRE_EQ(0, packet.get_packet_size());
	REQUIRE_EQ(nullptr, packet.get_packet_content());

	packet.clean();
	PACKET_CHECK_ZERO();

	const size_t buf_size = 1024;
	const size_t half_size = buf_size / 2;
	char temp_buf[buf_size] = { 0 };
	for (size_t i = 0; i < buf_size; i++) {
		((uint8_t*)temp_buf)[i] = (uint8_t)(rand() & 0xFF);
	}

	//build from small memory
	packet.build_from_memory(HEAD_SIZE, PACKET_ID, (uint16_t)half_size, temp_buf);
	PACKET_CHECK(half_size, PACKET_ID, HEAD_SIZE, temp_buf);

	packet.clean();
	PACKET_CHECK_ZERO();

	//build 1k memory buf
	packet.build_from_memory(HEAD_SIZE, PACKET_ID, (uint16_t)buf_size, temp_buf);
	PACKET_CHECK(buf_size, PACKET_ID, HEAD_SIZE, temp_buf);

	packet.clean();
	PACKET_CHECK_ZERO();

	//build as 2 parts
	packet.build_from_memory(HEAD_SIZE, PACKET_ID, (uint16_t)half_size, temp_buf, (uint16_t)half_size, temp_buf+ half_size);
	PACKET_CHECK(buf_size, PACKET_ID, HEAD_SIZE, temp_buf);

	packet.clean();
	PACKET_CHECK_ZERO();

	//build from ringbuf
	RingBuf rb;
	REQUIRE_FALSE(packet.build_from_ringbuf(HEAD_SIZE, rb));

	rb.memcpy_into(_makeHead(buf_size, PACKET_ID, head), sizeof(uint32_t));
	rb.memcpy_into(&RESERVED, sizeof(RESERVED));
	REQUIRE_FALSE(packet.build_from_ringbuf(HEAD_SIZE, rb));

	rb.memcpy_into(temp_buf, half_size);
	REQUIRE_FALSE(packet.build_from_ringbuf(HEAD_SIZE, rb));

	rb.memcpy_into(temp_buf+ half_size, half_size);
	REQUIRE_TRUE(packet.build_from_ringbuf(HEAD_SIZE, rb));
	PACKET_CHECK_WITH_RESERVED(buf_size, PACKET_ID, HEAD_SIZE, temp_buf, &RESERVED, sizeof(RESERVED));

	packet.clean();
	PACKET_CHECK_ZERO();

	//build from pipe
	Pipe pipe;
	REQUIRE_FALSE(packet.build_from_pipe(HEAD_SIZE, pipe));

	pipe.write((const char*)_makeHead(buf_size, PACKET_ID, head), sizeof(uint32_t));
	pipe.write((const char*)&RESERVED, sizeof(RESERVED));
	REQUIRE_FALSE(packet.build_from_pipe(HEAD_SIZE, pipe));
	PACKET_CHECK_ZERO();

	pipe.write((const char*)_makeHead(buf_size, PACKET_ID, head), sizeof(uint32_t));
	pipe.write((const char*)&RESERVED, sizeof(RESERVED));
	pipe.write(temp_buf, buf_size);
	REQUIRE_TRUE(packet.build_from_pipe(HEAD_SIZE, pipe));
	PACKET_CHECK_WITH_RESERVED(buf_size, PACKET_ID, HEAD_SIZE, temp_buf, &RESERVED, sizeof(RESERVED));

	//large memory
	const size_t max_size = 0xFFFF;
	const size_t half_max_size = max_size / 2;
	char* large_buf = (char*)CY_MALLOC(max_size);
	for (size_t i = 0; i < max_size; i++) {
		((uint8_t*)large_buf)[i] = (uint8_t)(rand() & 0xFF);
	}
	packet.build_from_memory(HEAD_SIZE, PACKET_ID, (uint16_t)max_size, large_buf);
	PACKET_CHECK(max_size, PACKET_ID, HEAD_SIZE, large_buf);

	//build as 2 parts
	packet.build_from_memory(HEAD_SIZE, PACKET_ID, (uint16_t)half_max_size, large_buf, (uint16_t)(max_size-half_max_size), large_buf + half_max_size);
	PACKET_CHECK(max_size, PACKET_ID, HEAD_SIZE, large_buf);

	CY_FREE(large_buf);
	large_buf = nullptr;

	packet.clean();
	PACKET_CHECK_ZERO();
}

}
