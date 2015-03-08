/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_LOOPER_SELECT_H_
#define _CYCLONE_EVENT_LOOPER_SELECT_H_

#include <cy_core.h>
#include <event/cye_looper.h>

namespace cyclone
{
namespace event
{

class Looper_select : public Looper
{
protected:
	virtual void _poll(int32_t time_out_ms,
		channel_list& readChannelList,
		channel_list& writeChannelList,
		channel_list& errorChannelList);
	/// Changes the interested I/O events.
	virtual void _update_channel_add_event(channel_s& channel, event_t type);
	virtual void _update_channel_remove_event(channel_s& channel, event_t type);
	/// Remove the channel, when it destructs.
	virtual void _remove_channel(channel_s& channel);

private:
	fd_set	m_master_read_fd_set;
	fd_set	m_master_write_fd_set;

	fd_set	m_work_read_fd_set;
	fd_set	m_work_write_fd_set;

	int32_t m_max_read_counts;
	int32_t m_max_write_counts;

	socket_t m_max_fd;  //TODO: disable max_fd at windows platform
public:
	Looper_select();
	virtual ~Looper_select();
};

}
}

#endif
