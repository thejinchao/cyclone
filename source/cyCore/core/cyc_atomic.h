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

template<typename T>
inline bool atomic_compare_exchange(std::atomic<T>& obj, T expected, T desired) 
{
	return obj.compare_exchange_weak(expected, desired);
}

template<typename T>
inline bool atomic_greater_exchange(std::atomic<T>& obj, T comparison, T desired) 
{
	T snapshot;
	bool greater;
	do {
		snapshot = obj.load();
		greater = (comparison > snapshot);
	} while (greater && !obj.compare_exchange_weak(snapshot, desired));
	return greater;
}

template<typename T>
inline bool atomic_smaller_exchange(std::atomic<T>& obj, T comparison, T desired)
{
	T snapshot;
	bool smaller;
	do {
		snapshot = obj.load();
		smaller = (comparison < snapshot);
	} while (smaller && !obj.compare_exchange_weak(snapshot, desired));
	return smaller;
}

}
