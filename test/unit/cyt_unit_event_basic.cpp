#include <cy_event.h>
#include "cyt_event_fortest.h"

#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
TEST_CASE("EventLooper basic test", "[EventLooper][Basic]")
{
	PRINT_CURRENT_TEST_NAME();

	EventLooper_ForTest looper;
	const size_t default_channel_counts = EventLooper_ForTest::get_DEFAULT_CHANNEL_BUF_COUNTS();

	const auto& channels = looper.get_channel_buf();
	CHECK_CHANNEL_SIZE(0, 0, 0);

	Looper::event_id_t id = looper.register_timer_event(1, nullptr, nullptr);
	CHECK_CHANNEL_SIZE(default_channel_counts, 1, default_channel_counts - 1);

	looper.disable_all(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 0, default_channel_counts - 1);

	looper.enable_read(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 1, default_channel_counts - 1);

	looper.disable_all(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 0, default_channel_counts - 1);

	looper.delete_event(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 0, default_channel_counts);

	std::vector<Looper::event_id_t> id_buffer;
	for (size_t i = 0; i < default_channel_counts; i++) {
		id_buffer.push_back(looper.register_timer_event(1, nullptr, nullptr));
		CHECK_CHANNEL_SIZE(default_channel_counts, i+1, default_channel_counts - i - 1);
	}

	id_buffer.push_back(looper.register_timer_event(1, nullptr, nullptr));
	CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts + 1, default_channel_counts - 1);

	looper.disable_all(id_buffer[id_buffer.size()-1]);
	CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts, default_channel_counts - 1);

	looper.delete_event(id_buffer[id_buffer.size() - 1]);
	CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts, default_channel_counts);

	for (size_t i = 0; i < default_channel_counts; i++) {
		looper.disable_all(id_buffer[i]);
		CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts-i-1, default_channel_counts+i);

		looper.delete_event(id_buffer[i]);
		CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts-i-1, default_channel_counts+i+1);
	}

	CHECK_CHANNEL_SIZE(default_channel_counts * 2, 0, default_channel_counts*2);
}

}
