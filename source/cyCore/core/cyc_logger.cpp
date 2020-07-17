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
		socket_api::global_init();
#endif
		//get process name
		char process_name[256] = { 0 };
		sys_api::process_get_module_name(process_name, 256);
		
		//get host name
		char host_name[256];
		::gethostname(host_name, 256);

		//get process id
		pid_t process_id = sys_api::process_get_id();

		//log filename patten
		char name_patten[256] = { 0 };
		std::snprintf(name_patten, 256, LOG_PATH"%s.%%Y%%m%%d-%%H%%M%%S.%s.%d.log", process_name, host_name, process_id);
		sys_api::local_time_now(file_name, 256, name_patten);

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
static DiskLogFile& _getDiskLog(void)
{
	static DiskLogFile thefile;
	return thefile;
}

//-------------------------------------------------------------------------------------
const char* getLogFileName(void)
{
	DiskLogFile& thefile = _getDiskLog();
	return thefile.file_name;
}

//-------------------------------------------------------------------------------------
void setLogThreshold(LOG_LEVEL level)
{
	assert(level >= 0 && level <= L_MAXIMUM_LEVEL);
	if (level < 0 || level > L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _getDiskLog();
	sys_api::auto_mutex guard(thefile.lock);

	thefile.level_threshold = level;
}

//-------------------------------------------------------------------------------------
void diskLog(LOG_LEVEL level, const char* message, ...)
{
	assert(level >= 0 && level < L_MAXIMUM_LEVEL);
	if (level < 0 || level >= L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _getDiskLog();
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
	sys_api::local_time_now(timebuf, 32, "%Y_%m_%d-%H:%M:%S");

	static const int32_t STATIC_BUF_LENGTH = 2048;

	char szTemp[STATIC_BUF_LENGTH] = { 0 };
	char* p = szTemp;
	va_list ptr; va_start(ptr, message);
	int len = vsnprintf(p, STATIC_BUF_LENGTH, message, ptr);
	if (len < 0) {
		va_start(ptr, message);
		len = vsnprintf(0, 0, message, ptr);
		if (len > 0) {
			p = (char*)CY_MALLOC((size_t)(len + 1));
			va_start(ptr, message);
			vsnprintf(p, (size_t)len + 1, message, ptr);
			p[len] = 0;
		}
	}
	else if (len >= STATIC_BUF_LENGTH) {
		p = (char*)CY_MALLOC((size_t)(len + 1));
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
