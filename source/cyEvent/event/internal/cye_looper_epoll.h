/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cy_core.h>
#include <event/cye_looper.h>

#ifdef CY_HAVE_EPOLL
#include <sys/epoll.h>

namespace cyclone
{

class Looper_epoll : public Looper
{
public:
	/// Polls the I/O events.
	virtual void _poll( 
		channel_list& readChannelList,
		channel_list& writeChannelList,
		bool block) override;
	/// Changes the interested I/O events.
	virtual void _update_channel_add_event(channel_s& channel, event_t event) override;
	virtual void _update_channel_remove_event(channel_s& channel, event_t event) override;

private:
	typedef std::vector<struct epoll_event> event_vector;

	event_vector m_events;
	int m_eoll_fd;

private:
	bool _set_event(channel_s& channel, int operation, uint32_t events);

public:
	Looper_epoll();
	virtual ~Looper_epoll() override;
};

}

#endif
