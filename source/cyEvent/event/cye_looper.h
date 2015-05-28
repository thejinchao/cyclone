/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_LOOPER_H_
#define _CYCLONE_EVENT_LOOPER_H_

#include <cyclone_config.h>
#include <cy_core.h>
#include <event/cye_pipe.h>

namespace cyclone
{

class Looper
{
public:
	typedef uint32_t event_id_t;
	typedef uint32_t event_t;

	//the value of event_t
	enum {
		kNone = 0,
		kRead	= 1,
		kWrite	= 1<<1,
	};

	typedef bool(*event_callback)(event_id_t id, socket_t fd, event_t event, void* param);
	typedef bool(*timer_callback)(event_id_t id, void* param);

public:
	//----------------------
	// event operation(NOT thread safe)
	//----------------------

	//// registe event, return channel id
	event_id_t register_event(socket_t sockfd,
		event_t type,
		void* param, 
		event_callback _on_read,
		event_callback _on_write);

	//// registe timer event
	event_id_t register_timer_event(uint32_t milliSeconds,
		void* param,
		timer_callback _on_timer);

	//// unregister event
	void delete_event(event_id_t id);

	//// main loop(reactor process)
	void loop(void);

	//// update event
	void disable_read(event_id_t id);
	void enable_read(event_id_t id);
	bool is_read(event_id_t id) const;

	void disable_write(event_id_t id);
	void enable_write(event_id_t id);
	bool is_write(event_id_t id) const;

	void disable_all(event_id_t id);

	pid_t get_thread_id(void) const { return m_current_thread; }

protected:
	Looper();
	virtual ~Looper();

public:
	static Looper* create_looper(void);
	static void destroy_looper(Looper*);

	//----------------------
	// inner data
	//----------------------
protected:
	enum { DEFAULT_CHANNEL_BUF_COUNTS = 16 };
	static const event_id_t INVALID_EVENT_ID;

	struct channel_s
	{
		event_id_t id;
		socket_t fd;
		event_t event;
		void *param;
		bool active;
		bool timer;

		event_callback on_read;
		event_callback on_write;

		event_id_t next;
		event_id_t prev;	//only used in select looper
	};
	typedef std::vector< channel_s > channel_buffer;
	typedef std::vector< channel_s* > channel_list;

	channel_buffer m_channelBuffer;	//all event buf
	event_id_t m_free_head;			//free list head in event buf

	pid_t m_current_thread;

	/// Polls the I/O events.
	virtual void _poll(
		channel_list& readChannelList,
		channel_list& writeChannelList) = 0;
	/// Changes the interested I/O events.
	virtual void _update_channel_add_event(channel_s& channel, event_t type) = 0;
	virtual void _update_channel_remove_event(channel_s& channel, event_t type) = 0;

	/// for timer
	struct timer_s
	{
		timer_callback on_timer;
		void* param;

#ifdef CY_SYS_WINDOWS
		Pipe pipe;
		uint32_t winmm_timer_id;
#else
#endif
	};
	
	static bool _on_timer_event_callback(event_id_t id, socket_t fd, event_t event, void* param);

#ifdef CY_SYS_WINDOWS
	static void __stdcall _on_windows_timer(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2);
#endif

private:
	event_id_t _get_free_slot(void);
};

}

#endif

