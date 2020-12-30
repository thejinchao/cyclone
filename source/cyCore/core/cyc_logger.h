/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{

enum LOG_LEVEL
{
	L_TRACE,
	L_DEBUG,
	L_INFO,
	L_WARN,
	L_ERROR,
	L_FATAL,

	L_MAXIMUM_LEVEL,
};

//----------------------
// log api
//----------------------

//log to a disk file
//default filename = process_name.date-time24h.hostname.pid.log
// like "test.20150302-1736.server1.63581.log"
// the time part is the time(LOCAL) of first log be written
void disk_log(LOG_LEVEL level, const char* message, ...);

//set current log filename
bool set_log_filename(const char* pathName, const char* fileName);

//get current log filename
const char* get_log_filename(void);

//set the a global log level, default is L_DEBUG, 
//all the log message lower than this level will be ignored
void set_log_threshold(LOG_LEVEL level);

}

//useful macro
#ifdef CY_ENABLE_LOG
#define CY_LOG cyclone::disk_log
#else
#define CY_LOG (void)
#endif
