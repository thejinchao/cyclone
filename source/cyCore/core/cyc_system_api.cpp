/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_system_api.h"

#ifdef CY_SYS_WINDOWS
#include <process.h>
#include <Shlwapi.h>
#else
#include <sys/syscall.h>
#include <sys/time.h>
#include <sched.h>
#include <condition_variable>
#endif

#ifdef CY_SYS_MACOS
#include <libproc.h>
#endif

#include <time.h>
#include <chrono>
#include <mutex>

namespace cyclone
{
namespace sys_api
{

//-------------------------------------------------------------------------------------
pid_t process_get_id(void)
{
#ifdef CY_SYS_WINDOWS
	return (pid_t)::GetCurrentProcessId();
#else
	return ::getpid();
#endif
}

//-------------------------------------------------------------------------------------
void process_get_module_name(char* module_name, size_t max_size)
{
#ifdef CY_SYS_WINDOWS
	char process_path_name[MAX_PATH] = { 0 };
	::GetModuleFileName(::GetModuleHandle(0), process_path_name, MAX_PATH);
	strncpy(module_name, ::PathFindFileNameA(process_path_name), max_size);
#else

	#ifdef CY_SYS_MACOS
		char process_path_name[PROC_PIDPATHINFO_MAXSIZE] = { 0 };
		proc_pidpath(process_get_id(), process_path_name, PROC_PIDPATHINFO_MAXSIZE);
	#else
		char process_path_name[256] = { 0 };
		if (readlink("/proc/self/exe", process_path_name, 256)<0) {
			strncpy(process_path_name, "unknown", 256);
		}
	#endif
	
	const char* process_name = strrchr(process_path_name, '/');
	if (process_name != 0) process_name++;
	else process_name = "unknown";

	strncpy(module_name, process_name, max_size);
#endif

}

//-------------------------------------------------------------------------------------
struct thread_data_s
{
	thread_id_t tid;
	std::thread thandle;
	thread_function entry_func;
	void* param;
	std::string name;
	bool detached;
	signal_t resume_signal;
	signal_t exit_signal;
};

//-------------------------------------------------------------------------------------
static thread_local thread_data_s* s_thread_data = nullptr;

//-------------------------------------------------------------------------------------
thread_id_t thread_get_current_id(void)
{
	return s_thread_data == 0 ? std::this_thread::get_id() : s_thread_data->tid;
}

//-------------------------------------------------------------------------------------
thread_id_t thread_get_id(thread_t t)
{
	thread_data_s* data = (thread_data_s*)t;
	return data->tid;
}

//-------------------------------------------------------------------------------------
void _thread_entry(thread_data_s* data)
{
	s_thread_data = data;
	signal_wait(data->resume_signal);
	signal_destroy(data->resume_signal);
	data->resume_signal = 0;
	
	//set random seed
	srand((uint32_t)::time(0));

	//run thread function
	if (data->entry_func) {
		data->entry_func(data->param);
	}

	//set exit signal
	signal_notify(data->exit_signal);

	s_thread_data = nullptr;
	if (data->detached) {
		signal_destroy(data->exit_signal);
		delete data;
	}
}

//-------------------------------------------------------------------------------------
thread_t _thread_create(thread_function func, void* param, const char* name, bool detached)
{
	thread_data_s* data = new thread_data_s;
	data->param = param;
	data->entry_func = func;
	data->detached = detached;
	data->name = name?name:"";
	data->resume_signal = signal_create();
	data->exit_signal = signal_create();
	data->thandle = std::thread(_thread_entry, data);
	data->tid = data->thandle.get_id();

	//detached thread
	if (data->detached) data->thandle.detach();
	
	//resume thread
	signal_notify(data->resume_signal);
	return detached ? nullptr : data;
}

//-------------------------------------------------------------------------------------
thread_t thread_create(thread_function func, void* param, const char* name)
{
	return _thread_create(func, param, name, false);
}

//-------------------------------------------------------------------------------------
void thread_create_detached(thread_function func, void* param, const char* name)
{
	_thread_create(func, param, name, true);
}

//-------------------------------------------------------------------------------------
void thread_sleep(int32_t msec)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

//-------------------------------------------------------------------------------------
bool thread_join(thread_t thread, int32_t wait_time_ms)
{
	thread_data_s* data = static_cast<thread_data_s*>(thread);
	if (data == nullptr) return true;

	assert(!(data->detached));

	if (wait_time_ms < 0) {
		data->thandle.join();
	}
	else {
		bool exited = signal_timewait(data->exit_signal, wait_time_ms);
		if (!exited) return false;

		data->thandle.join();
	}

	//already exited
	signal_destroy(data->exit_signal);
	delete data;
	return true;
}

//-------------------------------------------------------------------------------------
const char* thread_get_current_name(void)
{
	return (s_thread_data == nullptr) ? "<UNNAME>" : s_thread_data->name.c_str();
}

//-------------------------------------------------------------------------------------
void thread_yield(void)
{
	std::this_thread::yield();
}

//-------------------------------------------------------------------------------------
struct mutex_data_s
{
#ifdef CY_SYS_WINDOWS
	HANDLE h;
#else
	std::timed_mutex h;
#endif
};

//-------------------------------------------------------------------------------------
mutex_t mutex_create(void)
{
	mutex_data_s* mutex = new mutex_data_s;

#ifdef CY_SYS_WINDOWS
	mutex->h = ::CreateMutexA(NULL, FALSE, nullptr);
	if (mutex->h == NULL) {
		delete mutex;
		return nullptr;
	}
#endif
	return mutex;
}

//-------------------------------------------------------------------------------------
void mutex_destroy(mutex_t m)
{
	mutex_data_s* mutex = static_cast<mutex_data_s*>(m);
	if (mutex == nullptr) return;

#ifdef CY_SYS_WINDOWS
	::CloseHandle(mutex->h);
	mutex->h = NULL;
#endif
	delete mutex;
}

//-------------------------------------------------------------------------------------
void mutex_lock(mutex_t m)
{
	mutex_data_s* mutex = static_cast<mutex_data_s*>(m);
	if (mutex == nullptr) return;

#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(mutex->h, INFINITE);
#else
	mutex->h.lock();
#endif
}

//-------------------------------------------------------------------------------------
bool mutex_try_lock(mutex_t m, int32_t wait_time_ms)
{
	mutex_data_s* mutex = static_cast<mutex_data_s*>(m);
	if (mutex == nullptr) return false;

#ifdef CY_SYS_WINDOWS
	return WAIT_OBJECT_0 == ::WaitForSingleObject(mutex->h, wait_time_ms);
#else
	if (wait_time_ms <= 0)
	{
		return mutex->h.try_lock();
	}
	else
	{
		// Use std::timed_mutex::try_lock_for with chrono duration
		auto timeout = std::chrono::milliseconds(wait_time_ms);
		return mutex->h.try_lock_for(timeout);
	}
#endif
}

//-------------------------------------------------------------------------------------
void mutex_unlock(mutex_t m)
{
	mutex_data_s* mutex = static_cast<mutex_data_s*>(m);
	if (mutex == nullptr) return;

#ifdef CY_SYS_WINDOWS
	::ReleaseMutex(mutex->h);
#else
	mutex->h.unlock();
#endif
}

//-------------------------------------------------------------------------------------
struct signal_data_s
{
#ifdef CY_USE_NATIVE_WINDOWS_EVENT
	HANDLE h;
#else
	std::mutex mutex;
	std::condition_variable cond;
	atomic_int32_t  predicate;
#endif
};

//-------------------------------------------------------------------------------------
signal_t signal_create(void)
{
	signal_data_s* sig = new signal_data_s();
#ifdef CY_USE_NATIVE_WINDOWS_EVENT
	sig->h = ::CreateEventW(0, FALSE, FALSE, 0);
#else
	sig->predicate = 0;
#endif
	return static_cast<signal_t>(sig);
}

//-------------------------------------------------------------------------------------
void signal_destroy(signal_t s)
{
	signal_data_s* sig = static_cast<signal_data_s*>(s);
	if (sig == nullptr) return;

#ifdef CY_USE_NATIVE_WINDOWS_EVENT
	::CloseHandle(sig->h);
#endif
	delete sig;
}

//-------------------------------------------------------------------------------------
void signal_wait(signal_t s)
{
	signal_data_s* sig = static_cast<signal_data_s*>(s);
	if (sig == nullptr) return;

#ifdef CY_USE_NATIVE_WINDOWS_EVENT
	::WaitForSingleObject(sig->h, INFINITE);
#else
	std::unique_lock<std::mutex> lock(sig->mutex);
	sig->cond.wait(lock, [sig] { 
		return sig->predicate.exchange(0)==1;
	});
#endif
}

//-------------------------------------------------------------------------------------
bool signal_timewait(signal_t s, int32_t ms)
{
	signal_data_s* sig = static_cast<signal_data_s*>(s);
	if (sig == nullptr) return false;

#ifdef CY_USE_NATIVE_WINDOWS_EVENT
	return (WAIT_OBJECT_0 == ::WaitForSingleObject(sig->h, (DWORD)ms));
#else

	// attempt to atomically consume predicate first to avoid
	// taking the mutex if the signal is already set.
	if (sig->predicate.exchange(0) == 1) return true;

	// if ms>0, wait with timeout
	if (ms > 0)
	{
		std::unique_lock<std::mutex> lock(sig->mutex);
		auto timeout = std::chrono::milliseconds(ms);
		return sig->cond.wait_for(lock, timeout, [sig] {
			return sig->predicate.exchange(0)==1;
		});
	}
	return false;
#endif
}

//-------------------------------------------------------------------------------------
void signal_notify(signal_t s)
{
	signal_data_s* sig = static_cast<signal_data_s*>(s);
	if (sig == nullptr) return;

#ifdef CY_USE_NATIVE_WINDOWS_EVENT
	::SetEvent(sig->h);
#else
	{
		std::lock_guard<std::mutex> lock(sig->mutex);
		sig->predicate = 1;
	}
	sig->cond.notify_one();
#endif
}

//-------------------------------------------------------------------------------------
int64_t utc_time_now(void)
{
#ifdef CY_SYS_WINDOWS
	FILETIME ftm;
	::GetSystemTimeAsFileTime(&ftm);

	uint64_t time = ((uint64_t)ftm.dwLowDateTime) + ((uint64_t)ftm.dwHighDateTime << 32);

	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const int64_t EPOCH = ((int64_t)116444736000000000LL);

	//1 micro second =10 * (100 nano second)
	return (int64_t)((time - EPOCH) / 10LL);
#else
	const int64_t kMicroSecondsPerSecond = 1000ll * 1000ll;

	struct timeval tv;
	gettimeofday(&tv, 0);
	int64_t seconds = tv.tv_sec;
	return seconds * kMicroSecondsPerSecond + tv.tv_usec;
#endif
}

//-------------------------------------------------------------------------------------
void local_time_now(char* time_dest, size_t max_size, const char* format)
{
	time_t local_time = time(0);
	struct tm tm_now;
#ifdef CY_SYS_WINDOWS
	localtime_s(&tm_now, &local_time);
#else
	localtime_r(&local_time, &tm_now);
#endif
	strftime(time_dest, max_size, format, &tm_now);
}

//-------------------------------------------------------------------------------------
int64_t performance_time_now(void)
{
	const int64_t kMicroSecondsPerSecond = 1000ll * 1000ll;
#ifdef CY_SYS_WINDOWS

	static std::once_flag initialzed;
	static LARGE_INTEGER performanceFrequency = { 0 };
	static LARGE_INTEGER beginOffset = { 0 };

	std::call_once(initialzed, [&]() {
		// On systems that run Windows XP or later, the function will always succeed and will thus never return zero.
		BOOL getFrequence = QueryPerformanceFrequency(&performanceFrequency);
		assert(getFrequence);
		if (!getFrequence) 	return;

		// Get offset time
		QueryPerformanceCounter(&beginOffset);
	});

	if (performanceFrequency.QuadPart == 0) return 0;

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	now.QuadPart -= beginOffset.QuadPart;
	return now.QuadPart * kMicroSecondsPerSecond / performanceFrequency.QuadPart;
#else
	const int64_t kNanoSecondsPerSecond = 1000ll * 1000ll * 1000ll;
	const int64_t kNanoSecondsPerMicroSecond = 1000ll;

	static std::once_flag initialzed;
	static timespec beginOffset = { 0, 0 };

	std::call_once(initialzed, [&]() {
		if (clock_gettime(CLOCK_MONOTONIC, &beginOffset)) {
			return;
		}
	});
	if (beginOffset.tv_sec == 0 && beginOffset.tv_nsec==0) return 0;

	struct timespec tsNow = { 0, 0 };
	if (clock_gettime(CLOCK_MONOTONIC, &tsNow)) {
		return 0;
	}

	struct timespec tsDiff = { 0, 0 };
	if (tsNow.tv_nsec < beginOffset.tv_nsec) {
		tsDiff.tv_sec = tsNow.tv_sec - beginOffset.tv_sec - 1;
		tsDiff.tv_nsec = kNanoSecondsPerSecond + tsNow.tv_nsec - beginOffset.tv_nsec;
	}
	else {
		tsDiff.tv_sec = tsNow.tv_sec - beginOffset.tv_sec;
		tsDiff.tv_nsec = tsNow.tv_nsec - beginOffset.tv_nsec;
	}
	return tsDiff.tv_sec*kMicroSecondsPerSecond + tsDiff.tv_nsec / kNanoSecondsPerMicroSecond;
#endif
}

//-------------------------------------------------------------------------------------
int32_t get_cpu_counts(void)
{
	const int32_t DEFAULT_CPU_COUNTS = 2;

#ifdef CY_SYS_WINDOWS
	//use GetLogicalProcessorInformation

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	DWORD returnLength = 0;
	while (FALSE == GetLogicalProcessorInformation(buffer, &returnLength))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			if (buffer) CY_FREE(buffer);
			buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)CY_MALLOC(returnLength);
		}
		else
		{
			if (buffer) CY_FREE(buffer);
			return DEFAULT_CPU_COUNTS;
		}
	}

	int32_t cpu_counts = 0;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION p = buffer;
	DWORD byteOffset = 0;
	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		if (p->Relationship == RelationProcessorCore) {
			cpu_counts++;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		p++;
	}
	CY_FREE(buffer);
	return cpu_counts;
#else
	long int cpu_counts = 0;
	if ((cpu_counts = sysconf(_SC_NPROCESSORS_ONLN)) == -1){
		CY_LOG(L_ERROR, "get cpu counts \"sysconf(_SC_NPROCESSORS_ONLN)\" error, use default(%d)", DEFAULT_CPU_COUNTS);
		return DEFAULT_CPU_COUNTS;
	}
	return (int32_t)cpu_counts;
#endif
}

}
}
