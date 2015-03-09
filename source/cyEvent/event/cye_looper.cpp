/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper.h"
#include "internal/cye_looper_epoll.h"
#include "internal/cye_looper_select.h"

namespace cyclone
{
namespace event
{

//-------------------------------------------------------------------------------------
Looper::Looper()
	: m_current_thread(thread_api::thread_get_current_id())
	, m_free_head(INVALID_EVENT_ID)
{
}

//-------------------------------------------------------------------------------------
Looper::~Looper()
{
}

//-------------------------------------------------------------------------------------
Looper::event_id_t Looper::register_event(socket_t sockfd,
	event_t event,
	void* param,
	event_callback _on_read,
	event_callback _on_write)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);

	//get a new channel slot
	event_id_t id = _get_free_slot();
	channel_s& channel = m_channelBuffer[id];

	channel.id = id;
	channel.fd = sockfd;
	channel.event = 0;
	channel.param = param;
	channel.active = false;
	channel.on_read = _on_read;
	channel.on_write = _on_write;

	//update to poll
	if (event != kNone)
		_update_channel_add_event(channel, event);
	return id;
}

//-------------------------------------------------------------------------------------
void Looper::delete_event(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	//pool it 
	channel_s& channel = m_channelBuffer[id];
	assert(channel.event == kNone && channel.active == false); //should be disabled already
	
	//remove from active list to free list
	//if (channel.)
	channel.next = m_free_head;
	m_free_head = id;
}

//-------------------------------------------------------------------------------------
void Looper::disable_read(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kRead);
}

//-------------------------------------------------------------------------------------
void Looper::enable_read(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_add_event(channel, kRead);
}

//-------------------------------------------------------------------------------------
bool Looper::is_read(event_id_t id) const
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	const channel_s& channel = m_channelBuffer[id];
	return (channel.event & kRead)!=0;
}

//-------------------------------------------------------------------------------------
void Looper::disable_write(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
void Looper::enable_write(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_add_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
bool Looper::is_write(event_id_t id) const
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	const channel_s& channel = m_channelBuffer[id];
	return (channel.event & kWrite)!=0;
}

//-------------------------------------------------------------------------------------
void Looper::disable_all(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert(id >= 0 && id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kRead|kWrite);
}

//-------------------------------------------------------------------------------------
void Looper::loop(void)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);

	channel_list readList;
	channel_list writeList;

	for (;;)
	{
		readList.clear();
		writeList.clear();

		//wait in kernel...
		_poll(0, readList, writeList);
		
		//reactor
		for (size_t i = 0; i < readList.size(); i++)
		{
			channel_s* c = readList[i];
			if (c->on_read == 0) continue;

			c->on_read(c->id, c->fd, kRead, c->param);
		}

		for (size_t i = 0; i < writeList.size(); i++)
		{
			channel_s* c = writeList[i];
			if (c->on_write == 0) continue;

			c->on_write(c->id, c->fd, kWrite, c->param);
		}
	}
}

//-------------------------------------------------------------------------------------
Looper::event_id_t Looper::_get_free_slot(void)
{
	do {
		if (m_free_head != INVALID_EVENT_ID) {
			event_id_t id = m_free_head;

			channel_s& channel = m_channelBuffer[m_free_head];
			m_free_head = channel.next;

			return id;
		}

		//need alloc more space
		size_t old_size = m_channelBuffer.size();
		size_t new_size = (old_size == 0) ? DEFAULT_CHANNEL_BUF_COUNTS : old_size * 2;

		m_channelBuffer.reserve(new_size);

		for (size_t i = old_size; i < new_size; i++)
		{
			channel_s channel;
			memset(&channel, 0, sizeof(channel));

			channel.id = (event_id_t)i;
			channel.next = m_free_head;
			m_free_head = channel.id;
			m_channelBuffer.push_back(channel);
		}
		//try again now...
	} while (true);
}

}
}
