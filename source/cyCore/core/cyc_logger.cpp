/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_logger.h"

#ifdef CY_SYS_WINDOWS
#include <Shlwapi.h>
#else
#include <stdarg.h>
#endif

namespace cyclone
{

//-------------------------------------------------------------------------------------
struct DiskLogFile
{
#ifndef _MAX_PATH
#define _MAX_PATH (260)
#endif

	char file_name[_MAX_PATH];
	thread_api::mutex_t lock;
	const char* level_name[NUM_LOG_LEVELS];

	DiskLogFile() 
	{
#ifdef CY_SYS_WINDOWS
		//get process name
		char process_path_name[256] = { 0 };
		::GetModuleFileName(::GetModuleHandle(0), process_path_name, 256);
		const char* process_name = ::PathFindFileNameA(process_path_name);

		//current time
		SYSTEMTIME time;
		::GetLocalTime(&time);
		
		//get host name
		char host_name[256];
		::gethostname(host_name, 256);

		//get process id
		DWORD process_id = ::GetCurrentProcessId();

		snprintf(file_name, _MAX_PATH, "%s.%04d%02d%02d-%02d%02d%02d.%s.%d.log",
			process_name, 
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
			host_name, process_id);
#else
		//get process name
		char process_path_name[256] = { 0 };
		readlink("/proc/self/exe", process_path_name, 256);
		const char* process_name = strrchr(process_path_name, '/')+1;

		//current time
		time_t t = time(0);
		struct tm tm_now;
		localtime_r(&t, &tm_now); 

		char timebuf[32];
		strftime(timebuf, sizeof(timebuf), "%Y%m%d-%H%M%S", &tm_now);
		//filename += timebuf;

		//host name
		char host_name[256] = { 0 };
		::gethostname(host_name, sizeof(host_name));

		//get process id
		pid_t process_id = ::getpid();

		snprintf(file_name, _MAX_PATH, "%s.%s.%s.%d.log",
			process_name, timebuf, host_name, process_id);

#endif
		//create lock
		lock = thread_api::mutex_create();

		//set level name
		level_name[L_TRACE] = "TRACE";
		level_name[L_DEBUG] = "DEBUG";
		level_name[L_INFO] = "INFO";
		level_name[L_WARN] = "WARN";
		level_name[L_ERROR] = "ERROR";
		level_name[L_FATAL] = "FATAL";

		//create the log file first
		FILE* fp = fopen(file_name, "w");
		fclose(fp);
	}
};

//-------------------------------------------------------------------------------------
static DiskLogFile& _get_disk_log(void)
{
	static DiskLogFile thefile;
	return thefile;
}

//-------------------------------------------------------------------------------------
void disk_log(LOG_LEVEL level, const char* message, ...)
{
	assert(level >= 0 && level < NUM_LOG_LEVELS);
	if (level < 0 || level >= NUM_LOG_LEVELS)return;

	DiskLogFile& thefile = _get_disk_log();
	thread_api::auto_mutex guard(thefile.lock);

	FILE* fp = fopen(thefile.file_name, "a");

	char szTemp[1024] = { 0 };
	va_list ptr; va_start(ptr, message);
	vsnprintf((char *)szTemp, 1024, message, ptr);
	va_end(ptr);

	fprintf(fp, "%s\t%s\n",
		thefile.level_name[level],
		szTemp);
	fclose(fp);

	//print to stand output last
	fprintf(level >= L_ERROR ? stderr : stdout, "%s\t%s\n", 
		thefile.level_name[level], 
		szTemp);
}



}
