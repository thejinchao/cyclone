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
class ServerWorkThread;

class TcpServer : noncopyable
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
	Address get_bind_address(size_t index);

	/// send work message to one of work thread(thread safe)
	void send_work_message(int32_t work_thread_index, const Packet* message);
	void send_work_message(int32_t work_thread_index, const Packet** message, int32_t counts);

	/// get connection (NOT thread safe, MUST call in the work thread)
	Connection* get_connection(int32_t work_thread_index, int32_t conn_id);

	/// get work thread counts
	int32_t get_work_thread_counts(void) const { return m_work_thread_counts; }

	int32_t get_next_connection_id(void) {
		return m_next_connection_id++;
	}

	/// print debug variable to debuger cache system
	void debug(void);

public:
	class Listener
	{
	public:
		virtual void on_workthread_start(TcpServer* server, int32_t thread_index, Looper* looper) = 0;
		virtual void on_workthread_cmd(TcpServer* server, int32_t thread_index, Packet* cmd) = 0;

		virtual void on_connected(TcpServer* server, int32_t thread_index, Connection* conn) = 0;
		virtual void on_message(TcpServer* server, int32_t thread_index, Connection* conn) = 0;
		virtual void on_close(TcpServer* server, int32_t thread_index, Connection* conn) = 0;
	};

	Listener* get_listener(void) { 	return m_listener; }

private:
	enum { MAX_WORK_THREAD_COUNTS = 32 };
	typedef std::vector< socket_t > SocketVector;
	typedef std::vector< ServerWorkThread* > ServerWorkThreadArray;

	SocketVector	m_acceptor_sockets;
	Looper*			m_accept_looper;
	thread_t		m_acceptor_thread;

	ServerWorkThreadArray	m_work_thread_pool;
	int32_t			m_work_thread_counts;
	atomic_int32_t	m_next_work;

	int32_t _get_next_work_thread(void) { 
		return (m_next_work++) % m_work_thread_counts;
	}

	Listener* m_listener;

	atomic_int32_t m_running;
	atomic_int32_t m_shutdown_ing;

	atomic_int32_t m_next_connection_id;

	std::string	m_name;

	DebugInterface*	m_debuger;

private:
	/// accept thread function
	void _accept_thread(void);

	/// on acception callback function
	void _on_accept_function(Looper::event_id_t id, socket_t fd, Looper::event_t event);

public:
	TcpServer(Listener* listener, const char* name, DebugInterface* debuger);
	~TcpServer();
};

}

#endif
