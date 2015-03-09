/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>

#include "cye_looper_epoll.h"
#include "cye_looper_select.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Looper* Looper::create_looper(void)
{
#ifdef CY_HAVE_EPOLL
	return new Looper_epoll();
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
