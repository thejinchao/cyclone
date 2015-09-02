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

namespace cyclone
{

template<typename T>
class atomic_t
{
public:
	T get(void);
	T get_and_add(T x);
	T get_and_set(T x);

	T add_and_get(T x)	{
		return get_and_add(x) + x;
	}

	void set(T x) {
		get_and_set(x);
	}

	//like ++i
	T increment_and_get(void) {
		return add_and_get(1);
	}
	// like --i
	T decrement_and_get(void) {
		return add_and_get(-1);
	}

	void add(T x) {
		get_and_add(x);
	}

	void increment(void) {
		increment_and_get();
	}

	void decrement(void)	{
		decrement_and_get();
	}

	//compare and swap, if the current value is oldV, then write newV into self
	//return true if the comparison is successful and newV was written
	bool cas(T oldV, T newV);

	atomic_t() : value_(0) {}
	atomic_t(T x) : value_(x) {}
protected:
	volatile T value_;
};

typedef atomic_t<int32_t> atomic_int32_t;
typedef atomic_t<int64_t> atomic_int64_t;
typedef atomic_t<uint32_t> atomic_uint32_t;
typedef atomic_t<uint64_t> atomic_uint64_t;
typedef atomic_t<void*> atomic_ptr_t;

#ifdef _MSC_VER

//--------------
inline int32_t atomic_t<int32_t>::get(void) {
	return InterlockedCompareExchange((LONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline int32_t atomic_t<int32_t>::get_and_add(int32_t x)	{
	return InterlockedExchangeAdd((LONG volatile *)&value_, x);
}

inline int32_t atomic_t<int32_t>::get_and_set(int32_t x){
	return InterlockedExchange((LONG volatile *)&value_, x);
}

inline bool atomic_t<int32_t>::cas(int32_t oldV, int32_t newV) {
	return (InterlockedCompareExchange((LONG volatile *)&value_, (LONG)newV, (LONG)oldV) == (LONG)oldV);
}

//--------------

inline int64_t atomic_t<int64_t>::get(void){
	return InterlockedCompareExchange64((LONGLONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline int64_t atomic_t<int64_t>::get_and_add(int64_t x)	{
	return InterlockedExchangeAdd64((LONGLONG volatile *)&value_, x);
}

inline int64_t atomic_t<int64_t>::get_and_set(int64_t x){
	return InterlockedExchange64((LONGLONG volatile *)&value_, x);
}

inline bool atomic_t<int64_t>::cas(int64_t oldV, int64_t newV) {
	return (InterlockedCompareExchange64((LONGLONG volatile *)&value_, (LONGLONG)newV, (LONGLONG)oldV) == (LONGLONG)oldV);
}

//--------------
inline uint32_t atomic_t<uint32_t>::get(void) {
	return InterlockedCompareExchange((LONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline uint32_t atomic_t<uint32_t>::get_and_add(uint32_t x)	{
	return InterlockedExchangeAdd((LONG volatile *)&value_, x);
}

inline uint32_t atomic_t<uint32_t>::get_and_set(uint32_t x){
	return InterlockedExchange((LONG volatile *)&value_, x);
}

inline bool atomic_t<uint32_t>::cas(uint32_t oldV, uint32_t newV) {
	return (InterlockedCompareExchange((LONG volatile *)&value_, (LONG)newV, (LONG)oldV) == (LONG)oldV);
}

//--------------

inline uint64_t atomic_t<uint64_t>::get(void){
	return InterlockedCompareExchange64((LONGLONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline uint64_t atomic_t<uint64_t>::get_and_add(uint64_t x)	{
	return InterlockedExchangeAdd64((LONGLONG volatile *)&value_, x);
}

inline uint64_t atomic_t<uint64_t>::get_and_set(uint64_t x){
	return InterlockedExchange64((LONGLONG volatile *)&value_, x);
}

inline bool atomic_t<uint64_t>::cas(uint64_t oldV, uint64_t newV) {
	return (InterlockedCompareExchange64((LONGLONG volatile *)&value_, (LONGLONG)newV, (LONGLONG)oldV) == (LONGLONG)oldV);
}
//--------------
#ifdef CY_64BIT
inline void* atomic_t<void*>::get(void) {
	return (void*)InterlockedCompareExchange64((LONGLONG volatile *)&value_, (int64_t)0, (int64_t)0);
}

inline void* atomic_t<void*>::get_and_add(void* x)	{
	return (void*)InterlockedExchangeAdd64((LONGLONG volatile *)&value_, (int64_t)x);
}

inline void* atomic_t<void*>::get_and_set(void* x){
	return (void*)InterlockedExchange64((LONGLONG volatile *)&value_, (int64_t)x);
}

inline bool atomic_t<void*>::cas(void* oldV, void* newV) {
	return (InterlockedCompareExchange64((LONGLONG volatile *)&value_, (LONGLONG)newV, (LONGLONG)oldV) == (LONGLONG)oldV);
}

#else
inline void* atomic_t<void*>::get(void) {
	return (void*)(INT_PTR)InterlockedCompareExchange((LONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline void* atomic_t<void*>::get_and_add(void* x)	{
	return (void*)(INT_PTR)InterlockedExchangeAdd((LONG volatile *)&value_, (UINT)(UINT_PTR)x);
}

inline void* atomic_t<void*>::get_and_set(void* x){
	return (void*)(INT_PTR)InterlockedExchange((LONG volatile *)&value_, (UINT)(UINT_PTR)x);
}

inline bool atomic_t<void*>::cas(void* oldV, void* newV) {
	return (InterlockedCompareExchange((LONG volatile *)&value_, (LONG)newV, (LONG)oldV) == (LONG)oldV);
}

#endif

#else

template<typename T>
T atomic_t<T>::get(void) {
	// in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
	return __sync_val_compare_and_swap(&value_, 0, 0);
}

template<typename T>
T atomic_t<T>::get_and_add(T x) {
	// in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
	return __sync_fetch_and_add(&value_, x);
}

template<typename T>
T atomic_t<T>::get_and_set(T x) {
	// in gcc >= 4.7: __atomic_store_n(&value, x, __ATOMIC_SEQ_CST)
	return __sync_lock_test_and_set(&value_, x);
}

template<typename T>
bool atomic_t<T>::cas(T oldV, T newV) {
	return __sync_bool_compare_and_swap(&value_, oldV, newV);
}

#endif

}
#endif
