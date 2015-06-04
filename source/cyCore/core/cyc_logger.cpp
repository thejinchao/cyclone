/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_logger.h"

#ifdef CY_SYS_WINDOWS
#include <Shlwapi.h>
#include <direct.h>
#elif defined(CY_SYS_MACOS)
#include <libproc.h>
#else
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace cyclone
{

#define LOG_PATH		"./logs/"

//-------------------------------------------------------------------------------------
struct DiskLogFile
{
#ifndef _MAX_PATH
#define _MAX_PATH (260)
#endif

	char file_name[_MAX_PATH];
	thread_api::mutex_t lock;
	const char* level_name[L_MAXIMUM_LEVEL];
	LOG_LEVEL level_threshold;
	bool logpath_created;

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
		socket_api::global_init();
		::gethostname(host_name, 256);

		//get process id
		DWORD process_id = ::GetCurrentProcessId();

		snprintf(file_name, _MAX_PATH, LOG_PATH"%s.%04d%02d%02d-%02d%02d%02d.%s.%d.log",
			process_name, 
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
			host_name, process_id);
#else
		//get process id
		pid_t process_id = ::getpid();

		//get process name
#ifdef CY_SYS_MACOS
		char process_path_name[PROC_PIDPATHINFO_MAXSIZE]={0};
		proc_pidpath(process_id, process_path_name, PROC_PIDPATHINFO_MAXSIZE);
#else
		char process_path_name[256] = { 0 };
		readlink("/proc/self/exe", process_path_name, 256);
#endif
		const char* process_name = strrchr(process_path_name, '/');
		if(process_name!=0) process_name++;
		else process_name="unknown";

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

		snprintf(file_name, _MAX_PATH, LOG_PATH"%s.%s.%s.%d.log",
			process_name, timebuf, host_name, process_id);

#endif
		//create lock
		lock = thread_api::mutex_create();

		//default level(all level will be writed)
		level_threshold = L_TRACE;

		//log path didn't created
		logpath_created = false;

		//set level name
		level_name[L_TRACE] = "TRACE";
		level_name[L_DEBUG] = "DEBUG";
		level_name[L_INFO] = "INFO";
		level_name[L_WARN] = "WARN";
		level_name[L_ERROR] = "ERROR";
		level_name[L_FATAL] = "FATAL";
	}
};

//-------------------------------------------------------------------------------------
static DiskLogFile& _get_disk_log(void)
{
	static DiskLogFile thefile;
	return thefile;
}

//-------------------------------------------------------------------------------------
void set_log_threshold(LOG_LEVEL level)
{
	assert(level >= 0 && level <= L_MAXIMUM_LEVEL);
	if (level < 0 || level > L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _get_disk_log();
	thread_api::auto_mutex guard(thefile.lock);

	thefile.level_threshold = level;
}

//-------------------------------------------------------------------------------------
void disk_log(LOG_LEVEL level, const char* message, ...)
{
	assert(level >= 0 && level < L_MAXIMUM_LEVEL);
	if (level < 0 || level >= L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _get_disk_log();
	thread_api::auto_mutex guard(thefile.lock);

	//check the level
	if (level < thefile.level_threshold) return;

	//check dir
	if (!thefile.logpath_created) {
		if (
#ifdef CY_SYS_WINDOWS
			_mkdir(LOG_PATH)
#else
			mkdir(LOG_PATH, 0755)
#endif
			 != 0)
		{
			//create log path failed!
			return;
		}
		thefile.logpath_created = true;
	}

	FILE* fp = fopen(thefile.file_name, "a");
	if (fp == 0) {
		//create the log file first
		fp = fopen(thefile.file_name, "w");
	}
	if (fp == 0) return;

	char szTemp[1024] = { 0 };
	va_list ptr; va_start(ptr, message);
	vsnprintf(szTemp, 1024, message, ptr);
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
