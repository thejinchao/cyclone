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

	/// get callback param(thread safe)
	const void* get_callback_param(void) const { return m_callback_param; }

	/// get bind address, if index is invalid return default Address value
	Address get_bind_address(int index);

public:
	typedef void(*on_connection_callback)(TcpServer* server, Connection* conn);
	typedef void(*on_message_callback)(TcpServer* server, Connection* conn);
	typedef void(*on_close_callback)(TcpServer* server, Connection* conn);

	/// Set connection callback. (NOT thread safe)
	void set_connection_callback(on_connection_callback cb) { m_connection_cb = cb; }

	/// Set message callback. (NOT thread safe)
	void set_message_callback(on_message_callback cb)  { m_message_cb = cb; }

	//// Set close callback. (NOT thread safe)
	void set_close_callback(on_close_callback cb)  { m_close_cb = cb; }

public:
	//// called by connection(in work thread)
	static void _on_connection_event_entry(Connection::Event event, Connection* conn);
	void _on_connection_event(Connection::Event event, Connection* conn);

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

	const void*				m_callback_param;

	on_connection_callback	m_connection_cb;
	on_message_callback		m_message_cb;
	on_close_callback		m_close_cb;

	atomic_int32_t m_running;
	atomic_int32_t m_shutdown_ing;

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
	TcpServer(void* cb_param);
	~TcpServer();
};

}

#endif
