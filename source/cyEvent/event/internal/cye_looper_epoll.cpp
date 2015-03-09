/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper_epoll.h"

namespace cyclone
{

#ifdef CY_HAVE_EPOLL

//-------------------------------------------------------------------------------------
Looper_epoll::Looper_epoll()
	: Looper()
	, m_eoll_fd(::epoll_create1(EPOLL_CLOEXEC))
	, m_events(DEFAULT_CHANNEL_BUF_COUNTS)
{
}

//-------------------------------------------------------------------------------------
Looper_epoll::~Looper_epoll()
{
	::close(m_eoll_fd);
}

//-------------------------------------------------------------------------------------
void Looper_epoll::_poll(int32_t time_out_ms,
	channel_list& readChannelList,
	channel_list& writeChannelList)
{

	int num_events = 0;
	do {
		num_events = ::epoll_wait(m_eoll_fd,
			&*m_events.begin(), static_cast<int>(m_events.size()),
			-1);
	}while (num_events < 0 && errno == EINTR); //gdb may cause interrupted system call

	if (num_events < 0)
	{
		//TODO: error log something...
		return;
	}
	else if (num_events == 0)
	{
		//
		return;
	}

	//fill active channels
	for (int i = 0; i < num_events; i++)
	{
		const epoll_event& event = m_events[i];
		uint32_t revents = event.events;
		channel_s* channel = (channel_s*)event.data.ptr;

		if (revents & (EPOLLERR | EPOLLHUP)) {
			//TODO: error fd, log something...
		}

		if ((revents & (EPOLLERR | EPOLLHUP)) 
			&& (revents & (EPOLLIN | EPOLLOUT)) == 0)
		{
			/*
			* if the error events were returned without EPOLLIN or EPOLLOUT,
			* then add these flags to handle the events at least in one
			* active handler @nginx
			*/
			revents |= EPOLLIN | EPOLLOUT;
		}

		if ((revents & EPOLLIN) && channel->active && channel->on_read != 0)
		{
			//read event
			readChannelList.push_back(channel);
		}
		
		if ((revents & EPOLLOUT) && channel->active && channel->on_write != 0)
		{
			//read event
			writeChannelList.push_back(channel);
		}
	}
	
	if ((size_t)num_events == m_events.size())
	{
		m_events.resize(m_events.size() * 2);
	}
}

//-------------------------------------------------------------------------------------
bool Looper_epoll::_set_event(channel_s& channel, int operation, uint32_t events)
{
	//int operation = channel.active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	struct epoll_event event;
	memset(&event, 0, sizeof(event));
	if (operation != EPOLL_CTL_DEL)
	{
		event.events =  events;
		event.data.ptr = &channel;
	}

	if (::epoll_ctl(m_eoll_fd, operation, channel.fd, &event) < 0) {
		//TODO: log something...
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
void Looper_epoll::_update_channel_add_event(channel_s& channel, event_t event)
{
	if (channel.event == event || event == kNone) return;

	uint32_t event_to_set = 0;

	if (((event & kRead) || (channel.event & kRead)) && channel.on_read)
		event_to_set |= (EPOLLIN | EPOLLRDHUP);

	if (((event & kWrite) || (channel.event & kWrite)) && channel.on_write)
		event_to_set |= EPOLLOUT;

	int operation = channel.active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	if (_set_event(channel, operation, event_to_set))
	{
		channel.event |= event;
		channel.active = true;
	}
}

//-------------------------------------------------------------------------------------
void Looper_epoll::_update_channel_remove_event(channel_s& channel, event_t event)
{
	if (channel.event & event == kNone || !channel.active) return;
	uint32_t event_to_set = 0;

	if ((channel.event & kRead) && !(event & kRead) && channel.on_read)
		event_to_set |= (EPOLLIN | EPOLLRDHUP);

	if ((channel.event & kWrite) && !(event & kWrite) && channel.on_write)
		event_to_set |= EPOLLOUT;

	if (event_to_set!=0)
	{
		if(_set_event(channel, EPOLL_CTL_MOD, event_to_set))
		{
			channel.event &= ~event;
		}
	}
	else
	{
		if (_set_event(channel, EPOLL_CTL_DEL, 0))
		{
			channel.event = kNone;
			channel.active = false;
		}
	}
}

#else
//avoid MSVC LNK4221 WARNING
void nothing(void){}
#endif

}
