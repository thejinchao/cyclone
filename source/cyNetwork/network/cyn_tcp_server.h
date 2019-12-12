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
	typedef std::function<void(TcpServer* server, int32_t thread_index, Looper* looper)> WorkThreadStartCallback;
	typedef std::function<void(TcpServer* server, int32_t thread_index, Packet* cmd)> WorkThreadCommandCallback;
	typedef std::function<void(TcpServer* server, int32_t thread_index, ConnectionPtr conn)> EventCallback;

	struct Listener {
		WorkThreadStartCallback onWorkThreadStart;
		WorkThreadCommandCallback onWorkThreadCommand;
		EventCallback onConnected;
		EventCallback onMessage;
		EventCallback onClose;
	};

	Listener m_listener;
public:
	/// add a bind port, return false means too much port has been binded or bind failed
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
	void shutdown_connection(ConnectionPtr conn);

	/// get bind address, if index is invalid return default Address value
	Address get_bind_address(size_t index);

	/// stop listen binded port(thread safe, after start the server)
	void stop_listen(size_t index);

	/// send work message to one of work thread(thread safe)
	void send_work_message(int32_t work_thread_index, const Packet* message);
	void send_work_message(int32_t work_thread_index, const Packet** message, int32_t counts);

	/// get connection (NOT thread safe, MUST call in the work thread)
	ConnectionPtr get_connection(int32_t work_thread_index, int32_t conn_id);

	/// get work thread counts
	int32_t get_work_thread_counts(void) const { return m_work_thread_counts; }

	int32_t get_next_connection_id(void) {
		return m_next_connection_id++;
	}

	/// print debug variable to debuger cache system
	void debug(void);

private:
	enum { MAX_WORK_THREAD_COUNTS = 32 };
	typedef std::vector< std::tuple<socket_t, Looper::event_id_t> > SocketVector;
	typedef std::vector< ServerWorkThread* > ServerWorkThreadArray;

	SocketVector	m_acceptor_sockets;
	WorkThread		m_accept_thread;

	ServerWorkThreadArray	m_work_thread_pool;
	int32_t			m_work_thread_counts;
	atomic_int32_t	m_next_work;

	int32_t _get_next_work_thread(void) { 
		return (m_next_work++) % m_work_thread_counts;
	}

	atomic_int32_t m_running;
	atomic_int32_t m_shutdown_ing;

	atomic_int32_t m_next_connection_id;

	std::string	m_name;

	DebugInterface*	m_debuger;

private:
	//accept thread command
	enum { kShutdownCmdID=1, kDebugCmdID, kStopListenCmdID };
	struct ShutdownCmd
	{
		enum { ID = kShutdownCmdID };
	};

	struct DebugCmd
	{
		enum { ID = kDebugCmdID };
	};

	struct StopListenCmd
	{
		enum { ID = kStopListenCmdID };
		size_t index;
	};

	/// accept thread function start
	bool _on_accept_start(void);

	/// accept thread message
	void _on_accept_message(Packet*);

	/// on acception callback function
	void _on_accept_event(Looper::event_id_t id, socket_t fd, Looper::event_t event);

public:
	TcpServer(const char* name, DebugInterface* debuger);
	~TcpServer();
};

}

#endif
