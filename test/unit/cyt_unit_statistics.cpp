#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <utility/cyu_statistics.h>

#include <gtest/gtest.h>

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST(Statistics, MinMaxValue)
{
	typedef MinMaxValue<int32_t> IntMinMaxValue;

	{
		IntMinMaxValue v;
		EXPECT_EQ(v.min(), std::numeric_limits<int32_t>::max());
		EXPECT_EQ(v.max(), std::numeric_limits<int32_t>::min());
	}

	{
		IntMinMaxValue v(123);
		EXPECT_EQ(v.min(), 123);
		EXPECT_EQ(v.max(), 123);
	}

	{
		IntMinMaxValue v;
		v.update(123);
		EXPECT_EQ(v.min(), 123);
		EXPECT_EQ(v.max(), 123);
	}

	{
		IntMinMaxValue v;
		v.update(1);
		v.update(2);
		EXPECT_EQ(v.min(), 1);
		EXPECT_EQ(v.max(), 2);
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

		EXPECT_EQ(v.max(), _max);
		EXPECT_EQ(v.min(), _min);
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

		EXPECT_EQ(v.max(), _max);
		EXPECT_EQ(v.min(), _min);
	}
}

