/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_LOOPER_KQUEUE_H_
#define _CYCLONE_EVENT_LOOPER_KQUEUE_H_

#include <cy_core.h>
#include <event/cye_looper.h>

#ifdef CY_HAVE_KQUEUE

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

namespace cyclone
{

class Looper_kqueue : public Looper
{
public:
	/// Polls the I/O events.
	virtual void _poll( 
		channel_list& readChannelList,
		channel_list& writeChannelList,
		bool block);
	/// Changes the interested I/O events.
	virtual void _update_channel_add_event(channel_s& channel, event_t event);
	virtual void _update_channel_remove_event(channel_s& channel, event_t event);

private:
    typedef std::vector<struct kevent> kevent_list;
    enum {DEFAULT_MAX_CHANGE_COUNTS=512};
    enum {DEFAULT_MAX_TRIGGER_COUNTS=512};
    
    
	int	m_kqueue;
    kevent_list m_change_evlist;
    kevent_list m_trigger_evlist;
    size_t m_current_index;
    
private:
    bool _add_changes(channel_s& channel, int16_t events, uint16_t flags);
    
public:
	Looper_kqueue();
	virtual ~Looper_kqueue();
};

}

#endif

#endif
