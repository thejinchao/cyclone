/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_SERVER_WORK_THREAD_H_
#define _CYCLONE_NETWORK_SERVER_WORK_THREAD_H_

#include <hash_map>

namespace cyclone
{

class ServerWorkThread 
	: public Connection::Listener
	, public WorkThread::Listener
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
	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_index; }
	//// is current thread in work thread (thread safe)
	bool is_in_workthread(void) const;
	//// join work thread(thread safe)
	void join(void);
	//// get connection(NOT thread safe, MUST call in work thread)
	Connection* get_connection(int32_t connection_id);

private:
	const int32_t	m_index;
	TcpServer*		m_server;
	WorkThread*		m_work_thread;

#ifdef CY_SYS_WINDOWS
	typedef std::hash_map< int32_t, Connection* > ConnectionMap;
#else
	typedef __gnu_cxx::hash_map< int32_t, Connection*> ConnectionMap;
#endif	

	ConnectionMap	m_connections;

	char			m_name[MAX_PATH];
	DebugInterface*	m_debuger;

private:
	//// called by connection(in work thread)
	virtual void on_connection_event(Connection::Event event, Connection* conn);
	//// called by message port (in work thread)
	virtual bool on_workthread_start(void);
	virtual bool on_workthread_message(Packet*);

	void _debug(DebugCmd& cmd);
public:
	ServerWorkThread(int32_t index, TcpServer* server, const char* name, DebugInterface* debuger);
	~ServerWorkThread();

	//not-copyable
private:
	ServerWorkThread & operator=(const ServerWorkThread &) { return *this; }
};

}

#endif

