#pragma once

#include <cy_core.h>

using namespace cyclone;
struct redisContext;

/*
Usage:

    ...
    RedisDebuger redisDebuger;
    TcpServer server(&listener, "name", &redisDebuger);
    ...
    server.start()
    ...
    redisDebuger.init_redis_thread("127.0.0.1", 6379, 9, 2);
    redisDebuger.init_monitor_thread(1000, [](void* param) {
        ((TcpServer*)param)->debug();
    }, &server);
*/

/*
*HOW TO* print all key and value of debug db(no.9 db)?

echo -e "SELECT 9\nKEYS *" | redis-cli | awk '{if(NR==1){print "SELECT 9"} else { print "echo " $0 " \n GET " $0}}' | redis-cli | tail -n +2 | awk '{a=$1;getline;print a "\t" $1;next}'

*/
class RedisDebuger : public DebugInterface
{
public:
    typedef std::function<void(void*)> debug_entry_func;

	bool init_redis_thread(const char* redis_host, uint16_t redis_port, int32_t db_index, int32_t thread_counts);
	bool init_monitor_thread(int32_t debug_fraq, debug_entry_func func, void* param);

	virtual bool is_enable(void) { return m_enable; }
	virtual void update_value(const char* key, const char* value);
	virtual void update_value(const char* key, int32_t value);
	virtual void del_value(const char* key);

private:
	bool		m_enable;
	int32_t		m_thread_counts;
	std::string m_redis_host;
	int16_t		m_redis_port;
	int32_t		m_db_index;
	int32_t		m_debug_fraq;
    debug_entry_func m_debug_entry;
    
	typedef LockFreeQueue<std::string*> CmdQueue;

	struct RedisThread
	{
		RedisDebuger*	debug;
		int32_t			index;
		thread_t		thread;
		redisContext*	contex;
		atomic_int32_t	ready;

		CmdQueue			ms_queue;
		sys_api::signal_t	msg_signal;

	};
	RedisThread*	m_redis_threads;

	thread_t		m_monitor_thread;

private:
	void _redis_thread_func(RedisThread* param);
	void _monitor_thread_func(void*);

	redisContext* _connect_to_redis(bool flushdb);

public:
	RedisDebuger();
	~RedisDebuger();
};
