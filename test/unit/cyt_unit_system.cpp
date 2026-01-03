#include <cy_core.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST_CASE("System basic test", "[System][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	REQUIRE_EQ(1ull, sizeof(int8_t));
	REQUIRE_EQ(1ull, sizeof(uint8_t));

	REQUIRE_EQ(2ull, sizeof(int16_t));
	REQUIRE_EQ(2ull, sizeof(uint16_t));

	REQUIRE_EQ(4ull, sizeof(int32_t));
	REQUIRE_EQ(4ull, sizeof(uint32_t));

	REQUIRE_EQ(8ull, sizeof(int64_t));
	REQUIRE_EQ(8ull, sizeof(uint64_t));

	REQUIRE_EQ(0x3412u, socket_api::ntoh_16(0x1234));
	REQUIRE_EQ(0x78563412u, socket_api::ntoh_32(0x12345678));
}

//-------------------------------------------------------------------------------------
TEST_CASE("System atomic test", "[System][Atomic]")
{
	PRINT_CURRENT_TEST_NAME();

	atomic_int32_t a(1);

	REQUIRE_TRUE(atomic_compare_exchange(a, 1, 2)); //1==1
	REQUIRE_EQ(a.load(), 2);

	REQUIRE_FALSE(atomic_compare_exchange(a, 1, 2)); //1!=2
	REQUIRE_EQ(a.load(), 2);

	REQUIRE_TRUE(atomic_smaller_exchange(a, 1, 3)); //1<2
	REQUIRE_EQ(a.load(), 3);

	REQUIRE_FALSE(atomic_smaller_exchange(a, 4, 10)); // 3 !< 4
	REQUIRE_EQ(a.load(), 3);

	REQUIRE_FALSE(atomic_smaller_exchange(a, 3, 10)); // 3 !< 3
	REQUIRE_EQ(a.load(), 3);

	REQUIRE_TRUE(atomic_greater_exchange(a, 4, 4)); // 4>3
	REQUIRE_EQ(a.load(), 4);

	REQUIRE_FALSE(atomic_greater_exchange(a, 3, 10)); // 3 !> 4
	REQUIRE_EQ(a.load(), 4);

	REQUIRE_FALSE(atomic_greater_exchange(a, 4, 10)); // 4 !> 4
	REQUIRE_EQ(a.load(), 4);
}

