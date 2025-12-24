#include <cy_event.h>

#include <event/internal/cye_looper_epoll.h>
#include <event/internal/cye_looper_select.h>
#include <event/internal/cye_looper_kqueue.h>

//-------------------------------------------------------------------------------------
class EventLooper_ForTest : public
#if (CY_POLL_TECH==CY_POLL_EPOLL)
	cyclone::Looper_epoll
#elif (CY_POLL_TECH == CY_POLL_KQUEUE)
	cyclone::Looper_kqueue
#else
	cyclone::Looper_select
#endif
{
public:
	typedef std::vector< channel_s > channel_buffer;

public:
	//functions for test
	const channel_buffer& get_channel_buf(void) const { return m_channelBuffer; }
	event_id_t get_free_head(void) const { return m_free_head; }
	int32_t get_active_channel_counts(void) const { return m_active_channel_counts; }
	int32_t get_active_channel_list_counts(void) const {
#if (CY_POLL_TECH==CY_POLL_SELECT)
		int32_t counts = 0;
		event_id_t current = m_active_head;
		while (current != INVALID_EVENT_ID) {
			counts++;
			current = m_channelBuffer[current].next;
		}
		return counts;
#else
		return m_active_channel_counts;
#endif
	}
	int32_t get_free_channel_counts(void) const {
		int32_t counts = 0;
		event_id_t current = m_free_head;
		while (current != INVALID_EVENT_ID) {
			counts++;
			current = m_channelBuffer[current].next;
		}
		return counts;
	}
	static size_t get_DEFAULT_CHANNEL_BUF_COUNTS(void) { return DEFAULT_CHANNEL_BUF_COUNTS; }

	void reset_loop_counts(void) { m_loop_counts = 0; }
};

//-------------------------------------------------------------------------------------
#define CHECK_CHANNEL_SIZE(c, a, f) \
	REQUIRE_EQ((size_t)(c), channels.size()); \
	REQUIRE_EQ((int32_t)(a), looper.get_active_channel_counts()); \
	REQUIRE_EQ((int32_t)(a), looper.get_active_channel_list_counts()); \
	REQUIRE_EQ((int32_t)(f), looper.get_free_channel_counts()); 

