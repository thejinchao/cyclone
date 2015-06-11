/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_WORK_THREAD_H_
#define _CYCLONE_NETWORK_WORK_THREAD_H_

namespace cyclone
{

class WorkThread : public Connection::Listener
{
public:
	enum { kNewConnectionCmd=1, kCloseConnectionCmd=2, kShutdownCmd=3 };
	//// thread safe
	Pipe& get_cmd_port(void) { return m_pipe; }
	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_index; }
	/// wait thread to termeinate(thread safe)
	thread_t get_thread(void) const { return m_thread; }

private:
	const int32_t	m_index;
	thread_t		m_thread;
	Looper*			m_looper;
	Pipe			m_pipe;
	TcpServer*		m_server;

	typedef std::set< Connection* > ConnectionList;
	ConnectionList	m_connections;

	thread_api::signal_t	m_thread_ready;

	char		m_name[MAX_PATH];

public:
	//// called by connection(in work thread)
	virtual void on_connection_event(Connection::Event event, Connection* conn);

private:
	/// work thread function
	static void _work_thread_entry(void* param){
		((WorkThread*)param)->_work_thread();
	}
	void _work_thread(void);

	//// on work thread receive new connection
	static bool _on_command_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		return ((WorkThread*)param)->_on_command();
	}
	bool _on_command(void);

public:
	WorkThread(int32_t index, TcpServer* server, const char* name);
	~WorkThread();

	//not-copyable
private:
	WorkThread & operator=(const WorkThread &) { return *this; }
};

}

#endif

