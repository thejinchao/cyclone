#include <cy_core.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

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
	TestLockThread() : m_lock_status(0), m_mutex(nullptr), m_signal(nullptr) {}
	TestLockThread(sys_api::mutex_t m, sys_api::signal_t s) : m_lock_status(0), m_mutex(m), m_signal(s) {}
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
	sys_api::auto_mutex lock(nullptr); //should not crash

	//lock and unlock
	sys_api::mutex_lock(mutex);
	sys_api::mutex_lock(mutex);
	REQUIRE_TRUE(sys_api::mutex_try_lock(mutex));
	sys_api::mutex_unlock(mutex);
	sys_api::mutex_destroy(mutex);
}

//-------------------------------------------------------------------------------------
TEST_CASE("System mutex multithread test", "[System][Mutex][MultiThread]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::mutex_t m = sys_api::mutex_create();
	sys_api::signal_t s = sys_api::signal_create();
	//-------------------------
	//lock in main thread and try lock in other thread(failed then success)
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
	//lock in main thread and try lock in other thread(failed then exit)
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
	//test lock in other thread(lock success and hold 1 sec then unlock)
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

	//--------------------------
	//reentrant lock edge 
	sys_api::mutex_lock(m);
	sys_api::mutex_lock(m);
	sys_api::mutex_lock(m);
	REQUIRE_TRUE(sys_api::mutex_try_lock(m));
	sys_api::mutex_unlock(m); //only one unlock needed

	atomic_bool_t other_thread_got_lock(false);
	thread_t t = sys_api::thread_create([m, &other_thread_got_lock](void*) {
		if (sys_api::mutex_try_lock(m)) 
		{
			other_thread_got_lock = true;
			sys_api::mutex_unlock(m);
		}
	}, nullptr, nullptr);

	sys_api::thread_join(t);
	REQUIRE_TRUE(other_thread_got_lock.load());

	//clean
	sys_api::mutex_destroy(m);
}

//-------------------------------------------------------------------------------------
TEST_CASE("System mutex multithread passball test", "[System][Mutex][MultiThread][Passball]")
{
	PRINT_CURRENT_TEST_NAME();

	//lock in main thread
	sys_api::mutex_t m = sys_api::mutex_create();
	REQUIRE_TRUE(sys_api::mutex_try_lock(m));

	//run several work thread
	const int32_t k_thread_counts = sys_api::get_cpu_counts() < 16 ? 16 : sys_api::get_cpu_counts();
	TestLockThread* work_thread = new TestLockThread[k_thread_counts];
	thread_t* work_thread_handle = new thread_t[k_thread_counts];

	std::pair<bool, int32_t> the_ball(false, 0);
	for (int32_t i = 0; i < k_thread_counts; i++) {
		work_thread[i].m_mutex = m;
		work_thread_handle[i] = sys_api::thread_create(std::bind(&TestLockThread::test_pass_ball, &(work_thread[i]), std::placeholders::_1), &the_ball, nullptr);
	}

	int32_t* result = new int32_t[k_thread_counts];
	memset(result, 0, sizeof(int32_t) * k_thread_counts);

	//unlock in main thread
	sys_api::mutex_unlock(m);

	for (int32_t i = 0; i < k_thread_counts; i++) {
		sys_api::thread_join(work_thread_handle[i]);

		int32_t ball_number = work_thread[i].m_lock_status.load();
		REQUIRE_RANGE(ball_number, 0, k_thread_counts);

		result[ball_number] += 1;
	}
	for (int32_t i = 0; i < k_thread_counts; i++) {
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

struct CompeteThreadData
{
	sys_api::mutex_t mutex;
	thread_t thread_handle;
	int32_t thread_index;

	atomic_int32_t* ready_counts;
	sys_api::signal_t begin_signal;
	atomic_int32_t* finish_counts;
	bool winner;

	sys_api::signal_t next_round_signal;

	bool* game_end;
};

//-------------------------------------------------------------------------------------
void test_compete_ball(void* param)
{
	CompeteThreadData* threadData = (CompeteThreadData*)(param);
	
	while (*(threadData->game_end) == false)
	{
		//indicate ready
		threadData->ready_counts->fetch_add(1);

		//wait begin signal
		sys_api::signal_wait(threadData->begin_signal);

		//try to get the lock
		bool winner = sys_api::mutex_try_lock(threadData->mutex);
		threadData->winner = winner;

		//indicate finish
		threadData->finish_counts->fetch_add(1);

		//signal next round
		sys_api::signal_wait(threadData->next_round_signal);

		if (winner)
		{
			sys_api::mutex_unlock(threadData->mutex);
		}
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("System mutex multithread competition test", "[System][Mutex][MultiThread][Competitor]")
{
	PRINT_CURRENT_TEST_NAME();

	//create lock
	sys_api::mutex_t m = sys_api::mutex_create();

	//run several work thread
	const int32_t k_thread_counts = sys_api::get_cpu_counts() < 16 ? 16 : sys_api::get_cpu_counts();
	CompeteThreadData* threadData = new CompeteThreadData[k_thread_counts];
	const int32_t k_test_rounds = 1000;

	atomic_int32_t ready_counts(0), finish_counts(0);
	bool game_end = false;
	sys_api::signal_t begin_signal = sys_api::signal_create(true);
	sys_api::signal_t next_round_signal = sys_api::signal_create(true);

	for (int32_t i = 0; i < k_thread_counts; i++) {
		threadData[i].mutex = m;
		threadData[i].thread_index = i;
		threadData[i].ready_counts = &ready_counts;
		threadData[i].begin_signal = begin_signal;
		threadData[i].finish_counts = &finish_counts;
		threadData[i].next_round_signal = next_round_signal;
		threadData[i].game_end = &game_end;
		threadData[i].thread_handle = sys_api::thread_create(test_compete_ball, &(threadData[i]), nullptr);
	}

	for (int32_t j = 0; j < k_test_rounds; j++)  //test k_test_rounds rounds
	{
		//wait all threads ready
		while (ready_counts.load() < k_thread_counts) {
			sys_api::thread_yield();
		}

		sys_api::signal_reset(next_round_signal);

		//notify all threads to begin compete
		sys_api::signal_notify(begin_signal);

		//wait all threads finish
		while (finish_counts.load() < k_thread_counts) {
			sys_api::thread_yield();
		}

		//check result
		bool winner_found = false;
		for (int32_t i = 0; i < k_thread_counts; i++)
		{
			if (threadData[i].winner)
			{
				REQUIRE_FALSE(winner_found); //only one winner allowed
				winner_found = true;
			}
		}
		REQUIRE_TRUE(winner_found);

		//reset for next round
		ready_counts = 0;
		finish_counts = 0;
		sys_api::signal_reset(begin_signal);
		for (int32_t i = 0; i < k_thread_counts; i++)
		{
			threadData[i].winner = false;
		}

		//notify all threads to next round
		game_end = (j== k_test_rounds-1);
		sys_api::signal_notify(next_round_signal);
	}

	//clean up
	for (int32_t i = 0; i < k_thread_counts; i++) {
		sys_api::thread_join(threadData[i].thread_handle);
	}
	delete[] threadData;
}

TEST_CASE("System mutex auto_mutex RAII test", "[System][Mutex][RAII]")
{
	PRINT_CURRENT_TEST_NAME();
	sys_api::mutex_t mutex = sys_api::mutex_create();
	atomic_int32_t counter(0);
	{
		sys_api::auto_mutex lock(mutex);        
		REQUIRE_EQ(counter.load(), 0);        
		counter = 1;    
	}

	bool exception_thrown = false;
	try {
		sys_api::auto_mutex lock(mutex);
		throw std::runtime_error("test exception");    
	} catch (...) 
	{
		exception_thrown = true;    
	}
	REQUIRE_TRUE(exception_thrown);

	REQUIRE_TRUE(sys_api::mutex_try_lock(mutex));
	sys_api::mutex_unlock(mutex);
	sys_api::mutex_destroy(mutex);
}

TEST_CASE("System mutex stress test", "[System][Mutex][Stress]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::mutex_t mutex = sys_api::mutex_create();
	const int32_t k_thread_counts = sys_api::get_cpu_counts() < 16 ? 16 : sys_api::get_cpu_counts();
	int32_t k_test_counts = 1000;
	atomic_int32_t counter(0);

	std::vector<thread_t> threads;

	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		thread_t t = sys_api::thread_create([mutex, &counter, k_test_counts](void*)
			{
				for (int32_t j = 0; j < k_test_counts; j++) 
				{
					sys_api::mutex_lock(mutex);
					int32_t old_val = counter.load();
					sys_api::thread_yield();
					counter = old_val + 1;
					sys_api::mutex_unlock(mutex);
				}
			}, nullptr, nullptr);
		threads.push_back(t);
	}

	for (auto t : threads) 
	{
		sys_api::thread_join(t);
	}

	REQUIRE_EQ(counter.load(), k_thread_counts * k_test_counts);
	sys_api::mutex_destroy(mutex);
}

TEST_CASE("System mutex memory visibility test", "[System][Mutex][Memory]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::mutex_t mutex = sys_api::mutex_create();
	atomic_bool_t data_ready(false);
	int32_t shared_data = 0;

	// producer thread
	thread_t producer = sys_api::thread_create([mutex, &shared_data, &data_ready](void*) 
		{
			sys_api::mutex_lock(mutex);
			shared_data = 42; // write data
			data_ready = true;
			sys_api::mutex_unlock(mutex);
		}, nullptr, nullptr
	);

	// consumer thread
	thread_t consumer = sys_api::thread_create([mutex, &shared_data, &data_ready](void*) 
		{
			while (!data_ready.load()) {
				sys_api::thread_yield();
			}
			sys_api::mutex_lock(mutex);
			REQUIRE_EQ(shared_data, 42);
			sys_api::mutex_unlock(mutex);
		}, nullptr, nullptr
	);

	sys_api::thread_join(producer);
	sys_api::thread_join(consumer);

	sys_api::mutex_destroy(mutex);
}
