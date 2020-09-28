﻿/*
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
class ServerMasterThread;
class ServerWorkThread;

class TcpServer : noncopyable
{
public:
	typedef std::function<void(TcpServer* server, Looper* looper)> MasterThreadStartCallback;
	typedef std::function<void(TcpServer* server, Packet* cmd)> MasterThreadCommandCallback;

	typedef std::function<void(TcpServer* server, int32_t thread_index, Looper* looper)> WorkThreadStartCallback;
	typedef std::function<void(TcpServer* server, int32_t thread_index, Packet* cmd)> WorkThreadCommandCallback;

	typedef std::function<void(TcpServer* server, int32_t thread_index, ConnectionPtr conn)> EventCallback;

	struct Listener {
		MasterThreadStartCallback on_master_thread_start;
		MasterThreadCommandCallback on_master_thread_command;

		WorkThreadStartCallback on_work_thread_start;
		WorkThreadCommandCallback on_work_thread_command;

		EventCallback on_connected;
		EventCallback on_message;
		EventCallback on_close;
	};

	Listener m_listener;
public:
	/// add a bind port, return false means too much port has been binded or bind failed
	// NOT thread safe, and this function must be called before start the server
	bool bind(const Address& bind_addr, bool enable_reuse_port);

	/// start the server(start one accept thread and n work threads)
	/// (thread safe, but you wouldn't want call it again...)
	bool start(int32_t work_thread_counts);

	/// wait server to terminate(thread safe)
	void join(void);

	/// stop the server gracefully 
	//(NOT thread safe, you can't call this function in any work thread)
	void stop(void);

	/// shutdown one of connection(thread safe)
	void shutdown_connection(ConnectionPtr conn);

	/// get bind address, if index is invalid return default Address value
	Address get_bind_address(size_t index);

	/// stop listen binded port(thread safe, after start the server)
	void stop_listen(size_t index);

	/// send message to master thread(thread safe)
	void send_master_message(uint16_t id, uint16_t size, const char* message);
	void send_master_message(const Packet* message);

	/// send work message to one of work thread(thread safe)
	void send_work_message(int32_t work_thread_index, const Packet* message);
	void send_work_message(int32_t work_thread_index, const Packet** message, int32_t counts);

	/// get work thread counts
	int32_t get_work_thread_counts(void) const { return m_workthread_counts; }

	int32_t get_next_connection_id(void) {
		return m_next_connection_id++;
	}

	/// print debug variable to debuger cache system
	void debug(void);

	/// get custom param
	void* get_param(void) const { return m_param; }

	/// get name
	const std::string& get_name(void) const { return m_name; }

private:
	enum { MAX_WORK_THREAD_COUNTS = 32 };

	/// custom param
	void* m_param;

	/// master thread
	ServerMasterThread* m_master_thread;

	/// work thread pool
	typedef std::vector< ServerWorkThread* > ServerWorkThreadArray;
	ServerWorkThreadArray m_work_thread_pool;

	int32_t			m_workthread_counts;
	atomic_int32_t	m_next_workthread_id;

	atomic_int32_t m_running;
	atomic_int32_t m_shutdown_ing;

	enum { kStartConnectionID = 1 };
	atomic_int32_t m_next_connection_id;

	std::string	m_name;

private:
	//called by master thread
	void _on_accept_socket(socket_t fd);

	friend class ServerMasterThread;
private:
	// called by server work thread only
	void _on_socket_connected(int32_t work_thread_index, ConnectionPtr conn);
	void _on_socket_message(int32_t work_thread_index, ConnectionPtr conn);
	void _on_socket_close(int32_t work_thread_index, ConnectionPtr conn);

	friend class ServerWorkThread;
public:
	TcpServer(const char* name, void* param = nullptr);
	~TcpServer();
};

}

#endif
