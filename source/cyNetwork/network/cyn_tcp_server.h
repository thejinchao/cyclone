/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_TCP_SERVER_H_
#define _CYCLONE_NETWORK_TCP_SERVER_H_

#include <cyclone_config.h>
#include <cy_event.h>
#include <network/cyn_socket.h>
#include <network/cyn_address.h>

namespace cyclone
{
namespace network
{

//pre-define
class WorkThread;
class Connection;

class TcpServer
{
public:
	/// start the server(start one accept thread and n workthreads)
	/// (thread safe, but you wouldn't want call it again...)
	void start(int32_t work_thread_counts);

	/// wait server to termeinate(thread safe)
	void join(void);

	/// shutdown one of connection(thread safe)
	void shutdown_connection(Connection* conn); 

public:
	typedef void(*on_connection_callback)(TcpServer* server, Connection* conn);
	typedef void(*on_message_callback)(TcpServer* server, Connection* conn);
	typedef void(*on_close_callback)(TcpServer* server, Connection* conn);

	/// Set/Get connection callback. (NOT thread safe)
	void set_connection_callback(on_connection_callback cb) { m_connection_cb = cb; }
	on_connection_callback get_connection_callback(void) { return m_connection_cb; }

	/// Set message callback. (NOT thread safe)
	void set_message_callback(on_message_callback cb)  { m_message_cb = cb; }
	on_message_callback get_message_callback(void)  { return m_message_cb; }

	//// Set close callback. (NOT thread safe)
	void set_close_callback(on_close_callback cb)  { m_close_cb = cb; }
	on_close_callback get_close_callback(void) { return m_close_cb; }

private:
	enum { MAX_WORK_THREAD_COUNTS = 32 };

	Socket			m_acceptor_socket;
	Address			m_address;
	thread_t		m_acceptor_thread;

	WorkThread*		m_work_thread_pool[MAX_WORK_THREAD_COUNTS];
	int32_t			m_work_thread_counts;
	int32_t			m_next_work;

	int32_t _get_next_work_thread(void) { 
		return m_next_work = (m_next_work + 1) % m_work_thread_counts;
	}

	on_connection_callback	m_connection_cb;
	on_message_callback		m_message_cb;
	on_close_callback		m_close_cb;

private:
	/// accept thread function
	static void _accept_thread_entry(void* param){
		((TcpServer*)param)->_accept_thread();
	}
	void _accept_thread(void);

	/// on acception callback function
	static void _on_accept_function_entry(event::Looper::event_id_t id, socket_t fd, event::Looper::event_t event, void* param){
		((TcpServer*)param)->_on_accept_function(id, fd, event);
	}
	void _on_accept_function(event::Looper::event_id_t id, socket_t fd, event::Looper::event_t event);

public:
	TcpServer(const Address& addr);
	~TcpServer();
};

}
}

#endif
