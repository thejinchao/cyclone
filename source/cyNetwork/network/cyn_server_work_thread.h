/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_SERVER_WORK_THREAD_H_
#define _CYCLONE_NETWORK_SERVER_WORK_THREAD_H_

#include "cyn_connection.h"

namespace cyclone
{

class ServerWorkThread : noncopyable, public Connection::Owner
{
public:
	enum { kNewConnectionCmdID = 1, kCloseConnectionCmdID, kShutdownCmdID, kDebugCmdID };
	struct NewConnectionCmd
	{
		enum { ID = kNewConnectionCmdID };
		socket_t sfd;
	};

	struct CloseConnectionCmd
	{
		enum { ID = kCloseConnectionCmdID };
		int32_t conn_id;
		int32_t shutdown_ing;
	};

	struct ShutdownCmd
	{
		enum { ID = kShutdownCmdID };
	};

	struct DebugCmd
	{
		enum { ID = kDebugCmdID };
	};

public:
	//// send message to this work thread (thread safe)
	void send_message(uint16_t id, uint16_t size, const char* message);
	void send_message(const Packet* message);
	void send_message(const Packet** message, int32_t counts);

	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_index; }
	//// is current thread in work thread (thread safe)
	bool is_in_workthread(void) const;
	//// join work thread(thread safe)
	void join(void);
	//// get connection(NOT thread safe, MUST call in work thread)
	ConnectionPtr get_connection(int32_t connection_id);
	/// Connection Owner type
	virtual OWNER_TYPE get_connection_owner_type(void) const { return kServer; }

private:
	const int32_t	m_index;
	TcpServer*		m_server;
	WorkThread*		m_work_thread;

	typedef std::unordered_map< int32_t, ConnectionPtr > ConnectionMap;

	ConnectionMap	m_connections;

	std::string		m_name;
	DebugInterface*	m_debuger;

private:
	//// called by workthread
	bool _on_workthread_start(void);
	void _on_workthread_message(Packet*);

	void _debug(DebugCmd& cmd);
public:
	ServerWorkThread(int32_t index, TcpServer* server, const char* name, DebugInterface* debuger);
	virtual ~ServerWorkThread();
};

}

#endif

