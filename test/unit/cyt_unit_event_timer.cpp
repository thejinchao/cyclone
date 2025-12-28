#include <cy_event.h>
#include "cyt_event_fortest.h"

#include "cyt_unit_utils.h"

using namespace cyclone;

#define MAX_TIMER_ERROR	(20)		//20ms

namespace {

//-------------------------------------------------------------------------------------
struct ThreadData
{
	EventLooper_ForTest* looper;
	atomic_int32_t counts;
	int32_t break_counts;
	int32_t pause_counts;
	sys_api::signal_t begin_signal;
	sys_api::signal_t end_signal;
	sys_api::signal_t break_signal;
	uint32_t freq;
	Looper::event_id_t timer_id;
};

//-------------------------------------------------------------------------------------
static void _timerFunction(Looper::event_id_t id, void* param)
{
	(void)id;

	ThreadData* data = (ThreadData*)param;

	data->counts++;

	if (sys_api::signal_timewait(data->break_signal, 0) || 
		((data->break_counts > 0) && (data->counts) >= (data->break_counts))) {
		sys_api::signal_notify(data->end_signal);
		//push loop quit command
		data->looper->push_stop_request();
		return;
	}
	
	if ((data->pause_counts > 0) && (data->counts >= data->pause_counts)) {
		data->looper->disable_all(id);
	}
}

//-------------------------------------------------------------------------------------
static void _threadFunction(void* param)
{
	ThreadData* data = (ThreadData*)param;
	EventLooper_ForTest* looper = new EventLooper_ForTest();
	data->looper = looper;

	data->timer_id = looper->register_timer_event(data->freq, param, _timerFunction);
	sys_api::signal_notify(data->begin_signal);

	looper->loop();

	delete looper;
	data->looper = nullptr;
}

//-------------------------------------------------------------------------------------
struct MultiTimerData
{
	Looper::event_id_t id;
	uint32_t freq;
	uint32_t counts;
};

//-------------------------------------------------------------------------------------
struct MultiTimerThreadData
{
	EventLooper_ForTest* looper;
	std::vector< MultiTimerData > timers; 
	sys_api::signal_t begin_signal;
	sys_api::signal_t pause_signal;
	sys_api::signal_t resume_signal;
};

//-------------------------------------------------------------------------------------
static void _multiTimerFunction(Looper::event_id_t id, void* param)
{
	(void)id;

	MultiTimerData* data = (MultiTimerData*)param;

	data->counts++;
}

//-------------------------------------------------------------------------------------
static void _multiTimerThreadFunction(void* param)
{
	MultiTimerThreadData* data = (MultiTimerThreadData*)param;
	EventLooper_ForTest* looper = new EventLooper_ForTest();
	data->looper = looper;

	for (size_t i = 0; i < data->timers.size(); i++) {
		MultiTimerData& timer_data = data->timers[i];
		timer_data.id = looper->register_timer_event(timer_data.freq, &(timer_data), _multiTimerFunction);
	}
	sys_api::signal_notify(data->begin_signal);

	for (;;) {
		looper->step();

		if (looper->is_quit_pending()) break;
		if (sys_api::signal_timewait(data->pause_signal, 0)) {
			sys_api::signal_wait(data->resume_signal);
		}
	}

	delete looper;
	data->looper = nullptr;
}

//-------------------------------------------------------------------------------------
TEST_CASE("Timer test for EventLooper", "[EventLooper][Timer]")
{
	PRINT_CURRENT_TEST_NAME();

	ThreadData data;
	data.begin_signal = sys_api::signal_create();
	data.end_signal = sys_api::signal_create();
	data.break_signal = sys_api::signal_create();

	//timer once
	{
		data.counts = 0;
		data.break_counts = 1;
		data.pause_counts = -1;
		data.freq = 500;

		thread_t thread = sys_api::thread_create(_threadFunction, &data, "timer");

		sys_api::signal_wait(data.begin_signal);
		int64_t begin_time = sys_api::performance_time_now();

		sys_api::signal_wait(data.end_signal);
		int64_t end_time = sys_api::performance_time_now();

		REQUIRE_EQ(1, data.counts.load());
		REQUIRE_GE(end_time - begin_time, (data.freq - MAX_TIMER_ERROR) * 1000ll);
		REQUIRE_LE(end_time - begin_time, (data.freq + MAX_TIMER_ERROR) * 1000ll);
		sys_api::thread_join(thread);
	}

	//timer more than once
	{
		data.counts = 0;
		data.break_counts = -1;
		data.pause_counts = -1;
		data.freq = 89;

		const int repeat_times = 7;

		thread_t thread = sys_api::thread_create(_threadFunction, &data, "timer");

		sys_api::signal_wait(data.begin_signal);
		int64_t begin_time = sys_api::performance_time_now();

		uint32_t sleep_time = data.freq * repeat_times + MAX_TIMER_ERROR;
		sys_api::thread_sleep((int32_t)sleep_time);
		int32_t current_index = data.counts.load();

		sys_api::signal_notify(data.break_signal);
		REQUIRE_GE(current_index, repeat_times);
		REQUIRE_LE(current_index, repeat_times+1);
		sys_api::thread_join(thread);
		int64_t end_time = sys_api::performance_time_now();
		REQUIRE_GE(end_time - begin_time, sleep_time * 1000ll);
		REQUIRE_LE(end_time - begin_time, (int64_t)(sleep_time + data.freq + MAX_TIMER_ERROR) * 1000ll);
	}

	//timer then pause/resume
	{
		data.counts = 0;
		data.break_counts = -1;
		data.pause_counts = 5;
		data.freq = 83;

		thread_t thread = sys_api::thread_create(_threadFunction, &data, "timer");

		sys_api::signal_wait(data.begin_signal);

		uint32_t sleep_time = data.freq * ((uint32_t)data.pause_counts+2) + MAX_TIMER_ERROR;
		sys_api::thread_sleep((int32_t)sleep_time);
		REQUIRE_EQ(data.counts.load(), data.pause_counts);

		sys_api::thread_sleep(MAX_TIMER_ERROR);
		REQUIRE_EQ(data.counts.load(), data.pause_counts);

		//resume
		data.pause_counts = data.pause_counts*2;
		data.looper->enable_read(data.timer_id);
		sys_api::thread_sleep((int32_t)sleep_time);
		REQUIRE_EQ(data.counts.load(), data.pause_counts);

		//stop thread
		data.looper->push_stop_request();
		sys_api::thread_join(thread);
	}

	sys_api::signal_destroy(data.begin_signal);
	sys_api::signal_destroy(data.end_signal);
	sys_api::signal_destroy(data.break_signal);
}

//-------------------------------------------------------------------------------------
TEST_CASE("MultiTimer test for EventLooper", "[EventLooper][MultiTimer]")
{
	PRINT_CURRENT_TEST_NAME();

	MultiTimerThreadData data;
	data.begin_signal = sys_api::signal_create();
	data.pause_signal = sys_api::signal_create();
	data.resume_signal = sys_api::signal_create();

	{
		data.timers.push_back(MultiTimerData{ 0, 17, 0 });
		data.timers.push_back(MultiTimerData{ 0, 23, 0 });
		data.timers.push_back(MultiTimerData{ 0, 47, 0 });
		data.timers.push_back(MultiTimerData{ 0, 97, 0 });
		const int32_t sleep_time = 500;
		const size_t disable_timer_index = (size_t)rand()%data.timers.size();
		uint32_t disabled_timer_counts = 0;

		thread_t thread = sys_api::thread_create(_multiTimerThreadFunction, &data, "timer");

		//fly some time
		sys_api::signal_wait(data.begin_signal);
		sys_api::thread_sleep(sleep_time);

		//pause
		sys_api::signal_notify(data.pause_signal);

		//check
		const size_t default_channel_counts = EventLooper_ForTest::get_DEFAULT_CHANNEL_BUF_COUNTS();
		const auto& looper = *(data.looper);
		const auto& channels = data.looper->get_channel_buf();
		CHECK_CHANNEL_SIZE(default_channel_counts, data.timers.size(), default_channel_counts - data.timers.size());

		for (size_t i = 0; i < data.timers.size(); i++) {
			MultiTimerData& timer = data.timers[i];

			CAPTURE(timer.freq, timer.counts, sleep_time, sleep_time / timer.freq);

			REQUIRE_GE(timer.counts, sleep_time / timer.freq - 1);
			REQUIRE_LE(timer.counts, sleep_time / timer.freq + 1);

			if (i == disable_timer_index) {
				disabled_timer_counts = timer.counts;
			}
		}

		//pause one of timer
		data.looper->disable_all(data.timers[disable_timer_index].id);
		CHECK_CHANNEL_SIZE(default_channel_counts, data.timers.size()-1, default_channel_counts - data.timers.size());

		//resume and fly continue
		sys_api::signal_notify(data.resume_signal);
		sys_api::thread_sleep(sleep_time);

		//pause and check
		sys_api::signal_notify(data.pause_signal);
		for (size_t i = 0; i < data.timers.size(); i++) {
			MultiTimerData& timer = data.timers[i];

			if (i == disable_timer_index) {
				REQUIRE_EQ(disabled_timer_counts, timer.counts);
			}
			else {
				CAPTURE(timer.freq, timer.counts, timer.freq * timer.counts, sleep_time * 2, 2 * sleep_time / timer.freq);
				REQUIRE_GE(timer.counts, (uint32_t)(2 * sleep_time / timer.freq - 1));
				REQUIRE_LE(timer.counts, (uint32_t)(2 * sleep_time / timer.freq + 1));
			}
		}

		//stop thread
		sys_api::signal_notify(data.resume_signal);
		data.looper->push_stop_request();
		sys_api::thread_join(thread);
	}

	sys_api::signal_destroy(data.begin_signal);
	sys_api::signal_destroy(data.pause_signal);
	sys_api::signal_destroy(data.resume_signal);
}

}

