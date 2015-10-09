/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_ATOMIC_H_
#define _CYCLONE_CORE_ATOMIC_H_

//
// copy from muduo  https://github.com/chenshuo/muduo
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <cyclone_config.h>
#include <atomic>

namespace cyclone
{

typedef std::atomic_int32_t		atomic_int32_t;
typedef std::atomic_int64_t		atomic_int64_t;
typedef std::atomic_uint32_t	atomic_uint32_t;
typedef std::atomic_uint64_t	atomic_uint64_t;
typedef std::atomic<void*>		atomic_ptr_t;

template<typename T>
inline bool atomic_compare_exchange(std::atomic<T>& obj, T expected, T desired) {
	return obj.compare_exchange_weak(expected, desired);
}

}
#endif
