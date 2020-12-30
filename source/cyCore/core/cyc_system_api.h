/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

#ifndef CY_SYS_WINDOWS
#include <pthread.h>
#endif

typedef void* thread_t;
typedef std::thread::id thread_id_t;

namespace cyclone
{
namespace sys_api
{
//----------------------
// process functions
//----------------------

//// get current process id
pid_t process_get_id(void);

//// get current process name
void process_get_module_name(char* module_name, size_t max_size);

//----------------------
// thread functions
//----------------------
	
//// thread entry function
typedef std::function<void(void*)> thread_function;

//// get current thread id
thread_id_t thread_get_current_id(void);

//// get the system id of thread
thread_id_t thread_get_id(thread_t t);

//// create a new thread(use thread_join to release resources)
thread_t thread_create(thread_function func, void* param, const char* name);

//// create a new thread(all thread resources will be released automatic)
void thread_create_detached(thread_function func, void* param, const char* name);

//// sleep in current thread(milliseconds)
void thread_sleep(int32_t msec);

//// wait the thread to terminate
bool thread_join(thread_t t, int32_t wait_time_ms = -1);

//// get current thread name
const char* thread_get_current_name(void);

//// yield the processor
void thread_yield(void);

//----------------------
// mutex functions
//----------------------
typedef void* mutex_t;

/// create a mutex
mutex_t mutex_create(void);

/// destroy a mutex
void mutex_destroy(mutex_t m);

/// lock mutex(wait other owner unlock infinity time)
void mutex_lock(mutex_t m);

/// lock mutex, wait other owner unlock some time (milliseconds), return false mean timeout
bool mutex_try_lock(mutex_t m, int32_t wait_time);

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
// signal/semaphore functions
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

//// wait a signal infinite
void signal_wait(signal_t s);

//// wait a signal in [t] millisecond(second*1000), return true immediately if the signal is lighted, if false if timeout or other error
bool signal_timewait(signal_t s, int32_t ms);

//// light the signal
void signal_notify(signal_t s);

//----------------------
// time functions
//----------------------

//// get UTC time in microseconds(second*1000*1000) from Epoch(00:00:00 January 1, 1970)
int64_t utc_time_now(void);

/// get local time in format string(strftime)
void local_time_now(char* time_dest, size_t max_size, const char* format);

/// get high performance time, return microseconds(second*1000*1000) from the first call to this function
int64_t performance_time_now(void);

//----------------------
// utility functions
//----------------------
int32_t get_cpu_counts(void);

}
}
