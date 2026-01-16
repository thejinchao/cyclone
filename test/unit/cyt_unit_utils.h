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

struct AutoPrintTestCaseName
{
	std::string printTime(int64_t microseconds)
	{
		char buffer[64];
		if (microseconds < 1000ll) {
			snprintf(buffer, 64, "%" PRId64 " us", microseconds);
		}
		else if (microseconds < 1000ll * 1000ll) {
			snprintf(buffer, 64, "%.2f ms", static_cast<double>(microseconds) / 1000.0);
		}
		else if (microseconds < 60ll * 1000ll * 1000ll) {
			snprintf(buffer, 64, "%.2f s", static_cast<double>(microseconds) / (1000.0 * 1000.0));
		}
		else {
			snprintf(buffer, 64, "%.2f min", static_cast<double>(microseconds) / (60.0 * 1000.0 * 1000.0));
		}
		return std::string(buffer);
	}
	AutoPrintTestCaseName()
	{
		printf("[ Begin      ]%s\n",
			Catch::getCurrentContext().getResultCapture()->getCurrentTestName().c_str()
		);
		begin_time = cyclone::sys_api::performance_time_now();
	}
	~AutoPrintTestCaseName()
	{
		int64_t duration = cyclone::sys_api::performance_time_now() - begin_time;
		printf("[       Done ]%s(%s)\n",
			Catch::getCurrentContext().getResultCapture()->getCurrentTestName().c_str(),
			printTime(duration).c_str()
		);
	}
	int64_t begin_time = 0;
};
#define PRINT_CURRENT_TEST_NAME()	AutoPrintTestCaseName autoPrintTestCaseName;
