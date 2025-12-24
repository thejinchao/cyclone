#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <utility/cyu_ring_queue.h>

#include "cyt_unit_utils.h"

using namespace cyclone;

//-------------------------------------------------------------------------------------
#define CHECK_RINQUEUE_EMPTY(rq, c) \
	REQUIRE_EQ(0ul, rq.size()); \
	REQUIRE_EQ((size_t)(c), rq.capacity()); \
	REQUIRE_EQ((size_t)(c), rq.get_free_size()); \
	REQUIRE_TRUE(rq.empty()); 

//-------------------------------------------------------------------------------------
#define CHECK_RINGQUEUE_SIZE(rq, s, c) \
	REQUIRE_EQ((size_t)(s), rq.size()); \
	REQUIRE_EQ((size_t)(c), rq.capacity()); \
	REQUIRE_EQ((size_t)((c) - (s)), rq.get_free_size()); \
	if ((s) == 0) \
		REQUIRE_TRUE(rq.empty()); \
	else \
		REQUIRE_FALSE(rq.empty()); \

//-------------------------------------------------------------------------------------
TEST_CASE("FixedCapcity test for RingQueue", "[RingQueue]")
{
	const size_t TestSize = 31;
	typedef RingQueue<int32_t> IntRingQueue;

	// Initial conditions
	{
		IntRingQueue rq30(30);
		CHECK_RINQUEUE_EMPTY(rq30, 30);

		IntRingQueue rq31(31);
		CHECK_RINQUEUE_EMPTY(rq31, 31);

		IntRingQueue rq32(32);
		CHECK_RINQUEUE_EMPTY(rq32, 32);

		IntRingQueue rq33(33);
		CHECK_RINQUEUE_EMPTY(rq33, 33);
	}

	// push one element AND reset
	{
		IntRingQueue rq(TestSize);
		rq.push(1);
		CHECK_RINGQUEUE_SIZE(rq, 1, TestSize);

		REQUIRE_EQ(rq.front(), 1);

		rq.reset();
		CHECK_RINQUEUE_EMPTY(rq, TestSize);
	}

	// push element twice
	{
		IntRingQueue rq(TestSize);
		rq.push(1);
		rq.push(2);

		REQUIRE_EQ(rq.front(), 1);
		CHECK_RINGQUEUE_SIZE(rq, 2, TestSize);
	}

	//push element to full capacity AND reset
	{
		IntRingQueue rq(TestSize);
		for (int32_t i = 0; i < (int32_t)TestSize; i++) {
			rq.push(i);
		}
		CHECK_RINGQUEUE_SIZE(rq, TestSize, TestSize);
		REQUIRE_EQ(rq.front(), 0);

		rq.reset();
		CHECK_RINQUEUE_EMPTY(rq, TestSize);
	}

	//push element to full and overflow
	{
		IntRingQueue rq(TestSize);
		for (int32_t i = 0; i < (int32_t)TestSize; i++) {
			rq.push(i);
		}
		CHECK_RINGQUEUE_SIZE(rq, TestSize, TestSize);
		REQUIRE_EQ(rq.front(), 0);

		std::vector<int32_t> v1(TestSize);
		for (size_t i = 0; i < TestSize; i++) v1[i] = rand();

		for (int32_t i = 0; i < (int32_t)TestSize; i++) {
			REQUIRE_EQ(rq.front(), i);
			rq.push(v1[(size_t)i]);
			CHECK_RINGQUEUE_SIZE(rq, TestSize, TestSize);
		}

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), TestSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, v1[i]);
		}

		rq.reset();
		CHECK_RINQUEUE_EMPTY(rq, TestSize);
	}

	//get from empty ring queue
	{
		IntRingQueue rq(TestSize);

		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_TRUE(result.empty());

		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_TRUE(result.empty());
	}

	//push one element and get
	{
		IntRingQueue rq(TestSize);

		int32_t v1 = rand();
		rq.push(v1);
		CHECK_RINGQUEUE_SIZE(rq, 1, TestSize);

		int32_t v2 = rq.front();
		REQUIRE_EQ(v1, v2);
		CHECK_RINGQUEUE_SIZE(rq, 1, TestSize);

		std::vector<std::pair<size_t, int32_t>> v3;
		rq.walk([&v3](size_t index, const int32_t& v) ->bool {
			v3.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(v3.size(), (size_t)1);
		REQUIRE_EQ(v3[0].first, (size_t)0);
		REQUIRE_EQ(v3[0].second, v1);

		v3.clear();
		rq.walk_reserve([&v3](size_t index, const int32_t& v) ->bool {
			v3.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(v3.size(), (size_t)1);
		REQUIRE_EQ(v3[0].first, (size_t)0);
		REQUIRE_EQ(v3[0].second, v1);
	}

	//push some element and get
	{
		IntRingQueue rq(TestSize);

		const size_t FillSize = 13;
		std::vector<int32_t> v1(FillSize);
		for (size_t i = 0; i < FillSize; i++) v1[i] = rand();

		//push
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(v1[i]);
		}
		CHECK_RINGQUEUE_SIZE(rq, FillSize, TestSize);
		REQUIRE_EQ(rq.front(), v1[0]);

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, v1[i]);
		}

		//get reserve
		result.clear();
		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, result.size()-1-i);
			REQUIRE_EQ(result[i].second, v1[result.size() - 1 - i]);
		}
	}

	//make wrap condition and get
	{
		IntRingQueue rq(TestSize);

		const size_t FillSize = 13;
		std::vector<int32_t> v1(FillSize*2);
		for (size_t i = 0; i < FillSize*2; i++) v1[i] = rand();

		//push 0
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(0);
		}
		//push
		for (size_t i = 0; i < FillSize; i++) {
			REQUIRE_EQ(rq.front(), 0);

			rq.push(v1[i]);
			rq.pop();
		}
		for (size_t i = 0; i < FillSize; i++) {
			REQUIRE_EQ(rq.front(), v1[0]);

			rq.push(v1[i+FillSize]);
		}

		CHECK_RINGQUEUE_SIZE(rq, FillSize * 2, TestSize);

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize*2);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, v1[i]);
		}

		//get reserve
		result.clear();
		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 2);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, result.size()-1-i);
			REQUIRE_EQ(result[i].second, v1[result.size() - 1 - i]);
		}
	}

	//make wrap condition and pop to not
	{
		IntRingQueue rq(TestSize);

		const size_t FillSize = 13;
		std::vector<int32_t> v1(FillSize * 2);
		for (size_t i = 0; i < FillSize * 2; i++) v1[i] = rand();

		//push 0
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(0);
		}
		//push
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(v1[i]);
			rq.pop();
		}
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(v1[i + FillSize]);
		}

		//pop
		const size_t PopSize = TestSize - FillSize + 2;
		for (size_t i = 0; i < PopSize; i++) {
			REQUIRE_EQ(rq.front(), v1[i]);
			rq.pop();
		}

		CHECK_RINGQUEUE_SIZE(rq, FillSize * 2-PopSize, TestSize);

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 2-PopSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, v1[i+PopSize]);
		}

		//get reserve
		result.clear();
		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 2-PopSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, result.size() - 1 - i);
			REQUIRE_EQ(result[i].second, v1[FillSize*2 - 1 - i]);
		}
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("AutoResize test for RingQueue", "[RingQueue]")
{
	typedef RingQueue<int32_t> IntRingQueue;

	// Initial conditions
	{
		IntRingQueue rq;
		CHECK_RINQUEUE_EMPTY(rq, IntRingQueue::kDefaultCapacity);
	}

	// push one element AND reset
	{
		IntRingQueue rq;
		rq.push(1);
		CHECK_RINGQUEUE_SIZE(rq, 1, IntRingQueue::kDefaultCapacity);

		REQUIRE_EQ(rq.front(), 1);

		rq.reset();
		CHECK_RINQUEUE_EMPTY(rq, IntRingQueue::kDefaultCapacity);
	}

	// push element twice
	{
		IntRingQueue rq;
		rq.push(1);
		rq.push(2);

		REQUIRE_EQ(rq.front(), 1);
		CHECK_RINGQUEUE_SIZE(rq, 2, IntRingQueue::kDefaultCapacity);
	}

	//push element to full capacity AND reset
	{
		IntRingQueue rq;
		for (int32_t i = 0; i < (int32_t)IntRingQueue::kDefaultCapacity; i++) {
			rq.push(i);
		}
		CHECK_RINGQUEUE_SIZE(rq, IntRingQueue::kDefaultCapacity, IntRingQueue::kDefaultCapacity);
		REQUIRE_EQ(rq.front(), 0);

		rq.reset();
		CHECK_RINQUEUE_EMPTY(rq, IntRingQueue::kDefaultCapacity);
	}

	//push element to full and overflow
	{
		IntRingQueue rq;
		for (int32_t i = 0; i < (int32_t)IntRingQueue::kDefaultCapacity; i++) {
			rq.push(i);
		}
		CHECK_RINGQUEUE_SIZE(rq, IntRingQueue::kDefaultCapacity, IntRingQueue::kDefaultCapacity);
		REQUIRE_EQ(rq.front(), 0);

		std::vector<int32_t> v1;
		size_t next_capcity = (rq.capacity() + 1) * 2 - 1;
		for (int32_t i = 0; i < (int32_t)(next_capcity-IntRingQueue::kDefaultCapacity); i++) {
			REQUIRE_EQ(rq.front(), 0);

			int32_t v = rand();
			v1.push_back(v);
			rq.push(v);
			CHECK_RINGQUEUE_SIZE(rq, (size_t)(IntRingQueue::kDefaultCapacity+i+1), next_capcity);
		}

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), next_capcity);

		for (size_t i = 0; i < result.size(); i++)
		{
			REQUIRE_EQ(result[i].first, i);

			if(i< IntRingQueue::kDefaultCapacity)
				REQUIRE_EQ(result[i].second, (int32_t)i);
			else
				REQUIRE_EQ(result[i].second, v1[i- IntRingQueue::kDefaultCapacity]);
		}

		size_t next_capcity2 = (rq.capacity() + 1) * 2 - 1;
		rq.push(0);
		CHECK_RINGQUEUE_SIZE(rq, next_capcity + 1, next_capcity2);
	}

	//get from empty ring queue
	{
		IntRingQueue rq;

		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_TRUE(result.empty());

		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_TRUE(result.empty());
	}

	//push one element and get
	{
		IntRingQueue rq;

		int32_t v1 = rand();
		rq.push(v1);
		CHECK_RINGQUEUE_SIZE(rq, 1, IntRingQueue::kDefaultCapacity);

		int32_t v2 = rq.front();
		REQUIRE_EQ(v1, v2);
		CHECK_RINGQUEUE_SIZE(rq, 1, IntRingQueue::kDefaultCapacity);

		std::vector<std::pair<size_t, int32_t>> v3;
		rq.walk([&v3](size_t index, const int32_t& v) ->bool {
			v3.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(v3.size(), (size_t)1);
		REQUIRE_EQ(v3[0].first, (size_t)0);
		REQUIRE_EQ(v3[0].second, v1);

		v3.clear();
		rq.walk_reserve([&v3](size_t index, const int32_t& v) ->bool {
			v3.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(v3.size(), (size_t)1);
		REQUIRE_EQ(v3[0].first, (size_t)0);
		REQUIRE_EQ(v3[0].second, v1);
	}

	//push some element and get
	{
		IntRingQueue rq;

		const size_t FillSize = 13;
		std::vector<int32_t> v1(FillSize);
		for (size_t i = 0; i < FillSize; i++) v1[i] = rand();

		//push
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(v1[i]);
		}
		CHECK_RINGQUEUE_SIZE(rq, FillSize, IntRingQueue::kDefaultCapacity);
		REQUIRE_EQ(rq.front(), v1[0]);

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, v1[i]);
		}

		//get reserve
		result.clear();
		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, result.size() - 1 - i);
			REQUIRE_EQ(result[i].second, v1[result.size() - 1 - i]);
		}
	}

	//make wrap condition and get
	{
		IntRingQueue rq;

		const size_t FillSize = 13;
		std::vector<int32_t> v1(FillSize * 2);
		for (size_t i = 0; i < FillSize * 2; i++) v1[i] = rand();

		//push 0
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(0);
		}
		//push
		for (size_t i = 0; i < FillSize; i++) {
			REQUIRE_EQ(rq.front(), 0);

			rq.push(v1[i]);
			rq.pop();

			CHECK_RINGQUEUE_SIZE(rq, FillSize, IntRingQueue::kDefaultCapacity);
		}
		for (size_t i = 0; i < FillSize; i++) {
			REQUIRE_EQ(rq.front(), v1[0]);

			rq.push(v1[i + FillSize]);
		}

		CHECK_RINGQUEUE_SIZE(rq, FillSize * 2, IntRingQueue::kDefaultCapacity);

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 2);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, v1[i]);
		}

		//get reserve
		result.clear();
		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 2);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, result.size() - 1 - i);
			REQUIRE_EQ(result[i].second, v1[result.size() - 1 - i]);
		}
	}

	//make wrap condition and resize
	{
		IntRingQueue rq;

		const size_t FillSize = 13;
		std::vector<int32_t> v1(FillSize * 2);
		for (size_t i = 0; i < FillSize * 2; i++) v1[i] = rand();

		//push 0
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(0);
		}
		//push
		for (size_t i = 0; i < FillSize; i++) {
			REQUIRE_EQ(rq.front(), 0);

			rq.push(v1[i]);
			rq.pop();
		}
		for (size_t i = 0; i < FillSize; i++) {
			REQUIRE_EQ(rq.front(), v1[0]);

			rq.push(v1[i + FillSize]);
		}

		CHECK_RINGQUEUE_SIZE(rq, FillSize * 2, IntRingQueue::kDefaultCapacity);

		//make resize
		std::vector<int32_t> v2(FillSize);
		for (size_t i = 0; i < FillSize; i++) v2[i] = rand();

		size_t next_capacity = (IntRingQueue::kDefaultCapacity + 1) * 2 - 1;

		//push
		for (size_t i = 0; i < FillSize; i++) {
			rq.push(v2[i]);

			size_t expect_size = FillSize * 2 + i+1;
			size_t expect_capacity = (expect_size<= IntRingQueue::kDefaultCapacity) ? (size_t)IntRingQueue::kDefaultCapacity : next_capacity;

			CHECK_RINGQUEUE_SIZE(rq, expect_size, expect_capacity);
		}

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 3);

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);

			if(i<FillSize*2)
				REQUIRE_EQ(result[i].second, v1[i]);
			else
				REQUIRE_EQ(result[i].second, v2[i-FillSize*2]);
		}

		//get reserve
		result.clear();
		rq.walk_reserve([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), FillSize * 3);

		for (size_t ri = 0; ri < result.size(); ri++)
		{
			size_t i = (size_t)(result.size() - 1 - ri);

			REQUIRE_EQ(result[ri].first, i);
			if (i < FillSize * 2)
				REQUIRE_EQ(result[ri].second, v1[i]);
			else
				REQUIRE_EQ(result[ri].second, v2[i - FillSize * 2]);
		}
	}

	//make the worst wrap condition and resize
	{
		IntRingQueue rq;

		rq.push(0);
		for (size_t i = 0; i < IntRingQueue::kDefaultCapacity; i++)
		{
			REQUIRE_EQ(rq.front(), 0);

			rq.push(0);
			rq.pop();
		}

		for (size_t i = 1; i < IntRingQueue::kDefaultCapacity; i++)
		{
			rq.push((int32_t)i);
		}

		CHECK_RINGQUEUE_SIZE(rq, IntRingQueue::kDefaultCapacity, IntRingQueue::kDefaultCapacity);

		rq.push((int32_t)IntRingQueue::kDefaultCapacity);

		size_t next_capacity = (IntRingQueue::kDefaultCapacity + 1) * 2 - 1;
		CHECK_RINGQUEUE_SIZE(rq, IntRingQueue::kDefaultCapacity+1, next_capacity);

		//get
		std::vector<std::pair<size_t, int32_t>> result;
		rq.walk([&result](size_t index, const int32_t& v) ->bool {
			result.push_back(std::make_pair(index, v));
			return true;
		});
		REQUIRE_EQ(result.size(), (size_t)(IntRingQueue::kDefaultCapacity + 1));

		for (size_t i = 0; i < result.size(); i++) {
			REQUIRE_EQ(result[i].first, i);
			REQUIRE_EQ(result[i].second, (int32_t)i);
		}
	}
}
