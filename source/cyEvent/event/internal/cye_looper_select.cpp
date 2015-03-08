/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper_select.h"

namespace cyclone
{
namespace event
{

//-------------------------------------------------------------------------------------
Looper_select::Looper_select()
	: Looper()
	, m_max_read_counts(0)
	, m_max_write_counts(0)
	, m_max_fd(-1)
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
	channel_list& writeChannelList,
	channel_list& errorChannelList)
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
		for (size_t i = 0; i < m_channelBuffer.size(); i++)
		{
			channel_s& channel = m_channelBuffer[i];

			if ((channel.event & kRead) && channel.on_read != 0 && FD_ISSET(channel.fd, &m_work_read_fd_set))
			{
				readChannelList.push_back(&channel);
			}
			if ((channel.event & kWrite) && channel.on_write != 0 && FD_ISSET(channel.fd, &m_work_write_fd_set))
			{
				writeChannelList.push_back(&channel);
			}
		}
	}
}

//-------------------------------------------------------------------------------------
void Looper_select::_update_channel_add_event(channel_s& channel, event_t event)
{
	socket_t fd = channel.fd;

	if ((event == kRead && m_max_read_counts >= FD_SETSIZE)
		|| (event == kWrite && m_max_write_counts >= FD_SETSIZE))
	{
		//TODO: log error ,maximum number of descriptors supported by select() is FD_SETSIZE
		return;
	}

	if (event == kRead && !(channel.event & kRead) && channel.on_read)
	{
		FD_SET(fd, &m_master_read_fd_set);
		m_max_read_counts++;
		channel.event |= kRead;
		if (m_max_fd == -1 || m_max_fd < fd)  m_max_fd = fd;
	}
	else if (event == kWrite && !(channel.event & kWrite) && channel.on_write)
	{
		FD_SET(fd, &m_master_write_fd_set);
		m_max_write_counts++;
		channel.event |= kWrite;
		if (m_max_fd == -1 || m_max_fd < fd)  m_max_fd = fd;
	}
	else
	{
		//TODO: log error, select event is already in set or callback is null
		return;
	}
}

//-------------------------------------------------------------------------------------
void Looper_select::_update_channel_remove_event(channel_s& channel, event_t event)
{
	socket_t fd = channel.fd;

	if (event == kRead && (channel.event & kRead))
	{
		FD_CLR(fd, &m_master_read_fd_set);
		m_max_read_counts--;
		channel.event &= ~kRead;
		if (m_max_fd == fd) { m_max_fd = -1; }
	}
	else if (event == kWrite && (channel.event & kWrite))
	{
		FD_CLR(fd, &m_master_write_fd_set);
		m_max_write_counts--;
		channel.event &= ~kWrite;
		if (m_max_fd == fd) { m_max_fd = -1; }
	}
	else
	{
		//TODO: log error, select event is not in set 
	}
}

//-------------------------------------------------------------------------------------
void Looper_select::_remove_channel(channel_s& channel)
{

}

}
}
