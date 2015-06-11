/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_TCP_SERVER_H_
#define _CYCLONE_NETWORK_TCP_SERVER_H_

#include <cy_core.h>
#include <cy_event.h>
#include "cyn_connection.h"

namespace cyclone
{

//pre-define
class WorkThread;

class TcpServer
{
public:
	/// add a bind port, return false means too much port has beed binded or bind failed
	// NOT thread safe, and this function must be called before start the server
	bool bind(const Address& bind_addr, bool enable_reuse_port);

	/// start the server(start one accept thread and n workthreads)
	/// (thread safe, but you wouldn't want call it again...)
	bool start(int32_t work_thread_counts);

	/// wait server to termeinate(thread safe)
	void join(void);

	/// stop the server gracefully 
	//(NOT thread safe, you can't call this function in any work thread)
	void stop(void);

	/// shutdown one of connection(thread safe)
	void shutdown_connection(Connection* conn); 

	/// get bind address, if index is invalid return default Address value
	Address get_bind_address(int index);

public:
	class Listener
	{
	public:
		virtual void on_connection_callback(TcpServer* server, Connection* conn) = 0;
		virtual void on_message_callback(TcpServer* server, Connection* conn) = 0;
		virtual void on_close_callback(TcpServer* server, Connection* conn) = 0;
	};

	Listener* get_listener(void) { 	return m_listener; }

private:
	enum { MAX_BIND_PORT_COUNTS = 128, MAX_WORK_THREAD_COUNTS = 32 };

	Socket*			m_acceptor_socket[MAX_BIND_PORT_COUNTS];

	thread_t		m_acceptor_thread;

	WorkThread*		m_work_thread_pool[MAX_WORK_THREAD_COUNTS];
	int32_t			m_work_thread_counts;
	int32_t			m_next_work;

	int32_t _get_next_work_thread(void) { 
		return m_next_work = (m_next_work + 1) % m_work_thread_counts;
	}

	Listener* m_listener;

	atomic_int32_t m_running;
	atomic_int32_t m_shutdown_ing;

	char	m_name[MAX_PATH];

private:
	/// accept thread function
	static void _accept_thread_entry(void* param){
		((TcpServer*)param)->_accept_thread();
	}
	void _accept_thread(void);

	/// on acception callback function
	static bool _on_accept_function_entry(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param){
		return ((TcpServer*)param)->_on_accept_function(id, fd, event);
	}
	bool _on_accept_function(Looper::event_id_t id, socket_t fd, Looper::event_t event);

public:
	TcpServer(Listener* listener, const char* name);
	~TcpServer();
};

}

#endif
