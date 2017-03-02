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
	sys_api::mutex_t lock;
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
		if(readlink("/proc/self/exe", process_path_name, 256)<0) {
			strncpy(process_path_name, "unknown", 256);
		}
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

		//host name
		char host_name[256] = { 0 };
		::gethostname(host_name, sizeof(host_name));

		snprintf(file_name, _MAX_PATH, LOG_PATH"%s.%s.%s.%d.log",
			process_name, timebuf, host_name, process_id);

#endif
		//create lock
		lock = sys_api::mutex_create();

		//default level(all level will be writed)
		level_threshold = L_TRACE;

		//log path didn't created
		logpath_created = false;

		//set level name
		level_name[L_TRACE] = "[T]";
		level_name[L_DEBUG] = "[D]";
		level_name[L_INFO]  = "[I]";
		level_name[L_WARN]  = "[W]";
		level_name[L_ERROR] = "[E]";
		level_name[L_FATAL] = "[F]";
	}
};

//-------------------------------------------------------------------------------------
static DiskLogFile& _get_disk_log(void)
{
	static DiskLogFile thefile;
	return thefile;
}

//-------------------------------------------------------------------------------------
const char* get_logfile_name(void)
{
	DiskLogFile& thefile = _get_disk_log();
	return thefile.file_name;
}

//-------------------------------------------------------------------------------------
void set_log_threshold(LOG_LEVEL level)
{
	assert(level >= 0 && level <= L_MAXIMUM_LEVEL);
	if (level < 0 || level > L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _get_disk_log();
	sys_api::auto_mutex guard(thefile.lock);

	thefile.level_threshold = level;
}

//-------------------------------------------------------------------------------------
void disk_log(LOG_LEVEL level, const char* message, ...)
{
	assert(level >= 0 && level < L_MAXIMUM_LEVEL);
	if (level < 0 || level >= L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _get_disk_log();
	sys_api::auto_mutex guard(thefile.lock);

	//check the level
	if (level < thefile.level_threshold) return;

	//check dir
#ifdef CY_SYS_WINDOWS
	if (!thefile.logpath_created && PathFileExists(LOG_PATH)!=TRUE) {
		if (0 == CreateDirectory(LOG_PATH, NULL)) 
#else
	if (!thefile.logpath_created && access(LOG_PATH, F_OK)!=0) {
		if (mkdir(LOG_PATH, 0755) != 0) 
#endif
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

	char timebuf[32] = { 0 };
#ifdef CY_SYS_WINDOWS
	//current time
	SYSTEMTIME time;
	::GetLocalTime(&time);

	snprintf(timebuf, sizeof(timebuf), "%04d_%02d_%02d-%02d:%02d:%02d",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
#else
	time_t t = time(0);
	struct tm tm_now;
	localtime_r(&t, &tm_now);

	strftime(timebuf, sizeof(timebuf), "%Y_%m_%d-%H:%M:%S", &tm_now);
#endif

	static const int32_t STATIC_BUF_LENGTH = 2048;

	char szTemp[STATIC_BUF_LENGTH] = { 0 };
	char* p = szTemp;
	va_list ptr; va_start(ptr, message);
	int len = vsnprintf(p, STATIC_BUF_LENGTH, message, ptr);
	if (len < 0) {
		va_start(ptr, message);
		len = vsnprintf(0, 0, message, ptr);
		if (len > 0) {
			p = (char*)CY_MALLOC(len + 1);
			va_start(ptr, message);
			vsnprintf(p, (size_t)len + 1, message, ptr);
			p[len] = 0;
		}
	}
	else if (len >= STATIC_BUF_LENGTH) {
		p = (char*)CY_MALLOC(len + 1);
		va_start(ptr, message);
		vsnprintf(p, (size_t)len + 1, message, ptr);
		p[len] = 0;
	}
	va_end(ptr);

	fprintf(fp, "%s %s [%s] %s\n",
		timebuf, 
		thefile.level_name[level],
		sys_api::thread_get_current_name(),
		p);
	fclose(fp);

	//print to stand output last
	fprintf(level >= L_ERROR ? stderr : stdout, "%s %s [%s] %s\n",
		timebuf,
		thefile.level_name[level],
		sys_api::thread_get_current_name(),
		p);

	if (p != szTemp) {
		CY_FREE(p);
	}
}



}
