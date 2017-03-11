#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <gtest/gtest.h>

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
	EXPECT_EQ(0, memcmp(packet.get_memory_buf(), _makeHead((packet_size), (packet_id), head), sizeof(uint32_t))); \
	EXPECT_EQ(((head_size) + (packet_size)), packet.get_memory_size()); \
	EXPECT_EQ((packet_id), packet.get_packet_id()); \
	EXPECT_EQ((packet_size), packet.get_packet_size()); \
	EXPECT_EQ(0, memcmp(packet.get_packet_content(), (check_buf), (packet_size)));

//-------------------------------------------------------------------------------------
#define PACKET_CHECK_WITH_RESERVED(packet_size, packet_id, head_size, check_buf, reserved, resize) \
	PACKET_CHECK(packet_size, packet_id, head_size, check_buf) \
	EXPECT_EQ(0, memcmp(packet.get_memory_buf()+sizeof(uint32_t), (reserved), (resize))); 

//-------------------------------------------------------------------------------------
#define PACKET_CHECK_ZERO() \
	EXPECT_EQ(nullptr, packet.get_memory_buf()); \
	EXPECT_EQ(0ull, packet.get_memory_size()); \
	EXPECT_EQ(0u, packet.get_packet_id()); \
	EXPECT_EQ(0u, packet.get_packet_size()); \
	EXPECT_EQ(nullptr, packet.get_packet_content()); 

//-------------------------------------------------------------------------------------
TEST(Packet, Basic)
{
	const size_t HEAD_SIZE = 8;
	const uint16_t PACKET_ID = 0x1234;
	const uint32_t RESERVED = 0xFACEC00Du;

	uint32_t head = 0;

	Packet packet;
	PACKET_CHECK_ZERO();

	packet.build(HEAD_SIZE, PACKET_ID, 0, 0);
	EXPECT_EQ(0, memcmp(packet.get_memory_buf(), _makeHead(0, PACKET_ID, head), sizeof(uint32_t)));
	EXPECT_EQ(HEAD_SIZE, packet.get_memory_size());
	EXPECT_EQ(PACKET_ID, packet.get_packet_id());
	EXPECT_EQ(0, packet.get_packet_size());
	EXPECT_EQ(nullptr, packet.get_packet_content());

	packet.clean();
	PACKET_CHECK_ZERO();

	const size_t buf_size = 1024;
	const size_t half_size = buf_size / 2;
	char temp_buf[buf_size] = { 0 };
	for (size_t i = 0; i < buf_size; i++) {
		((uint8_t*)temp_buf)[i] = (uint8_t)(rand() & 0xFF);
	}

	//build from small memory
	packet.build(HEAD_SIZE, PACKET_ID, (uint16_t)half_size, temp_buf);
	PACKET_CHECK(half_size, PACKET_ID, HEAD_SIZE, temp_buf);

	packet.clean();
	PACKET_CHECK_ZERO();

	//build 1k memory buf
	packet.build(HEAD_SIZE, PACKET_ID, (uint16_t)buf_size, temp_buf);
	PACKET_CHECK(buf_size, PACKET_ID, HEAD_SIZE, temp_buf);

	packet.clean();
	PACKET_CHECK_ZERO();

	//build from ringbuf
	RingBuf rb;
	EXPECT_FALSE(packet.build(HEAD_SIZE, rb));

	rb.memcpy_into(_makeHead(buf_size, PACKET_ID, head), sizeof(uint32_t));
	rb.memcpy_into(&RESERVED, sizeof(RESERVED));
	EXPECT_FALSE(packet.build(HEAD_SIZE, rb));

	rb.memcpy_into(temp_buf, half_size);
	EXPECT_FALSE(packet.build(HEAD_SIZE, rb));

	rb.memcpy_into(temp_buf+ half_size, half_size);
	EXPECT_TRUE(packet.build(HEAD_SIZE, rb));
	PACKET_CHECK_WITH_RESERVED(buf_size, PACKET_ID, HEAD_SIZE, temp_buf, &RESERVED, sizeof(RESERVED));

	packet.clean();
	PACKET_CHECK_ZERO();

	//build from pipe
	Pipe pipe;
	EXPECT_FALSE(packet.build(HEAD_SIZE, pipe));

	pipe.write((const char*)_makeHead(buf_size, PACKET_ID, head), sizeof(uint32_t));
	pipe.write((const char*)&RESERVED, sizeof(RESERVED));
	EXPECT_FALSE(packet.build(HEAD_SIZE, pipe));
	PACKET_CHECK_ZERO();

	pipe.write((const char*)_makeHead(buf_size, PACKET_ID, head), sizeof(uint32_t));
	pipe.write((const char*)&RESERVED, sizeof(RESERVED));
	pipe.write(temp_buf, buf_size);
	EXPECT_TRUE(packet.build(HEAD_SIZE, pipe));
	PACKET_CHECK_WITH_RESERVED(buf_size, PACKET_ID, HEAD_SIZE, temp_buf, &RESERVED, sizeof(RESERVED));

	packet.clean();
	PACKET_CHECK_ZERO();
}

}
