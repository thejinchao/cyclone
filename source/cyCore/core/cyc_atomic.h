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

	atomic_t() : value_(0) {}
protected:
	volatile T value_;
};

typedef atomic_t<int32_t> atomic_int32_t;
typedef atomic_t<int64_t> atomic_int64_t;

#ifdef _MSC_VER

inline int32_t atomic_t<int32_t>::get(void) {
	return InterlockedCompareExchange((LONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline int32_t atomic_t<int32_t>::get_and_add(int32_t x)	{
	return InterlockedExchangeAdd((LONG volatile *)&value_, x);
}

inline int32_t atomic_t<int32_t>::get_and_set(int32_t x){
	return InterlockedExchange((LONG volatile *)&value_, x);
}

inline int64_t atomic_t<int64_t>::get(void){
	return InterlockedCompareExchange64((LONGLONG volatile *)&value_, (LONG)0, (LONG)0);
}

inline int64_t atomic_t<int64_t>::get_and_add(int64_t x)	{
	return InterlockedExchangeAdd64((LONGLONG volatile *)&value_, x);
}

inline int64_t atomic_t<int64_t>::get_and_set(int64_t x){
	return InterlockedExchange64((LONGLONG volatile *)&value_, x);
}


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

#endif

}
#endif
