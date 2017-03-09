#include <cy_core.h>
#include <gtest/gtest.h>

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
TEST(System, Basic)
{
	EXPECT_EQ(1ull, sizeof(int8_t));
	EXPECT_EQ(1ull, sizeof(uint8_t));

	EXPECT_EQ(2ull, sizeof(int16_t));
	EXPECT_EQ(2ull, sizeof(uint16_t));

	EXPECT_EQ(4ull, sizeof(int32_t));
	EXPECT_EQ(4ull, sizeof(uint32_t));

	EXPECT_EQ(8ull, sizeof(int64_t));
	EXPECT_EQ(8ull, sizeof(uint64_t));

	EXPECT_EQ(0x3412u, socket_api::ntoh_16(0x1234));
	EXPECT_EQ(0x78563412u, socket_api::ntoh_32(0x12345678));
}

}
