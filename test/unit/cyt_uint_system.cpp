#include <cy_core.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
TEST_CASE("Basic test for System", "[System]")
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
class TestLockThread
{
public:
	atomic_int32_t m_lock_status;
	sys_api::mutex_t m_mutex;

	void test_lock(void*)
	{
		m_lock_status = 0;
		sys_api::mutex_lock(m_mutex);
		m_lock_status = 1;
		sys_api::thread_sleep(1000);
		sys_api::mutex_unlock(m_mutex);
		m_lock_status = 2;
	}

	void test_lock_time_out(void* param)
	{
		int32_t wait_time_ms = *((int32_t*)param);
		m_lock_status = 0;
		bool got = sys_api::mutex_try_lock(m_mutex, wait_time_ms);
		m_lock_status = got ? 1 : 2;

		if (got == 1) {
			sys_api::thread_sleep(wait_time_ms+100);
			sys_api::mutex_unlock(m_mutex);
		}
	}

	void test_lock_on_time(void*)
	{
		m_lock_status = 0;
		bool got = sys_api::mutex_try_lock(m_mutex, 0);
		m_lock_status = got ? 1 : 2;
		sys_api::thread_sleep(1000);
		sys_api::mutex_unlock(m_mutex);
		m_lock_status = 3;
	}

	void test_pass_ball(void* param)
	{
		std::pair<bool, int32_t>* ball = (std::pair<bool, int32_t>*)(param);

		sys_api::mutex_lock(m_mutex);
		REQUIRE_FALSE(ball->first);

		ball->first = true;
		m_lock_status = ball->second;
		ball->second += 1;
		sys_api::thread_yield();

		ball->first = false;
		sys_api::mutex_unlock(m_mutex);
	}
	
public:
	TestLockThread() : m_lock_status(0), m_mutex(0) { }
	TestLockThread(sys_api::mutex_t m) : m_lock_status(0), m_mutex(m) {  }
};

//-------------------------------------------------------------------------------------
TEST_CASE("Mutex test for System", "[System]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::mutex_t m = sys_api::mutex_create();

	//-------------------------
	//lock in main thread
	sys_api::mutex_lock(m);

	//test lock in other thread
	TestLockThread another_thread(m);
	thread_t t1 = sys_api::thread_create(std::bind(&TestLockThread::test_lock, &another_thread, std::placeholders::_1), nullptr, nullptr);

	//wait 0.5 sec
	sys_api::thread_sleep(500);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 0);

	//free in main thread
	sys_api::mutex_unlock(m);
	sys_api::thread_sleep(500);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 1); //should be locked in other thread

	//try lock in main thread
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 0), false);

	//try lock in main thread with time out
	int64_t begin_time = sys_api::performance_time_now();
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 100), false);
	int64_t end_time = sys_api::performance_time_now();
	int32_t time_spend = (int32_t)(end_time - begin_time) / 1000;

	REQUIRE_GE(time_spend, 100);
	REQUIRE_LE(time_spend, 200);

	sys_api::thread_join(t1);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 2); //should unlock in other thread

	//--------------------
	//lock in main thread
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 0), true);

	//test lock time out in other thread
	another_thread.m_lock_status = 0;
	int32_t wait_time_ms = 1000;
	thread_t t2 = sys_api::thread_create(std::bind(&TestLockThread::test_lock_time_out, &another_thread, std::placeholders::_1), &wait_time_ms, nullptr);

	//wait 0.5 sec
	sys_api::thread_sleep(500);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 0);

	//wait 1 sec
	sys_api::thread_sleep(1000);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 2); //other thread should stopped
	sys_api::thread_join(t2);

	//unlock in main thread
	sys_api::mutex_unlock(m);

	//--------------------------
	//test lock time out in other thread
	another_thread.m_lock_status = 0;
	thread_t t3 = sys_api::thread_create(std::bind(&TestLockThread::test_lock_on_time, &another_thread, std::placeholders::_1), nullptr, nullptr);

	//wait 0.5 sec
	sys_api::thread_sleep(500);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 1); //should be locked in other thread
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 0), false);

	sys_api::thread_sleep(1000);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 3); //should unlock in other thread
	sys_api::thread_join(t3);
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 0), true);
	sys_api::mutex_unlock(m);

	//-------------------------------
	//lock in main thread 
	begin_time = sys_api::performance_time_now();
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 100), true);
	end_time = sys_api::performance_time_now();
	time_spend = (int32_t)(end_time - begin_time) / 1000;

	REQUIRE_GE(time_spend, 0);
	REQUIRE_LE(time_spend, 50);

	//run several work thread to wait unlock
	size_t work_thread_counts = (size_t)sys_api::get_cpu_counts();
	TestLockThread* work_thread = new TestLockThread[work_thread_counts];
	thread_t* work_thread_id = new thread_t[work_thread_counts];

	wait_time_ms = 1000;
	for (size_t i = 0; i < work_thread_counts; i++) {
		work_thread[i].m_mutex = m;
		work_thread_id[i] = sys_api::thread_create(std::bind(&TestLockThread::test_lock_time_out, &(work_thread[i]), std::placeholders::_1), &wait_time_ms, nullptr);
	}

	//unlock in main thread
	sys_api::mutex_unlock(m);

	// join all thread
	int32_t got_counts = 0, did_not_got_counts = 0;
	for (size_t i = 0; i < work_thread_counts; i++) {
		sys_api::thread_join(work_thread_id[i]);

		REQUIRE_TRUE(work_thread[i].m_lock_status.load() == 1 || work_thread[i].m_lock_status.load() == 2);

		if (work_thread[i].m_lock_status == 1)
			got_counts++;

		if (work_thread[i].m_lock_status == 2)
			did_not_got_counts++;
	}

	REQUIRE_EQ(got_counts, 1);
	REQUIRE_EQ((size_t)(got_counts + did_not_got_counts), work_thread_counts);

	//------------------------------
	//lock in main thread
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 0), true);

	std::pair<bool, int32_t> the_ball(false, 0);
	for (size_t i = 0; i < work_thread_counts; i++) {
		work_thread[i].m_mutex = m;
		work_thread_id[i] = sys_api::thread_create(std::bind(&TestLockThread::test_pass_ball, &(work_thread[i]), std::placeholders::_1), &the_ball, nullptr);
	}

	int32_t* result = new int32_t[work_thread_counts];
	memset(result, 0, sizeof(int32_t)*work_thread_counts);

	//unlock in main thread
	sys_api::mutex_unlock(m);

	for (size_t i = 0; i < work_thread_counts; i++) {
		sys_api::thread_join(work_thread_id[i]);

		int32_t ball_number = work_thread[i].m_lock_status.load();
		REQUIRE_TRUE(ball_number >= 0 || (size_t)ball_number < work_thread_counts);

		result[ball_number] += 1;
	}
	for (size_t i = 0; i < work_thread_counts; i++) {
		REQUIRE_EQ(result[i], 1);
	}

	//should unlock status now
	REQUIRE_EQ(sys_api::mutex_try_lock(m, 0), true);
	sys_api::mutex_unlock(m);


	//-----------------------------------
	//clean
	sys_api::mutex_destroy(m);
	delete[] result;
	delete[] work_thread;
	delete[] work_thread_id;
}

//-------------------------------------------------------------------------------------
TEST_CASE("Atomic test for System", "[System]")
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

}
