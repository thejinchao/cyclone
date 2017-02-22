/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>

#include "cye_looper_epoll.h"
#include "cye_looper_select.h"
#include "cye_looper_kqueue.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Looper* Looper::create_looper(void)
{
#ifdef CY_HAVE_EPOLL
	return new Looper_epoll();
#elif defined CY_HAVE_KQUEUE
	return new Looper_kqueue();
#else
	return new Looper_select();
#endif
}

//-------------------------------------------------------------------------------------
void Looper::destroy_looper(Looper* looper)
{
	delete looper;
}

}
