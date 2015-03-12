/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper_select.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Looper_select::Looper_select()
	: Looper()
	, m_max_read_counts(0)
	, m_max_write_counts(0)
	, m_max_fd(-1)
	, m_active_head(INVALID_EVENT_ID)
{
	FD_ZERO(&m_master_read_fd_set);
	FD_ZERO(&m_master_write_fd_set);
}

//-------------------------------------------------------------------------------------
Looper_select::~Looper_select()
{

}

//-------------------------------------------------------------------------------------
void Looper_select::_poll(int32_t time_out_ms,
	channel_list& readChannelList,
	channel_list& writeChannelList)
{
	if (m_max_fd == -1)
	{
		for (size_t i = 0; i < m_channelBuffer.size(); i++)
		{
			const channel_s& channel = m_channelBuffer[i];
			if ((channel.event & kRead || channel.event & kWrite) && channel.fd > m_max_fd)
				m_max_fd = channel.id;
		}
	}

	m_work_read_fd_set = m_master_read_fd_set;
	m_work_write_fd_set = m_master_write_fd_set;

	int ready = 0;
	if (m_max_read_counts > 0 || m_max_write_counts>0)
	{
		ready = ::select((int)(m_max_fd+1), &m_work_read_fd_set, &m_work_write_fd_set, 0, 0);
	}
	else 
	{
		//empty set, cause busy-loop, it should never happen!
		thread_api::thread_sleep(1);
	}

	int err = (ready < -1) ? errno : 0;

	if (err < 0)
	{
		if(err==
#ifdef CY_SYS_WINDOWS
			WSAENOTSOCK)
#else
			EBADF)
#endif
		{
			//TODO: invalid socket fd, need recheck channel buffer and kickoff some connection
		}
		return;
	}
	
	if (ready == 0)
	{
		//time out
		return;
	}
	else
	{
		for (event_id_t i = m_active_head; i != INVALID_EVENT_ID;)
		{
			channel_s& channel = m_channelBuffer[i];

			if (FD_ISSET(channel.fd, &m_work_read_fd_set))
			{
				assert(channel.event & kRead);
				assert(channel.on_read);

				readChannelList.push_back(&channel);
			}

			if (FD_ISSET(channel.fd, &m_work_write_fd_set))
			{
				assert(channel.event & kWrite);
				assert(channel.on_write);

				writeChannelList.push_back(&channel);
			}

			i = channel.next;
		}
	}
}

//-------------------------------------------------------------------------------------
void Looper_select::_insert_to_active_list(channel_s& channel)
{
	if (channel.active) return;

	if (m_active_head != INVALID_EVENT_ID) {
		channel_s& head = m_channelBuffer[m_active_head];
		head.prev = channel.id;
	}

	channel.next = m_active_head;
	channel.prev = INVALID_EVENT_ID;

	m_active_head = channel.id;
	channel.active = true;
}

//-------------------------------------------------------------------------------------
void Looper_select::_remove_from_active_list(channel_s& channel)
{
	if (!channel.active) return;

	if (channel.next != INVALID_EVENT_ID) {
		channel_s& next = m_channelBuffer[channel.next];
		next.prev = channel.prev;
	}

	if (channel.prev != INVALID_EVENT_ID) {
		channel_s& prev = m_channelBuffer[channel.prev];
		prev.next = channel.next;
	}
	if (channel.id == m_active_head)
		m_active_head = channel.next;

	channel.active = false;
}

//-------------------------------------------------------------------------------------
void Looper_select::_update_channel_add_event(channel_s& channel, event_t event)
{
	if (channel.event == event || event == kNone) return;
	socket_t fd = channel.fd;

	if (((event & kRead) && m_max_read_counts >= FD_SETSIZE)
		|| ((event & kWrite) && m_max_write_counts >= FD_SETSIZE))
	{
		//TODO: log error ,maximum number of descriptors supported by select() is FD_SETSIZE
		return;
	}

	if ((event & kRead) && !(channel.event & kRead) && channel.on_read)
	{
		FD_SET(fd, &m_master_read_fd_set);
		m_max_read_counts++;
		channel.event |= kRead;
		_insert_to_active_list(channel);
		if (m_max_fd == -1 || m_max_fd < fd)  m_max_fd = fd;
	}
	
	if ((event & kWrite) && !(channel.event & kWrite) && channel.on_write)
	{
		FD_SET(fd, &m_master_write_fd_set);
		m_max_write_counts++;
		channel.event |= kWrite;
		_insert_to_active_list(channel);
		if (m_max_fd == -1 || m_max_fd < fd)  m_max_fd = fd;
	}
}

//-------------------------------------------------------------------------------------
void Looper_select::_update_channel_remove_event(channel_s& channel, event_t event)
{
	if ((channel.event & event) == kNone || !channel.active) return;
	socket_t fd = channel.fd;

	if ((event & kRead) && (channel.event & kRead))
	{
		FD_CLR(fd, &m_master_read_fd_set);
		m_max_read_counts--;
		channel.event &= ~kRead;
		if (m_max_fd == fd) { m_max_fd = -1; }
	}
	
	if ((event & kWrite) && (channel.event & kWrite))
	{
		FD_CLR(fd, &m_master_write_fd_set);
		m_max_write_counts--;
		channel.event &= ~kWrite;
		if (m_max_fd == fd) { m_max_fd = -1; }
	}

	if (channel.event == kNone)
	{
		_remove_from_active_list(channel);
	}
}

}

