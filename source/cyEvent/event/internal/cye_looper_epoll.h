/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_LOOPER_EPOLL_H_
#define _CYCLONE_EVENT_LOOPER_EPOLL_H_

#include <cy_core.h>
#include <event/cye_looper.h>

namespace cyclone
{
namespace event
{

class Looper_epoll : public Looper
{

public:
	Looper_epoll();
	virtual ~Looper_epoll();
};

}
}

#endif
