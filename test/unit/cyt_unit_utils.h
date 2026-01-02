#include "catch2/catch.hpp"

#define REQUIRE_EQ(a, b)			REQUIRE((a) == (b))
#define REQUIRE_NE(a, b)			REQUIRE((a) != (b))
#define REQUIRE_LT(a, b)			REQUIRE((a) < (b))
#define REQUIRE_GT(a, b)			REQUIRE((a) > (b))
#define REQUIRE_LE(a, b)			REQUIRE((a) <= (b))
#define REQUIRE_GE(a, b)			REQUIRE((a) >= (b))
#define REQUIRE_RANGE(a, min, max)	REQUIRE((a) >= (min)); REQUIRE((a)<=(max));
#define REQUIRE_TRUE(a)				REQUIRE((a))
#define REQUIRE_STREQ(a, b)			REQUIRE_THAT((a), Catch::Matchers::Equals(b))
#define PRINT_CURRENT_TEST_NAME()	printf("%s\n", Catch::getCurrentContext().getResultCapture()->getCurrentTestName().c_str())

