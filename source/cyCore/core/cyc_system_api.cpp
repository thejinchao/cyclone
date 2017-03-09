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
#include <pthread.h>
#include <sched.h>
#endif

#ifdef CY_SYS_MACOS
#include <libproc.h>
#endif

#include <time.h>

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
	std::atomic<pid_t> tid;
	thread_function entry_func;
	void* param;
	char name[MAX_PATH];
	bool detached;
#ifdef CY_SYS_WINDOWS
	HANDLE handle;
#else
	pthread_t handle;
#endif
};

//-------------------------------------------------------------------------------------
#ifdef CY_SYS_WINDOWS
	static __declspec(thread) thread_data_s* s_thread_data = 0;
#else
	static __thread thread_data_s* s_thread_data = 0;
#endif

//-------------------------------------------------------------------------------------
pid_t INLINE _nativeThreadID(void)
{
#ifdef CY_SYS_WINDOWS
	return static_cast<pid_t>(::GetCurrentThreadId());
#elif defined CY_SYS_MACOS
	return static_cast<pid_t>(::syscall(SYS_thread_selfid));
#else
	return static_cast<pid_t>(::syscall(SYS_gettid));
#endif
}

//-------------------------------------------------------------------------------------
pid_t thread_get_current_id(void)
{
	return s_thread_data == 0 ? _nativeThreadID() : s_thread_data->tid.load();
}

//-------------------------------------------------------------------------------------
pid_t thread_get_id(thread_t t)
{
	thread_data_s* data = (thread_data_s*)t;
	return data->tid;
}

#ifdef CY_SYS_WINDOWS
//-------------------------------------------------------------------------------------
static unsigned int __stdcall __win32_thread_entry(void* param)
{
	thread_data_s* data = (thread_data_s*)param;
	s_thread_data = data;

	if (data->entry_func)
		data->entry_func(data->param);

	s_thread_data = 0;
	if (data->detached) {
		::CloseHandle(data->handle);
		CY_FREE(data);
	}
	_endthreadex(0);
	return 0;
}
#else
//-------------------------------------------------------------------------------------
static void* __pthread_thread_entry(void* param)
{
	thread_data_s* data = (thread_data_s*)param;
	s_thread_data = data;

	data->tid = _nativeThreadID();

	if (data->entry_func)
		data->entry_func(data->param);

	s_thread_data = 0;
	if (data->detached) {
		CY_FREE(data);
	}
	pthread_exit(0);
	return 0;
}
#endif

//-------------------------------------------------------------------------------------
thread_t _thread_create(thread_function func, void* param, const char* name, bool detached)
{
	thread_data_s* data = (thread_data_s*)CY_MALLOC(sizeof(*data));
	data->tid = 0;
	data->param = param;
	data->entry_func = func;
	data->handle = 0;
	data->detached = detached;
	if (name != 0)
		strncpy(data->name, name, MAX_PATH);
	else
		data->name[0] = 0;

#ifdef CY_SYS_WINDOWS
	unsigned int thread_id;
	HANDLE hThread = (HANDLE)::_beginthreadex(0, 0,
		__win32_thread_entry, (void*)(data),
		THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME, &thread_id);
	
	if (hThread < 0) {
		CY_FREE(data);
		return 0;
	}
	data->handle = hThread;
	data->tid = thread_id;
	::ResumeThread(hThread);
	return data;
#else
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if(detached)
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thread;
	int ret = pthread_create(&thread, &attr, __pthread_thread_entry, data);
	pthread_attr_destroy(&attr);

	if(ret){
		CY_FREE(data);
		return 0;
	}
	data->handle = thread;
	while(data->tid == 0); //make sure we got the pid(BUSY LOOP, BUT IT IS VERY SHORT)
	return data;
#endif
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
#ifdef CY_SYS_WINDOWS
	::Sleep(msec);
#else
	struct timespec ts = { 0, 0 };
	ts.tv_sec = static_cast<time_t>(msec / 1000);
	ts.tv_nsec = static_cast<long>((msec % 1000) * 1000*1000);
	::nanosleep(&ts, NULL);
#endif
}

//-------------------------------------------------------------------------------------
void thread_join(thread_t thread)
{
	thread_data_s* data = (thread_data_s*)thread;
#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(data->handle, INFINITE);
	::CloseHandle(data->handle);
#else
	pthread_join(data->handle, 0);
#endif
	if (!(data->detached)) {
		CY_FREE(data);
	}
}

//-------------------------------------------------------------------------------------
const char* thread_get_current_name(void)
{
	return s_thread_data == 0 ? "<UNNAME>" : s_thread_data->name;
}

//-------------------------------------------------------------------------------------
void thread_yield(void)
{
#ifdef CY_SYS_WINDOWS
	Sleep(0);
#else
	sched_yield();
#endif
}

//-------------------------------------------------------------------------------------
mutex_t mutex_create(void)
{
#ifdef CY_SYS_WINDOWS
	LPCRITICAL_SECTION cs = (LPCRITICAL_SECTION)CY_MALLOC(sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(cs);
	return cs;
#else
	pthread_mutex_t* pm = (pthread_mutex_t*)CY_MALLOC(sizeof(pthread_mutex_t));
	::pthread_mutex_init(pm, 0);
	return pm;
#endif
}

//-------------------------------------------------------------------------------------
void mutex_destroy(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::DeleteCriticalSection(m);
	CY_FREE(m);
#else
	::pthread_mutex_destroy(m);
	CY_FREE(m);
#endif
}

//-------------------------------------------------------------------------------------
void mutex_lock(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::EnterCriticalSection(m);
#else
	::pthread_mutex_lock(m);
#endif
}

//-------------------------------------------------------------------------------------
void mutex_unlock(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::LeaveCriticalSection(m);
#else
	::pthread_mutex_unlock(m);
#endif
}

//-------------------------------------------------------------------------------------
#ifndef CY_SYS_WINDOWS
struct signal_s
{
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	atomic_int32_t  predicate;
};
#endif

//-------------------------------------------------------------------------------------
signal_t signal_create(void)
{
#ifdef CY_SYS_WINDOWS
	return ::CreateEvent(0, FALSE, FALSE, 0);
#else
	signal_s *sig = (signal_s*)CY_MALLOC(sizeof(*sig));
	sig->predicate = 0;
	pthread_mutex_init(&(sig->mutex), 0);
	pthread_cond_init(&(sig->cond), 0);
	return (signal_t)sig;
#endif
}

//-------------------------------------------------------------------------------------
void signal_destroy(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::CloseHandle(s);
#else
	signal_s* sig = (signal_s*)s;
	pthread_cond_destroy(&(sig->cond));
	pthread_mutex_destroy(&(sig->mutex));
	CY_FREE(sig);
#endif
}

//-------------------------------------------------------------------------------------
void signal_wait(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(s, INFINITE);
#else
	signal_s* sig = (signal_s*)s;
	pthread_mutex_lock(&(sig->mutex));
	while (0==sig->predicate.load()) {
		pthread_cond_wait(&(sig->cond), &(sig->mutex));
	}
	sig->predicate = 0;
	pthread_mutex_unlock(&(sig->mutex));
#endif
}

//-------------------------------------------------------------------------------------
#ifndef CY_SYS_WINDOWS
bool _signal_unlock_wait(signal_s* sig, uint32_t ms)
{
	const uint64_t kNanoSecondsPerSecond = 1000ll * 1000ll * 1000ll;

	if (sig->predicate.load() == 1) { //It's light!
		sig->predicate = 0;
		return true;
	}

	//need wait...
	if (ms == 0) return  false;	//zero-timeout event state check optimization

	timeval tv;
	gettimeofday(&tv, 0);
	uint64_t nanoseconds = ((uint64_t)tv.tv_sec) * kNanoSecondsPerSecond + ms * 1000 * 1000 + ((uint64_t)tv.tv_usec) * 1000;

	timespec ts;
	ts.tv_sec = (time_t)(nanoseconds / kNanoSecondsPerSecond);
	ts.tv_nsec = (long int)(nanoseconds - ((uint64_t)ts.tv_sec) * kNanoSecondsPerSecond);
	
	//wait...
	while(0 == sig->predicate.load()) {
		if (pthread_cond_timedwait(&(sig->cond), &(sig->mutex), &ts) != 0)
			return false; //time out
	}

	sig->predicate = 0;
	return true;
}
#endif

//-------------------------------------------------------------------------------------
bool signal_timewait(signal_t s, uint32_t ms)
{
#ifdef CY_SYS_WINDOWS
	return (WAIT_OBJECT_0 == ::WaitForSingleObject(s, ms));
#else
	signal_s* sig = (signal_s*)s;
	if (ms == 0) {
		if (EBUSY == pthread_mutex_trylock(&(sig->mutex)))
			return false;
	}
	else {
		pthread_mutex_lock(&(sig->mutex));
	}

	bool ret = _signal_unlock_wait(sig, ms);

	pthread_mutex_unlock(&(sig->mutex));
	return ret;
#endif
}

//-------------------------------------------------------------------------------------
void signal_notify(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::SetEvent(s);
#else
	signal_s* sig = (signal_s*)s;
	pthread_mutex_lock(&(sig->mutex));
	sig->predicate = 1;
	pthread_cond_signal(&(sig->cond));
	pthread_mutex_unlock(&(sig->mutex));
#endif
}

//-------------------------------------------------------------------------------------
int64_t time_now(void)
{
	const int64_t kMicroSecondsPerSecond = 1000ll * 1000ll;

#ifdef CY_SYS_WINDOWS
	SYSTEMTIME ltm;
	GetLocalTime(&ltm);

	struct tm t;
	t.tm_year = ltm.wYear - 1900;
	t.tm_mon = ltm.wMonth - 1;
	t.tm_mday = ltm.wDay;
	t.tm_hour = ltm.wHour;
	t.tm_min = ltm.wMinute;
	t.tm_sec = ltm.wSecond;
	t.tm_isdst = -1;

	int64_t seconds = (int64_t)mktime(&t);
	return seconds*kMicroSecondsPerSecond + ltm.wMilliseconds * 1000;

#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	int64_t seconds = tv.tv_sec;
	return seconds * kMicroSecondsPerSecond + tv.tv_usec;
#endif
}

//-------------------------------------------------------------------------------------
void time_now(char* time_dest, size_t max_size, const char* format)
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
uint32_t get_cpu_counts(void)
{
	const uint32_t DEFAULT_CPU_COUNTS = 2;

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

	uint32_t cpu_counts = 0;

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
		CY_LOG(L_ERROR, "get cpu counts \"sysconf(_SC_NPROCESSORS_ONLN)\" error, use default(2)");
		return DEFAULT_CPU_COUNTS;
	}
	return (uint32_t)cpu_counts;
#endif
}

}
}
