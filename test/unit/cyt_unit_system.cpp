#include <cy_core.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {

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
class TestLockThread
{
public:
	atomic_int32_t m_lock_status;
	sys_api::mutex_t m_mutex;
	sys_api::signal_t m_signal;

	void test_lock(void*)
	{
		m_lock_status = 0;
		sys_api::mutex_lock(m_mutex);  //block here and wait main thread to unlock
		m_lock_status = 1;
		sys_api::signal_wait(m_signal);
		sys_api::mutex_unlock(m_mutex);
		m_lock_status = 2;
	}
	
	void test_try_lock_failed(void*)
	{
		m_lock_status = 0;
		REQUIRE_FALSE(sys_api::mutex_try_lock(m_mutex));
		m_lock_status = 1;
	}
	
	void test_try_lock_success(void*)
	{
		m_lock_status = 0;
		REQUIRE_TRUE(sys_api::mutex_try_lock(m_mutex));
		m_lock_status = 1;
		sys_api::thread_sleep(1000);
		sys_api::mutex_unlock(m_mutex);
		m_lock_status = 2;
	}

	void test_lock_wait(void*)
	{
		m_lock_status = 0;
		sys_api::mutex_lock(m_mutex);
		m_lock_status = 1;
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
	TestLockThread() : m_lock_status(0), m_mutex(nullptr), m_signal(nullptr) { }
	TestLockThread(sys_api::mutex_t m, sys_api::signal_t s) : m_lock_status(0), m_mutex(m), m_signal(s) {  }
};

//-------------------------------------------------------------------------------------
TEST_CASE("System mutex basic test", "[System][Mutex][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::mutex_t mutex = sys_api::mutex_create();

	// Test that all functions handle nullptr safely(These should not crash and should return safely)
	sys_api::mutex_lock(nullptr);
	REQUIRE_FALSE(sys_api::mutex_try_lock(nullptr));
	sys_api::mutex_unlock(nullptr);
	sys_api::mutex_destroy(nullptr);

	//lock and unlock
	sys_api::mutex_lock(mutex);
	sys_api::mutex_lock(mutex);
	REQUIRE_TRUE(sys_api::mutex_try_lock(mutex));
	sys_api::mutex_unlock(mutex);
	sys_api::mutex_destroy(mutex);
}

//-------------------------------------------------------------------------------------
TEST_CASE("System mutex test", "[System][Mutex]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::mutex_t m = sys_api::mutex_create();
	sys_api::signal_t s = sys_api::signal_create();
	//-------------------------
	//lock in main thread
	sys_api::mutex_lock(m);

	//test lock in other thread
	TestLockThread another_thread(m, s);
	thread_t t1 = sys_api::thread_create(std::bind(&TestLockThread::test_lock, &another_thread, std::placeholders::_1), nullptr, nullptr);

	//wait 0.5 sec
	sys_api::thread_sleep(500);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 0);

	//free in main thread
	sys_api::mutex_unlock(m); //make other thread to continue
	sys_api::thread_sleep(100); //wait other thread to lock
	REQUIRE_EQ(another_thread.m_lock_status.load(), 1); //should be locked in other thread

	//try lock in main thread(should fail)
	REQUIRE_FALSE(sys_api::mutex_try_lock(m));

	sys_api::signal_notify(s); //make other thread to continue
	sys_api::thread_join(t1);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 2); //should unlock in other thread

	//--------------------
	//lock in main thread
	REQUIRE_EQ(sys_api::mutex_try_lock(m), true);

	//test lock time out in other thread
	another_thread.m_lock_status = 0;
	thread_t t2 = sys_api::thread_create(std::bind(&TestLockThread::test_try_lock_failed, &another_thread, std::placeholders::_1), nullptr, nullptr);

	//other thread should stopped
	sys_api::thread_join(t2);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 1); 

	//unlock in main thread
	sys_api::mutex_unlock(m);

	//--------------------------
	//test lock time out in other thread
	another_thread.m_lock_status = 0;
	thread_t t3 = sys_api::thread_create(std::bind(&TestLockThread::test_try_lock_success, &another_thread, std::placeholders::_1), nullptr, nullptr);

	//wait 0.5 sec
	sys_api::thread_sleep(500);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 1); //should be locked in other thread
	REQUIRE_FALSE(sys_api::mutex_try_lock(m));

	sys_api::thread_sleep(1000);
	REQUIRE_EQ(another_thread.m_lock_status.load(), 2); //should unlock in other thread
	sys_api::thread_join(t3);
	REQUIRE_TRUE(sys_api::mutex_try_lock(m));
	sys_api::mutex_unlock(m);

	//run several work thread to wait unlock
	int32_t work_thread_counts = sys_api::get_cpu_counts();
	TestLockThread* work_thread = new TestLockThread[work_thread_counts];
	thread_t* work_thread_handle = new thread_t[work_thread_counts];

	//------------------------------
	//lock in main thread
	REQUIRE_TRUE(sys_api::mutex_try_lock(m));

	std::pair<bool, int32_t> the_ball(false, 0);
	for (int32_t i = 0; i < work_thread_counts; i++) {
		work_thread[i].m_mutex = m;
		work_thread_handle[i] = sys_api::thread_create(std::bind(&TestLockThread::test_pass_ball, &(work_thread[i]), std::placeholders::_1), &the_ball, nullptr);
	}

	int32_t* result = new int32_t[work_thread_counts];
	memset(result, 0, sizeof(int32_t)*work_thread_counts);

	//unlock in main thread
	sys_api::mutex_unlock(m);

	for (int32_t i = 0; i < work_thread_counts; i++) {
		sys_api::thread_join(work_thread_handle[i]);

		int32_t ball_number = work_thread[i].m_lock_status.load();
		REQUIRE_RANGE(ball_number, 0, work_thread_counts);

		result[ball_number] += 1;
	}
	for (int32_t i = 0; i < work_thread_counts; i++) {
		REQUIRE_EQ(result[i], 1);
	}

	//should unlock status now
	REQUIRE_TRUE(sys_api::mutex_try_lock(m));
	sys_api::mutex_unlock(m);

	delete[] result;
	delete[] work_thread;
	delete[] work_thread_handle;

	//-----------------------------------
	//clean
	sys_api::mutex_destroy(m);
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

}
