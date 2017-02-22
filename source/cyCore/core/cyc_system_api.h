/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_THREAD_API_H_
#define _CYCLONE_CORE_THREAD_API_H_

#include <cyclone_config.h>

#ifndef CY_SYS_WINDOWS
#include <pthread.h>
#endif

typedef void* thread_t;

namespace cyclone
{
namespace sys_api
{

//----------------------
// thread functions
//----------------------
	
//// thread entry function
typedef void (*thread_function)(void* param);

//// get current thread id
pid_t thread_get_current_id(void);

//// get the system id of thread
pid_t thread_get_id(thread_t t);

//// create a new thread
thread_t thread_create(thread_function func, void* param, const char* name);

//// sleep in current thread
void thread_sleep(int32_t msec);

//// wait the thread to terminate
void thread_join(thread_t t);

//// get current thread name
const char* thread_get_current_name(void);

//// yield the processor
void thread_yield(void);

//----------------------
// mutex functions
//----------------------
#ifdef CY_SYS_WINDOWS
typedef LPCRITICAL_SECTION	mutex_t;
#else
typedef pthread_mutex_t* mutex_t;
#endif

/// create a mutex
mutex_t mutex_create(void);

/// destroy a mutex
void mutex_destroy(mutex_t m);

/// lock mutex(wait other owner unlock)
void mutex_lock(mutex_t m);

/// unlock mutex
void mutex_unlock(mutex_t m);

/// auto lock
struct auto_mutex
{
	auto_mutex(mutex_t m) : _m(m) { mutex_lock(_m); }
	~auto_mutex() { mutex_unlock(_m); }
	mutex_t _m;
};

//----------------------
// signal/semaphone functions
//----------------------

#ifdef CY_SYS_WINDOWS
typedef HANDLE	signal_t;
#else
typedef void*	signal_t;
#endif

//// create a signal
signal_t signal_create(void);

//// destroy a signal
void signal_destroy(signal_t s);

//// wait a signal inifinite
void signal_wait(signal_t s);

//// light the signal
void signal_notify(signal_t s);

//----------------------
// time functions
//----------------------

//// get time in microseconds(second*1000*1000) from Epoch
int64_t time_now(void);

/// get time in format string(strftime)
void time_now(char* time_dest, size_t max_size, const char* format);

//----------------------
// utility functions
//----------------------
uint32_t get_cpu_counts(void);

}
}
#endif
