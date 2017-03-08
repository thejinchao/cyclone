/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper.h"
#include "internal/cye_looper_epoll.h"
#include "internal/cye_looper_select.h"

#ifdef CY_SYS_LINUX
#include <sys/timerfd.h>
#endif

namespace cyclone
{

//-------------------------------------------------------------------------------------
const Looper::event_id_t Looper::INVALID_EVENT_ID = (Looper::event_id_t)(~0);

//-------------------------------------------------------------------------------------
Looper::Looper()
	: m_free_head(INVALID_EVENT_ID)
	, m_active_channel_counts(0)
	, m_loop_counts(0)
	, m_current_thread(sys_api::thread_get_current_id())
	, m_inner_pipe(0)
	, m_inner_pipe_touched(0)
	, m_quit_cmd(0)
{
	m_lock = sys_api::mutex_create();
}

//-------------------------------------------------------------------------------------
Looper::~Looper()
{
	sys_api::mutex_destroy(m_lock);
}

//-------------------------------------------------------------------------------------
Looper::event_id_t Looper::register_event(socket_t sockfd,
	event_t event,
	void* param,
	event_callback _on_read,
	event_callback _on_write)
{
	assert(sys_api::thread_get_current_id() == m_current_thread);
	sys_api::auto_mutex lock(m_lock);

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
	if ((event & kRead) != 0)
		_update_channel_add_event(channel, kRead);
    if ((event & kWrite) != 0)
        _update_channel_add_event(channel, kWrite);
    
	return id;
}

//-------------------------------------------------------------------------------------
Looper::event_id_t Looper::register_timer_event(uint32_t milliSeconds,
	void* param,
	timer_callback _on_timer)
{
	assert(sys_api::thread_get_current_id() == m_current_thread);
	sys_api::auto_mutex lock(m_lock);

	//get a new channel slot
	event_id_t id = _get_free_slot();
	channel_s& channel = m_channelBuffer[id];

	timer_s* timer = new timer_s();
	timer->on_timer = _on_timer;
	timer->param = param;
    timer->milli_seconds = milliSeconds;

	channel.id = id;
	channel.event = 0;
	channel.param = timer;
	channel.active = false;
	channel.timer = true;
	channel.on_read = _on_timer_event_callback;
	channel.on_write = 0;

#ifdef CY_SYS_WINDOWS
	channel.fd = timer->pipe.get_read_port();

	//create windows timer queue
	if (!CreateTimerQueueTimer(&(timer->htimer), 0, _on_windows_timer, timer, milliSeconds, milliSeconds, WT_EXECUTEINTIMERTHREAD)) {
		//error...
		CY_LOG(L_FATAL, "CreateTimerQueueTimer Failed");
        delete timer;
		return 0;
	}
#elif defined(CY_SYS_LINUX)
	channel.fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	if (channel.fd < 0) {
		//error
        CY_LOG(L_FATAL, "call timerfd_create Failed");
        delete timer;
        return 0;
	}

	struct itimerspec newValue;
	struct itimerspec oldValue;
	memset(&newValue, 0, sizeof(newValue));
	memset(&oldValue, 0, sizeof(oldValue));

	struct timespec ts;
	ts.tv_sec = milliSeconds / 1000;
	ts.tv_nsec = (milliSeconds%1000) * 1000*1000;

	newValue.it_value = ts;
	newValue.it_interval=ts; //set non-zero for repeated timer
	::timerfd_settime(channel.fd, 0, &newValue, &oldValue);
#else	//macOS, use kqueue
	channel.fd = 0;
#endif

	//add kRead event to poll
	_update_channel_add_event(channel, kRead);
	return id;
}

//-------------------------------------------------------------------------------------
void Looper::delete_event(event_id_t id)
{
	assert(sys_api::thread_get_current_id() == m_current_thread);
	if (id == INVALID_EVENT_ID) return;
	sys_api::auto_mutex lock(m_lock);
	assert((size_t)id < m_channelBuffer.size());

	//unpool it 
	channel_s& channel = m_channelBuffer[id];
	assert(channel.event == kNone && channel.active == false); //should be disabled already

	//if timer event
	if (channel.timer) {
		timer_s* timer = (timer_s*)channel.param;
#ifdef CY_SYS_WINDOWS
		::DeleteTimerQueueTimer(0, timer->htimer, INVALID_HANDLE_VALUE);
#elif defined(CY_SYS_LINUX)
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
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return;
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kRead);
}

//-------------------------------------------------------------------------------------
void Looper::enable_read(event_id_t id)
{
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return;
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_add_event(channel, kRead);
}

//-------------------------------------------------------------------------------------
bool Looper::is_read(event_id_t id) const
{
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return false;
	assert((size_t)id < m_channelBuffer.size());

	const channel_s& channel = m_channelBuffer[id];
	return (channel.event & kRead)!=0;
}

//-------------------------------------------------------------------------------------
void Looper::disable_write(event_id_t id)
{
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return;
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_remove_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
void Looper::enable_write(event_id_t id)
{
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return;
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
	_update_channel_add_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
bool Looper::is_write(event_id_t id) const
{
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return false;
	assert((size_t)id < m_channelBuffer.size());

	const channel_s& channel = m_channelBuffer[id];
	return (channel.event & kWrite)!=0;
}

//-------------------------------------------------------------------------------------
void Looper::disable_all(event_id_t id)
{
	sys_api::auto_mutex lock(m_lock);
	if (id == INVALID_EVENT_ID) return;
	assert((size_t)id < m_channelBuffer.size());

	channel_s& channel = m_channelBuffer[id];
    if(channel.event & kRead)
        _update_channel_remove_event(channel, kRead);
    
    if(channel.event & kWrite)
        _update_channel_remove_event(channel, kWrite);
}

//-------------------------------------------------------------------------------------
void Looper::loop(void)
{
	assert(sys_api::thread_get_current_id() == m_current_thread);

	//is quit request pushed before loop begin
	if (is_quit_pending()) return;

	//register inner pipe first
	Pipe inner_pipe;
	m_inner_pipe = &inner_pipe;
	Looper::event_id_t inner_event_id = register_event(m_inner_pipe->get_read_port(), kRead, this, _on_inner_pipe_touched, 0);

	channel_list readList;
	channel_list writeList;

	for (;;)
	{
		readList.clear();
		writeList.clear();

		//wait in kernel...
		_poll(readList, writeList, true);
		m_loop_counts++;

		if (is_quit_pending()) break;

		//reactor
		for (size_t i = 0; i < readList.size(); i++)
		{
			channel_s* c = &(m_channelBuffer[readList[i]]);
			if (c->on_read == 0 || (c->event & kRead) == 0) continue;

			c->on_read(c->id, c->fd, kRead, c->param);

			if (is_quit_pending()) break;
		}

		if (is_quit_pending()) break;

		for (size_t i = 0; i < writeList.size(); i++)
		{
			channel_s* c = &(m_channelBuffer[writeList[i]]);
			if (c->on_write == 0 || (c->event & kWrite) == 0) continue;

			c->on_write(c->id, c->fd, kWrite, c->param);

			if (is_quit_pending()) break;
		}

		if (is_quit_pending()) break;
	}

	//it's the time to shutdown everything...
	disable_all(inner_event_id);
	delete_event(inner_event_id);
	m_inner_pipe = nullptr;
}

//-------------------------------------------------------------------------------------
void Looper::step(void)
{
	assert(sys_api::thread_get_current_id() == m_current_thread);
	assert(m_inner_pipe==0);
	if (is_quit_pending()) return;

	channel_list readList;
	channel_list writeList;

	//wait in kernel...
	_poll(readList, writeList, false);
	m_loop_counts++;

	if (is_quit_pending()) return;

	//reactor
	for (size_t i = 0; i < readList.size(); i++)
	{
		channel_s* c = &(m_channelBuffer[readList[i]]);
		if (c->on_read == 0 || (c->event & kRead) == 0) continue;

		c->on_read(c->id, c->fd, kRead, c->param);

		if (is_quit_pending()) return;
	}

	for (size_t i = 0; i < writeList.size(); i++)
	{
		channel_s* c = &(m_channelBuffer[writeList[i]]);
		if (c->on_write == 0 || (c->event & kWrite) == 0) continue;

		c->on_write(c->id, c->fd, kWrite, c->param);

		if (is_quit_pending()) return;
	}
}

//-------------------------------------------------------------------------------------
void Looper::push_stop_request(void)
{
	m_quit_cmd = 1;
	_touch_inner_pipe();
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
void Looper::_on_windows_timer(PVOID param, BOOLEAN timer_or_wait_fired)
{
	(void)timer_or_wait_fired;
	timer_s* timer = (timer_s*)param;

	uint64_t touch = 0;
	timer->pipe.write((const char*)(&touch), sizeof(touch));
}
#endif

//-------------------------------------------------------------------------------------
void Looper::_on_timer_event_callback(event_id_t id, socket_t fd, event_t event, void* param)
{
	(void)event;

	timer_s* timer = (timer_s*)param;
#ifndef CY_HAVE_KQUEUE
	uint64_t touch = 0;
	socket_api::read(fd, &touch, sizeof(touch));
#endif

	if (timer->on_timer) {
		timer->on_timer(id, timer->param);
	}
}

//-------------------------------------------------------------------------------------
void Looper::_touch_inner_pipe(void)
{
	if (m_inner_pipe==0) return;

	//just touch once!
	if (m_inner_pipe_touched.exchange(1) != 0) return;

	uint64_t touch = 0;
	m_inner_pipe->write((const char*)&touch, sizeof(touch));
}

//-------------------------------------------------------------------------------------
void Looper::_on_inner_pipe_touched(event_id_t , socket_t fd, event_t , void* param)
{
	uint64_t touch = 0;
	socket_api::read(fd, &touch, sizeof(touch));

	((Looper*)param)->m_inner_pipe_touched = 0;
}

//-------------------------------------------------------------------------------------
void Looper::debug(DebugInterface* debuger, const char* name)
{
	if (!debuger || !(debuger->is_enable())) return;

	char key_temp[256] = { 0 };

	snprintf(key_temp, 256, "Looper:%s:channel_buffer_size", name);
	debuger->set_debug_value(key_temp, (int32_t)m_channelBuffer.size());

	snprintf(key_temp, 256, "Looper:%s:active_channel_size", name);
	debuger->set_debug_value(key_temp, m_active_channel_counts);
}

}

