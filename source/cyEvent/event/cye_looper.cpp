/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper.h"
#include "internal/cye_looper_epoll.h"
#include "internal/cye_looper_select.h"

#ifndef CY_SYS_WINDOWS
#include <sys/timerfd.h>
#endif

namespace cyclone
{

//-------------------------------------------------------------------------------------
const Looper::event_id_t Looper::INVALID_EVENT_ID = (Looper::event_id_t)(~0);

//-------------------------------------------------------------------------------------
Looper::Looper()
	: m_free_head(INVALID_EVENT_ID)
	, m_current_thread(thread_api::thread_get_current_id())
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
	channel.timer = false;
	channel.on_read = _on_read;
	channel.on_write = _on_write;

	//update to poll
	if (event != kNone)
		_update_channel_add_event(channel, event);
	return id;
}

//-------------------------------------------------------------------------------------
Looper::event_id_t Looper::register_timer_event(uint32_t milliSeconds,
	void* param,
	timer_callback _on_timer)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);

	//get a new channel slot
	event_id_t id = _get_free_slot();
	channel_s& channel = m_channelBuffer[id];

	timer_s* timer = new timer_s();
	timer->on_timer = _on_timer;
	timer->param = param;

#ifdef CY_SYS_WINDOWS
	channel.id = id;
	channel.fd = timer->pipe.get_read_port();
	channel.event = 0;
	channel.param = timer;
	channel.active = false;
	channel.timer = true;
	channel.on_read = _on_timer_event_callback;
	channel.on_write = 0;

	//create mmsystem timer
	timer->winmm_timer_id = ::timeSetEvent(milliSeconds, 1, 
		_on_windows_timer, (DWORD_PTR)(timer->pipe.get_write_port()), 
		TIME_CALLBACK_FUNCTION|TIME_PERIODIC);
#else
	channel.id = id;
	channel.event = 0;
	channel.param = timer;
	channel.active = false;
	channel.timer = true;
	channel.on_read = _on_timer_event_callback;
	channel.on_write = 0;
	channel.fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	if (channel.fd < 0) {
		//TODO: error
	}

	struct itimerspec newValue;
	struct itimerspec oldValue;
	memset(&newValue, 0, sizeof(newValue));
	memset(&oldValue, 0, sizeof(oldValue));

	struct timespec ts;
	ts.tv_sec = milliSeconds / 1000;
	ts.tv_nsec = (milliSeconds%1000) * 1000*1000;

	newValue.it_value = ts;
	newValue.it_interval.tv_sec=1; //set non-zero for repeated timer
	::timerfd_settime(channel.fd, 0, &newValue, &oldValue);

#endif

	//add kRead event to poll
	_update_channel_add_event(channel, kRead);
	return id;
}

//-------------------------------------------------------------------------------------
void Looper::delete_event(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	//unpool it 
	channel_s& channel = m_channelBuffer[id];
	assert(channel.event == kNone && channel.active == false); //should be disabled already

	//if timer event
	if (channel.timer) {
		timer_s* timer = (timer_s*)channel.param;
#ifdef CY_SYS_WINDOWS
		::timeKillEvent(timer->winmm_timer_id);
#else
		socket_api::close_socket(channel.fd);
#endif
		delete timer;
	}
	
	//remove from active list to free list
	channel.next = m_free_head;
	m_free_head = id;
}

//-------------------------------------------------------------------------------------
void Looper::disable_read(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kRead);
}

//-------------------------------------------------------------------------------------
void Looper::enable_read(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_add_event(channel, kRead);
}

//-------------------------------------------------------------------------------------
bool Looper::is_read(event_id_t id) const
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	const channel_s& channel = m_channelBuffer[id];
	return (channel.event & kRead)!=0;
}

//-------------------------------------------------------------------------------------
void Looper::disable_write(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
void Looper::enable_write(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_add_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
bool Looper::is_write(event_id_t id) const
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

	const channel_s& channel = m_channelBuffer[id];
	return (channel.event & kWrite)!=0;
}

//-------------------------------------------------------------------------------------
void Looper::disable_all(event_id_t id)
{
	assert(thread_api::thread_get_current_id() == m_current_thread);
	assert((size_t)id < m_channelBuffer.size());

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
		_poll(readList, writeList);
		
		bool quit_cmd = false;

		//reactor
		for (size_t i = 0; i < readList.size(); i++)
		{
			channel_s* c = readList[i];
			if (c->on_read == 0 || (c->event & kRead) == 0) continue;

			if (c->on_read(c->id, c->fd, kRead, c->param)) {
				quit_cmd = true;
			}
		}

		for (size_t i = 0; i < writeList.size(); i++)
		{
			channel_s* c = writeList[i];
			if (c->on_write == 0 || (c->event & kWrite) == 0) continue;

			if (c->on_write(c->id, c->fd, kWrite, c->param)){
				quit_cmd = true;
			}
		}

		//it's the time to shutdown everything...
		if (quit_cmd) break;
	}
}

//-------------------------------------------------------------------------------------
Looper::event_id_t Looper::_get_free_slot(void)
{
	for (;;)
	{
		if (m_free_head != INVALID_EVENT_ID) {
			event_id_t id = m_free_head;

			channel_s& channel = m_channelBuffer[m_free_head];
			m_free_head = channel.next;

			return id;
		}

		//need alloc more space
		size_t old_size = m_channelBuffer.size();
		size_t new_size = (old_size == 0) ? ((size_t)DEFAULT_CHANNEL_BUF_COUNTS) : (old_size * 2);

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
	}
}

#ifdef CY_SYS_WINDOWS
//-------------------------------------------------------------------------------------
void Looper::_on_windows_timer(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	(void)wTimerID;
	(void)msg;
	(void)dw1;
	(void)dw2;

	socket_t sfd = (socket_t)dwUser;
	uint64_t touch = 0;
	socket_api::write(sfd, (const char*)(&touch), sizeof(touch));
}
#endif

//-------------------------------------------------------------------------------------
bool Looper::_on_timer_event_callback(event_id_t id, socket_t fd, event_t event, void* param)
{
	(void)event;

	timer_s* timer = (timer_s*)param;

	uint64_t touch = 0;
	socket_api::read(fd, &touch, sizeof(touch));

	if (timer->on_timer) {
		return timer->on_timer(id, timer->param);
	}
	return false;
}

}

