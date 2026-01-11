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

struct AtomicTestThreadData
{
	atomic_int32_t* a;
	int32_t loop_counts;
	sys_api::signal_t begin_signal;
};

//-------------------------------------------------------------------------------------
static void _fetchAddThread(void* param)
{
	AtomicTestThreadData* threadData = (AtomicTestThreadData*)param;
	sys_api::signal_wait(threadData->begin_signal);

	for(int32_t i=0; i<threadData->loop_counts; i++)
	{
		threadData->a->fetch_add(1);
	}
}

//-------------------------------------------------------------------------------------
static void _fetchSubThread(void* param)
{
	AtomicTestThreadData* threadData = (AtomicTestThreadData*)param;
	sys_api::signal_wait(threadData->begin_signal);

	for (int32_t i = 0; i < threadData->loop_counts; i++)
	{
		threadData->a->fetch_sub(1);
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("System atomic stress test", "[System][Atomic][Stress]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t k_thread_counts = sys_api::get_cpu_counts() < 16 ? 16 : sys_api::get_cpu_counts();

	atomic_int32_t a(42);

	AtomicTestThreadData threadData;
	threadData.begin_signal = sys_api::signal_create(true);
	threadData.a = &a;
	threadData.loop_counts = 10000;
	
	thread_t* fetchAddThread = new thread_t[k_thread_counts];
	thread_t* fetchSubThread = new thread_t[k_thread_counts];
	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		fetchAddThread[i] = sys_api::thread_create(_fetchAddThread, &threadData, nullptr);
		fetchSubThread[i] = sys_api::thread_create(_fetchSubThread, &threadData, nullptr);
	}

	//begin all threads
	sys_api::signal_notify(threadData.begin_signal);

	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		sys_api::thread_join(fetchAddThread[i]);
		sys_api::thread_join(fetchSubThread[i]);
	}

	REQUIRE_EQ(threadData.a->load(), 42);
	
	delete[] fetchAddThread;
	delete[] fetchSubThread;
}
