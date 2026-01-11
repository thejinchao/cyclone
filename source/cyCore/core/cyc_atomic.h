/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{

typedef std::atomic<int32_t>	atomic_int32_t;
typedef std::atomic<int64_t>	atomic_int64_t;
typedef std::atomic<uint32_t>	atomic_uint32_t;
typedef std::atomic<uint64_t>	atomic_uint64_t;
typedef std::atomic<void*>		atomic_ptr_t;
typedef std::atomic<bool>		atomic_bool_t;

}
