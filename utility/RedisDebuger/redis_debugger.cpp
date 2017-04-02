#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <cy_core.h>
#include <cy_crypt.h>

#include <hiredis/hiredis.h>

#include "redis_debugger.h"

//-------------------------------------------------------------------------------------
RedisDebuger::RedisDebuger()
	: m_enable(false)
	, m_thread_counts(0)
	, m_db_index(0)
	, m_debug_entry(nullptr)
	, m_redis_threads(0)
{
}

//-------------------------------------------------------------------------------------
RedisDebuger::~RedisDebuger()
{
	//TODO: close redis connection
}

//-------------------------------------------------------------------------------------
bool RedisDebuger::init_redis_thread(const char* redis_host, uint16_t redis_port, int32_t db_index, int32_t thread_counts)
{
	assert(m_thread_counts == 0);
	assert(thread_counts > 0 && redis_host && redis_host[0] && redis_port>0);

	m_thread_counts = thread_counts;
	m_redis_host = redis_host;
	m_redis_port = redis_port;
	m_db_index = db_index;

	//create work thread pool
	m_redis_threads = new RedisThread[thread_counts];
	for (auto i = 0; i < thread_counts; i++)
	{
		char name[128] = { 0 };
		snprintf(name, 128, "dbg_%d", i);
		RedisThread& thread = m_redis_threads[i];

		thread.debug = this;
		thread.index = i;
		thread.contex = 0;
		thread.ready = 0;
		thread.msg_signal = sys_api::signal_create();
		thread.thread = sys_api::thread_create(
		    std::bind(&RedisDebuger::_redis_thread_func, this, &(thread)), 0, name);

		//wait...
		while (m_redis_threads[i].ready == 0) sys_api::thread_yield();

		//check result
		if (thread.ready > 1) {
			CY_LOG(L_ERROR, "Connect to redis failed!");
			return false;
		}
	}

	m_enable = true;
	return true;
}

//-------------------------------------------------------------------------------------
bool RedisDebuger::init_monitor_thread(int32_t debug_fraq, debug_entry_func func, void* param)
{
	if (!m_enable) return false;

	m_debug_fraq = debug_fraq;
	m_debug_entry = func;
	//create monitor thread
	m_monitor_thread = sys_api::thread_create(
	    std::bind(&RedisDebuger::_monitor_thread_func, this, param), this, "monitor");

	return true;
}

//-------------------------------------------------------------------------------------
redisContext* RedisDebuger::_connect_to_redis(bool flushdb)
{
	//begin connect to redis
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	redisContext* contex = redisConnectWithTimeout(m_redis_host.c_str(), m_redis_port, timeout);
	if (contex == 0 || contex->err) {
		return 0;
	}
	//select db
	char select_cmd[32] = { 0 };
	snprintf(select_cmd, 32, "SELECT %d", m_db_index);

	redisReply* reply = (redisReply*)redisCommand(contex, select_cmd);
	freeReplyObject(reply);

	//clean current db
	if(flushdb) {
		reply = (redisReply*)redisCommand(contex, "FLUSHDB");
		freeReplyObject(reply);
	}

	return contex;
}

//-------------------------------------------------------------------------------------
void RedisDebuger::_redis_thread_func(RedisThread* param)
{
	CY_LOG(L_DEBUG, "debug[%d] thread start, connect redis %s:%d", param->index, m_redis_host.c_str(), m_redis_port);

	param->contex = _connect_to_redis(param->index==0);
	if(param->contex==0) {
		param->ready = 2;
		CY_LOG(L_DEBUG, "redis connect failed!");
		return;
	}
	
	CY_LOG(L_DEBUG, "redis connect ok!");
	param->ready = 1;

	//begin work
	for (;;)
	{
		//wait cmd
		sys_api::signal_wait(param->msg_signal);

		for (;;) {
			std::string* cmd;
			if (!(param->ms_queue.pop(cmd))) break;

			//process the packet
			redisReply* reply = (redisReply*)redisCommand(param->contex, cmd->c_str());
			freeReplyObject(reply);

			//delete the packet
			delete cmd;
		}
	}
}

//-------------------------------------------------------------------------------------
void RedisDebuger::set_debug_value(const char* key, int32_t value)
{
	if (!m_enable) return;

	char str_value[32];
	snprintf(str_value, 32, "%d", value);

	set_debug_value(key, str_value);
}

//-------------------------------------------------------------------------------------
void RedisDebuger::set_debug_value(const char* key, const char* value)
{
	if(!m_enable) return;

	char temp[2048] = { 0 };
	snprintf(temp, 2048, "SET %s %s", key, value);

	uint32_t adler = INITIAL_ADLER;
	int32_t index = adler32(adler, (const uint8_t*)key, strlen(key)) % m_thread_counts;
	RedisThread& thread = m_redis_threads[index];

	std::string* cmd = new std::string(temp);
	thread.ms_queue.push(cmd);
	sys_api::signal_notify(thread.msg_signal);
}

//-------------------------------------------------------------------------------------
void RedisDebuger::del_debug_value(const char* key)
{
	if (!m_enable) return;

	uint32_t adler = adler32(0, 0, 0);
	int32_t index = adler32(adler, (const uint8_t*)key, strlen(key)) % m_thread_counts;
	RedisThread& thread = m_redis_threads[index];

	char temp[MAX_PATH] = { 0 };
	snprintf(temp, MAX_PATH, "DEL %s", key);

	std::string* cmd = new std::string(temp);
	thread.ms_queue.push(cmd);
	sys_api::signal_notify(thread.msg_signal);
}

//-------------------------------------------------------------------------------------
void RedisDebuger::_monitor_thread_func(void* param)
{
	CY_LOG(L_DEBUG, "Monitor thread function start, debug per %d second(s)", m_debug_fraq);

	for (;;) {
	    if(m_debug_entry!=nullptr) {
	        m_debug_entry(param);
	    }

		sys_api::thread_sleep(m_debug_fraq);
	}
}
