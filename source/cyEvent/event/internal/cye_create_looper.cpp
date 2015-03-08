/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>

#include "cye_looper_epoll.h"
#include "cye_looper_select.h"

namespace cyclone
{
namespace event
{

//-------------------------------------------------------------------------------------
Looper* Looper::create_looper(void)
{
	return new Looper_select();
}

//-------------------------------------------------------------------------------------
void Looper::destroy_looper(Looper* looper)
{
	delete looper;
}

}
}
