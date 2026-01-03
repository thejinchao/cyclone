#include <cy_core.h>
#include "cyt_unit_utils.h"
#include <vector>

using namespace cyclone;

//-------------------------------------------------------------------------------------
TEST_CASE("Signal basic test", "[Signal][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	// Test that all functions handle nullptr safely(These should not crash and should return safely)
	sys_api::signal_destroy(nullptr);
	sys_api::signal_wait(nullptr);
	sys_api::signal_notify(nullptr);
	REQUIRE_FALSE(sys_api::signal_timewait(nullptr, 100));
	REQUIRE_FALSE(sys_api::signal_timewait(nullptr, 0));

	//create signal
	sys_api::signal_t signal = sys_api::signal_create();

	//wait
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 1));

	//notify, wait and timewait
	sys_api::signal_notify(signal);
	sys_api::signal_wait(signal);
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 1));

	//notify once and time_wait twice
	sys_api::signal_notify(signal);
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 1));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0));

	//notify once and time_wait twice
	sys_api::signal_notify(signal);
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 1));

	//notify once and time_wait twice
	sys_api::signal_notify(signal);
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0));

	//notify twice and time_wait twice
	sys_api::signal_notify(signal);
	sys_api::signal_notify(signal); // Multiple notify (it's binary event)
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 0)); // First wait should immediately return (signal is already set)
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0)); // Second wait should timeout (signal was consumed)
	// Notify again
	sys_api::signal_notify(signal);
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 0));

	// Multiple notify calls (binary signal, should not accumulate)
	for (int i = 0; i < 10; i++) {
		sys_api::signal_notify(signal);
	}
	// Only one wait should succeed (signal is binary)
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 0));
	// Subsequent waits should fail
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 10));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0));

	//time_wait
	sys_api::signal_timewait(signal, 0); //consume the signal,make it unsignaled
	int64_t begin_time = sys_api::performance_time_now();
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 100));
	int64_t end_time = sys_api::performance_time_now();
	REQUIRE_RANGE(end_time - begin_time, 100 * 1000, 120 * 1000);

	sys_api::signal_destroy(signal);
}

//-------------------------------------------------------------------------------------
TEST_CASE("Signal timeout precision test", "[Signal][Timeout]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::signal_t signal = sys_api::signal_create();

	// Test different timeout values
	int32_t timeouts[] = { 10, 50, 100, 500 };
	const size_t timeout_count = sizeof(timeouts) / sizeof(timeouts[0]);

	for (size_t i = 0; i < timeout_count; i++) {
		int32_t timeout = timeouts[i];

		// Consume any existing signal
		sys_api::signal_timewait(signal, 0);

		int64_t begin_time = sys_api::performance_time_now();
		REQUIRE_FALSE(sys_api::signal_timewait(signal, timeout));
		int64_t end_time = sys_api::performance_time_now();
		int64_t elapsed = (end_time - begin_time) / 1000; // Convert to milliseconds

		// Allow 20% tolerance + 10ms overhead
		int32_t min_time = static_cast<int32_t>(static_cast<float>(timeout) * 0.8f);
		int32_t max_time = static_cast<int32_t>(static_cast<float>(timeout) * 1.2f) + 10;
		CAPTURE(timeout, elapsed);
		REQUIRE_RANGE(elapsed, min_time, max_time);
	}

	sys_api::signal_destroy(signal);
}

//-------------------------------------------------------------------------------------
TEST_CASE("Signal rapid notify and wait", "[Signal][Stress]")
{
	PRINT_CURRENT_TEST_NAME();

	sys_api::signal_t signal = sys_api::signal_create();

	const int32_t iterations = 1000;
	int32_t success_count = 0;

	// Rapid notify/wait cycle
	for (int i = 0; i < iterations; i++) {
		sys_api::signal_notify(signal);
		if (sys_api::signal_timewait(signal, 0)) {
			success_count++;
		}
	}

	// All should succeed (notify before wait)
	REQUIRE_EQ(iterations, success_count);

	// Final wait should timeout
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 10));

	sys_api::signal_destroy(signal);
}

//-------------------------------------------------------------------------------------
struct CompetitorThreadData
{
	int32_t wait_time; //<0 means wait infinity
	sys_api::signal_t signal;
	atomic_int32_t* goal_counts;
	atomic_bool_t* quit_signal;
};

//-------------------------------------------------------------------------------------
static void _competitorThreadFunction(void* param)
{
	CompetitorThreadData* data = (CompetitorThreadData*)param;

	while (data->quit_signal->load() == false)
	{
		if (data->wait_time >= 0)
		{
			//busy wait
			while (true)
			{
				if (sys_api::signal_timewait(data->signal, data->wait_time))
				{
					break;
				}
			}
		}
		else
		{
			//normal wait
			sys_api::signal_wait(data->signal);
		}
		data->goal_counts->fetch_add(1);
	}
	delete data;
}

//-------------------------------------------------------------------------------------
TEST_CASE("Signal multi thread competitor test", "[Signal][Competitor]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t k_thread_counts = 10;
	const int32_t k_test_counts = 16;
	thread_t p_thread_array[k_thread_counts];

	for (int32_t type = 0; type < 4; type++)
	{
		sys_api::signal_t signal = sys_api::signal_create();
		atomic_bool_t quit_signal(false);
		atomic_int32_t goal_counts(0);

		//create competitor threads(normal wait)
		for (int32_t i = 0; i < k_thread_counts; i++)
		{
			CompetitorThreadData* data = new CompetitorThreadData();
			data->signal = signal;
			data->goal_counts = &goal_counts;
			data->quit_signal = &quit_signal;

			switch (type)
			{
			case 0:// type=0: normal wait, 
				data->wait_time = -1;
				break;
			case 1:// type=1: busy wait, 
				data->wait_time = 0;
				break;
			case 2:// type=2: wait 100ms, 
				data->wait_time = 100;
				break;
			case 3:// type=3: mix
			{
				int percent = rand() % 100;
				if (percent < 33)
					data->wait_time = -1;
				else if (percent < 66)
					data->wait_time = 0;
				else
					data->wait_time = 100;
			}
			break;
			}

			p_thread_array[i] = sys_api::thread_create(_competitorThreadFunction, data, nullptr);
		}

		for (int32_t i = 0; i < k_test_counts; i++)
		{
			REQUIRE_EQ(goal_counts.load(), i);

			//notify one thread
			sys_api::signal_notify(signal);

			//wait oneof threads got the signal
			sys_api::thread_sleep(100);
			REQUIRE_EQ(goal_counts.load(), i + 1);
		}

		//stop all threads
		goal_counts = 0;
		quit_signal = true;
		while (goal_counts.load() < k_thread_counts)
		{
			sys_api::signal_notify(signal);
		}

		//wait for all threads to finish
		for (int32_t i = 0; i < k_thread_counts; i++)
		{
			sys_api::thread_join(p_thread_array[i]);
		}
		REQUIRE_EQ(k_thread_counts, goal_counts.load());
		sys_api::signal_destroy(signal);
	}
}

//-------------------------------------------------------------------------------------
struct PingPongThreadData
{
	atomic_int32_t* ready_counts;
	thread_t thread_handle;
	int32_t thread_index;
	sys_api::signal_t signal_ping;
	sys_api::signal_t signal_pong;
	int32_t pong_counts;
	atomic_bool_t* quit_signal;
};

//-------------------------------------------------------------------------------------
static void _pingPongThreadFunction(void* param)
{
	PingPongThreadData* data = (PingPongThreadData*)param;

	data->ready_counts->fetch_add(1);

	while (true)
	{
		sys_api::signal_wait(data->signal_ping);

		data->pong_counts += 1;

		bool time_to_quit = data->quit_signal->load();

		sys_api::signal_notify(data->signal_pong);

		if (time_to_quit) break;
	}
}

//-------------------------------------------------------------------------------------
TEST_CASE("Signal multi thread pingpong test", "[Signal][PingPong]")
{
	PRINT_CURRENT_TEST_NAME();

	const int32_t k_thread_counts = sys_api::get_cpu_counts() < 2 ? 2 : sys_api::get_cpu_counts();
	const int32_t k_ping_counts = 100;

	sys_api::signal_t signal_ping = sys_api::signal_create();
	sys_api::signal_t signal_pong = sys_api::signal_create();
	atomic_bool_t quit_signal(false);
	atomic_int32_t ready_counts(0);

	PingPongThreadData* thread_data_array = new PingPongThreadData[k_thread_counts];
	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		PingPongThreadData& data = thread_data_array[i];
		data.thread_index = i;
		data.ready_counts = &ready_counts;
		data.signal_ping = signal_ping;
		data.signal_pong = signal_pong;
		data.pong_counts = 0;
		data.quit_signal = &quit_signal;

		data.thread_handle = sys_api::thread_create(_pingPongThreadFunction, &data, nullptr);
	}

	while (ready_counts.load() < k_thread_counts); //wait all thread ready

	for (int32_t i = 0; i < k_ping_counts; i++)
	{
		sys_api::signal_notify(signal_ping);
		sys_api::signal_wait(signal_pong);
	}

	int32_t total_pong_counts = 0;
	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		total_pong_counts += thread_data_array[i].pong_counts;
	}
	REQUIRE_EQ(k_ping_counts, total_pong_counts);

	//stop all threads
	quit_signal = true;
	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		sys_api::signal_notify(signal_ping);
		sys_api::signal_wait(signal_pong);
	}
	for (int32_t i = 0; i < k_thread_counts; i++)
	{
		sys_api::thread_join(thread_data_array[i].thread_handle);
	}
	delete[] thread_data_array;

	REQUIRE_FALSE(sys_api::signal_timewait(signal_ping, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal_pong, 0));

	sys_api::signal_destroy(signal_ping);
	sys_api::signal_destroy(signal_pong);
}
