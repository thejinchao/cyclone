/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

#ifndef CY_SYS_WINDOWS
#include <mutex>
#include <condition_variable>
#endif


namespace cyclone
{

// opaque thread handle
typedef void* thread_t;

// thread id type
typedef uint64_t thread_id_t; 

namespace sys_api
{
//----------------------
// process functions
//----------------------

//// get current process id
pid_t process_get_id(void);

/// Write the current process module name into `module_name` buffer.
/// Up to `max_size` bytes will be written; caller should provide a buffer
/// large enough for the expected name. The function attempts to null-terminate
/// the result when possible.
void process_get_module_name(char* module_name, size_t max_size);

//----------------------
// thread functions
//----------------------

/// Thread entry function type. The function receives the user `void*` parameter.
typedef std::function<void(void*)> thread_function;

/// Return an identifier for the current thread.
thread_id_t thread_get_current_id(void);

/// Return the thread id stored in `thread_t` returned by `thread_create`.
thread_id_t thread_get_id(thread_t t);

/// Create a new joinable thread. The returned `thread_t` represents the
/// created thread and must be released with `thread_join` when no longer
/// needed. The `name` parameter is an optional descriptive name.
thread_t thread_create(thread_function func, void* param, const char* name);

/// Create a new detached thread. All thread resources will be released
/// automatically when the thread exits; do not call `thread_join` on a
/// detached thread.
void thread_create_detached(thread_function func, void* param, const char* name);

/// Sleep the current thread for `msec` milliseconds.
void thread_sleep(int32_t msec);

/// Wait for the thread to terminate. If `wait_time_ms < 0` the call blocks
/// indefinitely. Returns true when the thread has terminated, false on
/// timeout or error. Passing a null `thread_t` returns true immediately.
bool thread_join(thread_t t, int32_t wait_time_ms = -1);

/// Return the name associated with the current thread (if available).
const char* thread_get_current_name(void);

/// Yield the processor to allow other threads to run.
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
typedef void* signal_t;

/// Create an auto-reset signal (wake one waiter). Returns a handle that
/// must be destroyed with `signal_destroy`.
signal_t signal_create(void);

/// Destroy a signal object. The signal must not be waited on when destroyed.
void signal_destroy(signal_t s);

/// Wait for the signal. If the signal is set, the call returns immediately
/// and consumes the signal (auto-reset). Otherwise blocks until notified.
void signal_wait(signal_t s);

/// Wait for the signal for up to `ms` milliseconds. Returns true if the
/// signal was received (consumed), false on timeout or error.
bool signal_timewait(signal_t s, int32_t ms);

/// Notify (set) the signal. This will wake a single waiting thread.
void signal_notify(signal_t s);

//----------------------
// time functions
//----------------------

/// Return current UTC time in microseconds(second*1000*1000) since Unix epoch (1970-01-01).
int64_t utc_time_now(void);

/// Format local time using `strftime`-style `format` into `time_dest`.
/// `max_size` is the size of the destination buffer.
void local_time_now(char* time_dest, size_t max_size, const char* format);

/// Return a high-resolution performance counter value. The return value is
/// microseconds(second*1000*1000) elapsed since the first call to this function (monotonic).
int64_t performance_time_now(void);

//----------------------
// utility functions
//----------------------
int32_t get_cpu_counts(void);

}
}
