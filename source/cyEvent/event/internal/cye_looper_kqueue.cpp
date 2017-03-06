/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_looper_kqueue.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Looper_kqueue::Looper_kqueue()
	: Looper()
	, m_kqueue(::kqueue())
{
    m_change_evlist.resize(DEFAULT_MAX_CHANGE_COUNTS);
    m_trigger_evlist.resize(DEFAULT_MAX_TRIGGER_COUNTS);
    m_current_index = 0;
}

//-------------------------------------------------------------------------------------
Looper_kqueue::~Looper_kqueue()
{
	::close(m_kqueue);
    m_change_evlist.clear();
    m_trigger_evlist.clear();
    m_current_index = 0;
}

//-------------------------------------------------------------------------------------
void Looper_kqueue::_poll(
	channel_list& readChannelList,
	channel_list& writeChannelList,
	bool block)
{
    int n = (int) m_current_index;
    m_current_index = 0;
    
    struct timespec ts={0,0};
    
    int event_counts = ::kevent(m_kqueue, &(m_change_evlist[0]), n,
                                &(m_trigger_evlist[0]), (int)m_trigger_evlist.size(), block ? nullptr : &ts);
    
    int err_info = event_counts==-1 ? errno : 0;
    
    if(err_info!=0) {
        CY_LOG(L_ERROR, "kevent() error, no=%d", err_info);
        return;
    }
    
    if(event_counts==0) {
        if(block) {
            CY_LOG(L_ERROR, "kevent() returned no events without timeout");
        }
        return;
    }
    
    for (size_t i = 0; i < (size_t)event_counts; i++) {
        
        const struct kevent& ev = m_trigger_evlist[i];
        
        if (ev.flags & EV_ERROR) {
            CY_LOG(L_ERROR, "kevent() error on %d filter:%d flags:%04Xd",
                          (int)ev.ident, ev.filter, ev.flags);
            continue;
        }
        channel_s* channel = &(m_channelBuffer[(uint32_t)(uintptr_t)ev.udata]);
        
        if ((ev.filter==EVFILT_READ||ev.filter==EVFILT_TIMER) && channel->active && channel->on_read != 0)
        {
            //read event
            readChannelList.push_back(channel->id);
        }
        
        if ((ev.filter ==EVFILT_WRITE) && channel->active && channel->on_write != 0)
        {
            //write event
            writeChannelList.push_back(channel->id);
        }
    }
}

//-------------------------------------------------------------------------------------
bool Looper_kqueue::_add_changes(channel_s& channel, int16_t filter, uint16_t flags)
{
    if(m_current_index>=m_change_evlist.size()) {
        //error log something...
        CY_LOG(L_WARN, "kqueue change list is filled up");
        
        struct timespec ts={0,0};
        
        //upload current changes to kernel
        if (::kevent(m_kqueue, &(m_change_evlist[0]), (int)m_current_index, nullptr, 0, &ts)== -1)
        {
            CY_LOG(L_ERROR, "kevent() failed");
            return false;
        }
        
        m_current_index = 0;
    }
    
    struct kevent* kev = &m_change_evlist[m_current_index];
    
    kev->ident = (uintptr_t)channel.fd;
    kev->filter = (short) filter;
    kev->flags = (u_short) flags;
    kev->fflags = 0;
    kev->data = channel.timer ? ((timer_s*)(channel.param))->milli_seconds : 0;
    kev->udata = (void*)(uintptr_t)channel.id;
    
    m_current_index++;
    return true;
}

//-------------------------------------------------------------------------------------
void Looper_kqueue::_update_channel_add_event(channel_s& channel, event_t event)
{
    assert(event==kRead || event==kWrite);
    int16_t filter = 0;
    
    if(channel.timer) {
        //timer event
        filter = EVFILT_TIMER;
    }
    else if ((event == kRead) && !(channel.event & kRead) && channel.on_read)
        filter = EVFILT_READ;
    
    else if ((event == kWrite) && !(channel.event & kWrite) && channel.on_write)
        filter = EVFILT_WRITE;
    
    if (_add_changes(channel, filter, EV_ADD|EV_ENABLE))
    {
        if(!channel.active) m_active_channel_counts++;
        
        channel.event |= event;
        channel.active = true;
    }
    
    _touch_inner_pipe();
}

//-------------------------------------------------------------------------------------
void Looper_kqueue::_update_channel_remove_event(channel_s& channel, event_t event)
{
    assert(event==kRead || event==kWrite);
    
    if ((channel.event & event) == kNone || !channel.active) return;

    int16_t filter = 0;
    
    if(channel.timer) {
        filter = EVFILT_TIMER;
    }
    else if ((event == kRead) && (channel.event & kRead) && channel.on_read)
        filter = EVFILT_READ;
    
    else if ((event == kWrite) && (channel.event & kWrite) && channel.on_write)
        filter = EVFILT_WRITE;
    
    //TODO: BAD!
    // if the event is still not passed to a kernel we will not pass it
    for(size_t i=0; i<m_current_index; i++) {
        const struct kevent& kev = m_change_evlist[i];
        
        if(channel.id == (size_t)(uintptr_t)kev.udata && filter==kev.filter) {
            
            m_current_index--;
            
            //move the last kevent to this slot
            if (i < m_current_index) {
                m_change_evlist[i] = m_change_evlist[m_current_index];
            }
        
            channel.event &= ~event;
            if (channel.event == kNone){
                if(channel.active) m_active_channel_counts--;
                channel.active = false;
            }
            return;
        }
    }
        
    //upload to kernel
    if(_add_changes(channel, filter, EV_DELETE)) {
        channel.event &= ~event;
    }
    
    if (channel.event == kNone){
        if(channel.active) m_active_channel_counts--;
        channel.active = false;
    }
}

}
