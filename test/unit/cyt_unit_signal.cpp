#include <cy_core.h>
#include <gtest/gtest.h>

using namespace cyclone;

namespace {
//-------------------------------------------------------------------------------------
TEST(Signal, Basic)
{
	sys_api::signal_t signal = sys_api::signal_create();

	//wait
	EXPECT_FALSE(sys_api::signal_timewait(signal, 0));
	EXPECT_FALSE(sys_api::signal_timewait(signal, 1));

	//notify, wait and timewait
	sys_api::signal_notify(signal);
	sys_api::signal_wait(signal);
	EXPECT_FALSE(sys_api::signal_timewait(signal, 1));

	//notify and time_wait twice
	sys_api::signal_notify(signal);
	EXPECT_TRUE(sys_api::signal_timewait(signal, 1));
	EXPECT_FALSE(sys_api::signal_timewait(signal, 0));

	//notify twice and time_wait twice
	sys_api::signal_notify(signal);
	sys_api::signal_notify(signal);
	EXPECT_TRUE(sys_api::signal_timewait(signal, 0));
	EXPECT_FALSE(sys_api::signal_timewait(signal, 1));

	//time_wait
	int64_t begin_time = sys_api::performance_time_now();
	EXPECT_FALSE(sys_api::signal_timewait(signal, 100));
	int64_t end_time = sys_api::performance_time_now();
	EXPECT_GE(end_time - begin_time, 100*1000);
	EXPECT_LE(end_time - begin_time, 110*1000);


	sys_api::signal_destroy(signal);
}

//-------------------------------------------------------------------------------------
struct ThreadData
{
	sys_api::signal_t signal_ping;
	sys_api::signal_t signal_pong;
	atomic_int32_t* live_counts;
};

//-------------------------------------------------------------------------------------
static void _threadFunction(void* param)
{
	ThreadData* data = (ThreadData*)param;

	sys_api::signal_wait(data->signal_ping);
	data->live_counts->fetch_sub(1);
	sys_api::signal_notify(data->signal_pong);

	delete data;
}

//-------------------------------------------------------------------------------------
TEST(Signal, Multithread)
{
	const int32_t thread_counts = 10;

	sys_api::signal_t signal_ping = sys_api::signal_create();
	sys_api::signal_t signal_pong = sys_api::signal_create();
	atomic_int32_t live_counts(0);

	for (int32_t i = 0; i < thread_counts; i++) {
		ThreadData* data = new ThreadData();
		data->signal_ping = signal_ping;
		data->signal_pong = signal_pong;
		data->live_counts = &live_counts;

		sys_api::thread_create_detached(_threadFunction, data, "");

		live_counts++;
	}

	EXPECT_EQ(thread_counts, live_counts);

	while (live_counts>0) {
		int32_t current_live_counts = live_counts;
		sys_api::signal_notify(signal_ping);
		sys_api::signal_wait(signal_pong);
		EXPECT_EQ(current_live_counts - 1, live_counts.load());
	}

	EXPECT_EQ(0, live_counts);
	EXPECT_FALSE(sys_api::signal_timewait(signal_ping, 0));
	EXPECT_FALSE(sys_api::signal_timewait(signal_pong, 0));

	sys_api::signal_destroy(signal_ping);
	sys_api::signal_destroy(signal_pong);
}

}
