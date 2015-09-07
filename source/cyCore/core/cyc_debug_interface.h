/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_DEBUG_INTERFACE_H_
#define _CYCLONE_CORE_DEBUG_INTERFACE_H_

#include <cyclone_config.h>

namespace cyclone
{

class DebugInterface
{
public:
	// all functions must thread safe
	virtual bool is_enable(void) = 0;

	virtual void set_debug_value(const char* key, const char* value) = 0;
	virtual void set_debug_value(const char* key, int32_t value) = 0;

	virtual void del_debug_value(const char* key) = 0;
};

}

#endif
