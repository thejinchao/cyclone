#include <cy_core.h>
#include "cyt_unit_utils.h"

#include <array>

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST_CASE("LockFreeQueue basic test", "[LockFreeQueue][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t QUEUE_SIZE = 32;
	typedef LockFreeQueue<int32_t, QUEUE_SIZE> IntQueue;
	IntQueue queue;

	REQUIRE_EQ(0ll, queue.size());

	int32_t pop_num;
	bool have_data = queue.pop(pop_num);
	REQUIRE_FALSE(have_data);
	REQUIRE_EQ(0ll, queue.size());

	int32_t push_num = (int32_t)rand();
	bool push_ret = queue.push(push_num);
	REQUIRE_EQ(1ll, queue.size());
	REQUIRE_TRUE(push_ret);

	have_data = queue.pop(pop_num);
	REQUIRE_EQ(pop_num, push_num);
	REQUIRE_TRUE(have_data);
	REQUIRE_EQ(0ll, queue.size());

	for (int32_t t = 0; t < 10; t++) 
	{
		for (int32_t i = 0; i < QUEUE_SIZE - 1; i++) {
			push_ret = queue.push(i);
			REQUIRE_TRUE(push_ret);
			REQUIRE_EQ(size_t(i + 1), queue.size());
		}

		REQUIRE_EQ(size_t(QUEUE_SIZE - 1), queue.size());
		push_ret = queue.push(QUEUE_SIZE);
		REQUIRE_FALSE(push_ret);

		for (int32_t i = 0; i < QUEUE_SIZE - 1; i++) {
			have_data = queue.pop(pop_num);
			REQUIRE_TRUE(have_data);
			REQUIRE_EQ(i, pop_num);
			REQUIRE_EQ(size_t(QUEUE_SIZE - i - 2), queue.size());
		}
		REQUIRE_EQ(0ll, queue.size());
	}
}

//-------------------------------------------------------------------------------------
template<int32_t PUSH_THREADS, int32_t POP_THREADS, int32_t TOP_VALUE>
class MultiThreadPushPop
{
public:
	typedef LockFreeQueue<uint32_t, 1024> UIntQueue;

	struct ThreadData
	{
		MultiThreadPushPop* globalData;
		thread_t 			threadHandle;
		uint32_t			threadIndex;
	};

private:
	UIntQueue*			m_queue;
	std::array<std::atomic_flag, (size_t)(TOP_VALUE+1)> m_result;

	atomic_bool_t		m_noMoreData;
	sys_api::signal_t	m_beginSignal;

public:
	MultiThreadPushPop()
		: m_queue(0)
		, m_noMoreData(false)
		, m_beginSignal(nullptr)
	{
	}

	~MultiThreadPushPop()
	{
	}

public:
	void pushAndPop(void) {
		m_queue = new UIntQueue();
		for (uint32_t i = 0; i <= TOP_VALUE; i++) {
			m_result[i].clear();
		}
		m_beginSignal = sys_api::signal_create(true);
		m_noMoreData = false;

		std::array< ThreadData, (size_t)PUSH_THREADS> pushThreadData;
		std::array< ThreadData, (size_t)POP_THREADS> popThreadData;

		for (size_t i = 0; i < PUSH_THREADS; i++) {
			pushThreadData[i].globalData = this;
			pushThreadData[i].threadIndex = static_cast<uint32_t>(i);
			pushThreadData[i].threadHandle = sys_api::thread_create(
				std::bind(&MultiThreadPushPop::pushFunction, this, std::placeholders::_1), &(pushThreadData[i]), nullptr);
		}
		for (size_t i = 0; i < POP_THREADS; i++) {
			popThreadData[i].globalData = this;
			popThreadData[i].threadIndex = static_cast<uint32_t>(i);
			popThreadData[i].threadHandle = sys_api::thread_create(
				std::bind(&MultiThreadPushPop::popFunction, this, std::placeholders::_1), &(popThreadData[i]), nullptr);
		}

		//begin work
		sys_api::signal_notify(m_beginSignal);

		//wait all push thread
		for (size_t i = 0; i < PUSH_THREADS; i++) 
		{
			sys_api::thread_join(pushThreadData[i].threadHandle);
		}	
		m_noMoreData = true;

		//wait all pop thread
		for (size_t i = 0; i < POP_THREADS; i++)
		{
			sys_api::thread_join(popThreadData[i].threadHandle);
		}

		//check result
		for (size_t i = 1; i <= TOP_VALUE; i++)
		{
			REQUIRE_TRUE(m_result[i].test_and_set());
		}

		//free memory
		sys_api::signal_destroy(m_beginSignal);
		delete m_queue;
	}

private:
	void pushFunction(void* param)
	{
		ThreadData* threadData = (ThreadData*)param;
		sys_api::signal_wait(m_beginSignal);

		uint32_t nextValue = threadData->threadIndex + 1;
		for(;;)
		{
			if (nextValue > TOP_VALUE) break;

			REQUIRE_RANGE(nextValue, 1u, TOP_VALUE);
			while (!(m_queue->push(nextValue))) {
				sys_api::thread_yield();
			}
			nextValue += PUSH_THREADS;
		};
	}

	void popFunction(void*)
	{
		sys_api::signal_wait(m_beginSignal);

		for (;;)
		{
			uint32_t pop_value;
			bool pop_result = m_queue->pop(pop_value);

			if (pop_result) 
			{
				REQUIRE_RANGE(pop_value, 1u, TOP_VALUE);
				REQUIRE_FALSE(m_result[pop_value].test_and_set());
			}
			else 
			{
				if (m_noMoreData.load()) break;

				//minor wait when queue is empty
				sys_api::thread_yield();
			}
		}
	}
};

//-------------------------------------------------------------------------------------
TEST_CASE("LockFreeQueue multi thread test", "[LockFreeQueue][MultiThread]")
{
	PRINT_CURRENT_TEST_NAME();

	//single-producer single-consumer
	MultiThreadPushPop<1, 1, 100> test1;
	test1.pushAndPop();
	
	//multi-producer single-consumer
	MultiThreadPushPop<3, 1, 1000> test2;
	test2.pushAndPop();

	//single-producer multi-consumer
	MultiThreadPushPop<1, 3, 1000> test3;
	test3.pushAndPop();

	//multi-producer multi-consumer
	MultiThreadPushPop<5, 3, 100000> test4;
	test4.pushAndPop();

	//multi-producer multi-consumer
	MultiThreadPushPop<3, 5, 100000> test5;
	test5.pushAndPop();
}

TEST_CASE("LockFreeQueue high contention stress test", "[LockFreeQueue][Stress]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t QUEUE_SIZE = 256;
	typedef LockFreeQueue<uint64_t, QUEUE_SIZE> UInt64Queue;
	UInt64Queue queue;

	const int32_t THREAD_COUNT = 16;
	const int32_t OPERATIONS_PER_THREAD = 50000;
	std::atomic<uint64_t> push_count(0);
	std::atomic<uint64_t> pop_count(0);
	std::atomic<uint64_t> push_failures(0);
	std::atomic<uint64_t> pop_failures(0);
	sys_api::signal_t start_flag = sys_api::signal_create(true);
	std::vector<thread_t> threads;

	// create work thread
	for (int32_t i = 0; i < THREAD_COUNT; i++) 
	{
		threads.emplace_back( sys_api::thread_create( [&](void*) 
		{
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 99);

			sys_api::signal_wait(start_flag);

			for (int32_t j = 0; j < OPERATIONS_PER_THREAD; j++) {
				// 70% push, 30% pop
				if (dis(gen) < 70) {
					uint64_t value = push_count.fetch_add(1);
					if (!queue.push(value)) {
						push_failures.fetch_add(1);
						push_count.fetch_sub(1);
					}
				}
				else {
					uint64_t val;
					if (queue.pop(val)) {
						pop_count.fetch_add(1);
					}
					else {
						pop_failures.fetch_add(1);
					}
				}
			}
		}, nullptr, nullptr));
	}

	//begin work
	sys_api::signal_notify(start_flag);
	
	//wait all thread done
	for (auto& t : threads) 
	{
		sys_api::thread_join(t);
	}

	// cleanup the remaining data
	uint64_t val;
	while (queue.pop(val)) 
	{
		pop_count.fetch_add(1);
	}

	REQUIRE_EQ(push_count.load(), pop_count.load());
}

TEST_CASE("LockFreeQueue different data types", "[LockFreeQueue][Types]")
{
	PRINT_CURRENT_TEST_NAME();

	// int64_t
	{
		LockFreeQueue<int64_t, 64> queue;
		REQUIRE_TRUE(queue.push(INT64_MAX));
		int64_t val;
		REQUIRE_TRUE(queue.pop(val));
		REQUIRE_EQ(INT64_MAX, val);
	}

	// double
	{
		LockFreeQueue<double, 64> queue;
		double test_val = 3.141592653589793;
		REQUIRE_TRUE(queue.push(test_val));
		double val;
		REQUIRE_TRUE(queue.pop(val));
		REQUIRE_EQ(test_val, val);
	}

	// struct
	{
		struct TestStruct {
			int32_t a;
			int32_t b;
			double c;
		};
		static_assert(std::is_trivial<TestStruct>::value, "TestStruct must be trivial");

		LockFreeQueue<TestStruct, 64> queue;
		TestStruct ts = { 123, 456, 789.0 };
		REQUIRE_TRUE(queue.push(ts));
		TestStruct val;
		REQUIRE_TRUE(queue.pop(val));
		REQUIRE_EQ(123, val.a);
		REQUIRE_EQ(456, val.b);
		REQUIRE_EQ(789.0, val.c);
	}
}

TEST_CASE("LockFreeQueue interleaved operations", "[LockFreeQueue][Interleaved]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t QUEUE_SIZE = 1024;
	typedef LockFreeQueue<int32_t, QUEUE_SIZE> IntQueue;
	IntQueue queue;

	const int32_t THREAD_COUNT = 4;
	const int32_t OPERATIONS = 10000;
	std::atomic<int32_t> success_count(0);
	sys_api::signal_t start_flag = sys_api::signal_create(true);
	std::vector<thread_t> threads;

	for (int32_t i = 0; i < THREAD_COUNT; i++) 
	{
		threads.emplace_back(sys_api::thread_create([&](void*)
		{
			sys_api::signal_wait(start_flag);

			// push and pop interleaved
			for (int32_t j = 0; j < OPERATIONS; j++) 
			{
				if (j % 2 == 0) 
				{
					if (queue.push(i * OPERATIONS + j)) 
					{
						success_count.fetch_add(1);
					}
				}
				else 
				{
					int32_t val;
					if (queue.pop(val)) 
					{
						success_count.fetch_add(1);
					}
				}
			}
			}, nullptr, nullptr));
	}

	//begin work
	sys_api::signal_notify(start_flag);

	//wait all thread done
	for (auto& t : threads)
	{
		sys_api::thread_join(t);
	}

	// clean the remain data
	int32_t val;
	while (queue.pop(val)) 
	{
		success_count.fetch_add(1);
	}

	REQUIRE_RANGE(success_count.load(), OPERATIONS * THREAD_COUNT - 10, OPERATIONS * THREAD_COUNT + 10);
}
