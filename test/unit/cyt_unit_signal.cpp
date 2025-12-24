#include <cy_core.h>
#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {
//-------------------------------------------------------------------------------------
TEST_CASE("Basic test for Signal", "[Signal]") 
{
	sys_api::signal_t signal = sys_api::signal_create();

	//wait
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 1));

	//notify, wait and timewait
	sys_api::signal_notify(signal);
	sys_api::signal_wait(signal);
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 1));

	//notify and time_wait twice
	sys_api::signal_notify(signal);
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 1));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 0));

	//notify twice and time_wait twice
	sys_api::signal_notify(signal);
	sys_api::signal_notify(signal);
	REQUIRE_TRUE(sys_api::signal_timewait(signal, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 1));

	//time_wait
	int64_t begin_time = sys_api::performance_time_now();
	REQUIRE_FALSE(sys_api::signal_timewait(signal, 100));
	int64_t end_time = sys_api::performance_time_now();
	REQUIRE_GE(end_time - begin_time, 100*1000);
	REQUIRE_LE(end_time - begin_time, 120*1000);


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
TEST_CASE("Multithread test for Signal", "[Signal]")
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

	REQUIRE_EQ(thread_counts, live_counts);

	while (live_counts>0) {
		int32_t current_live_counts = live_counts;
		sys_api::signal_notify(signal_ping);
		sys_api::signal_wait(signal_pong);
		REQUIRE_EQ(current_live_counts - 1, live_counts.load());
	}

	REQUIRE_EQ(0, live_counts);
	REQUIRE_FALSE(sys_api::signal_timewait(signal_ping, 0));
	REQUIRE_FALSE(sys_api::signal_timewait(signal_pong, 0));

	sys_api::signal_destroy(signal_ping);
	sys_api::signal_destroy(signal_pong);
}

}
