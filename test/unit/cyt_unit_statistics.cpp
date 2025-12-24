#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <utility/cyu_statistics.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST_CASE("MinMaxValue test for Signal", "[Statistics]")
{
	typedef MinMaxValue<int32_t> IntMinMaxValue;

	{
		IntMinMaxValue v;
		REQUIRE_EQ(v.min(), std::numeric_limits<int32_t>::max());
		REQUIRE_EQ(v.max(), std::numeric_limits<int32_t>::min());
	}

	{
		IntMinMaxValue v(123);
		REQUIRE_EQ(v.min(), 123);
		REQUIRE_EQ(v.max(), 123);
	}

	{
		IntMinMaxValue v;
		v.update(123);
		REQUIRE_EQ(v.min(), 123);
		REQUIRE_EQ(v.max(), 123);
	}

	{
		IntMinMaxValue v;
		v.update(1);
		v.update(2);
		REQUIRE_EQ(v.min(), 1);
		REQUIRE_EQ(v.max(), 2);
	}

	{
		IntMinMaxValue v;
		int32_t firstValue = rand()-RAND_MAX/2;
		int32_t _min = firstValue;
		int32_t _max = firstValue;
		v.update(firstValue);

		for (int32_t i = 0; i < 256; i++) {
			int32_t randomValue = rand() - RAND_MAX / 2;
			if (randomValue > _max) _max = randomValue;
			if (randomValue < _min) _min = randomValue;
			v.update(randomValue);
		}

		REQUIRE_EQ(v.max(), _max);
		REQUIRE_EQ(v.min(), _min);
	}

	//multi thread
	{
		int32_t firstValue = rand() - RAND_MAX / 2;
		int32_t _min = firstValue;
		int32_t _max = firstValue;

		IntMinMaxValue v(firstValue);

		struct ThreadContext
		{
			int32_t _min;
			int32_t _max;
			thread_t _thread;
		};

		int32_t threadCounts = sys_api::get_cpu_counts();
		if (threadCounts < 4) threadCounts = 4;

		ThreadContext** test_threads = new ThreadContext*[threadCounts];

		for (int32_t i = 0; i < threadCounts; i++) {

			ThreadContext* ctx = new ThreadContext;
			ctx->_min = ctx->_max = firstValue;
			ctx->_thread = sys_api::thread_create([&v](void* param) {
				
				ThreadContext* _ctx = (ThreadContext*)param;

				for (int32_t j = 0; j < 0xFFFF; j++) {
					int32_t value = rand() - RAND_MAX / 2;

					if (value > _ctx->_max) _ctx->_max = value;
					if (value < _ctx->_min) _ctx->_min = value;
					v.update(value);
				}
			}, ctx, "");

			test_threads[i] = ctx;
		}

		for (int32_t i = 0; i < threadCounts; i++) {
			sys_api::thread_join(test_threads[i]->_thread);

			if (test_threads[i]->_max > _max) _max = test_threads[i]->_max;
			if (test_threads[i]->_min < _min) _min = test_threads[i]->_min;
		}

		REQUIRE_EQ(v.max(), _max);
		REQUIRE_EQ(v.min(), _min);
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("PeriodValue test for Signal", "[Statistics]")
{
	typedef PeriodValue<int32_t, true> IntPeriodValue;

	{
		IntPeriodValue v;

		REQUIRE_EQ(v.total_counts(), 0);
		REQUIRE_EQ(v.sum_and_counts(), std::make_pair(0, 0));
	}

	{
		IntPeriodValue v(4000);

		// 0-100, 1-200, 2-300, 3-400, ... ,31-3200
		for (int32_t i = 0; i < 32; i++) {
			v.push(i, (int64_t)(i + 1) * 100ll);
		}

		//all
		REQUIRE_EQ(v.total_counts(), 32);
		REQUIRE_EQ(v.sum_and_counts(3300), std::make_pair(496, 32)); // 496 = (0+31)*32/2
	}

	{
		IntPeriodValue v(1000);

		// 0-100, 1-200, 2-300, 3-400, ... ,39-4000
		for (int32_t i = 0; i < 40; i++) {
			v.push(i, (int64_t)(i + 1) * 100ll);
		}

		REQUIRE_EQ(v.total_counts(), IntPeriodValue::ValueQueue::kDefaultCapacity-1);

		//30-3100, 31-3200, ..., 39-4000
		REQUIRE_EQ(v.sum_and_counts(4100), std::make_pair(345, 10)); //345 = (30 + 39) * 10 / 2
		REQUIRE_EQ(v.total_counts(), 10);

		//39-4000
		REQUIRE_EQ(v.sum_and_counts(5000), std::make_pair(39, 1));
		REQUIRE_EQ(v.total_counts(), 1);

		//all expired
		REQUIRE_EQ(v.sum_and_counts(5001), std::make_pair(0, 0));
		REQUIRE_EQ(v.total_counts(), 0);
	}
}
