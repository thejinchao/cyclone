/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_WORK_THREAD_H_
#define _CYCLONE_NETWORK_WORK_THREAD_H_

namespace cyclone
{
namespace network
{

class WorkThread
{
public:
	enum { kNewConnectionCmd=1, kCloseConnectionCmd=2 };
	//// thread safe
	event::Pipe& get_cmd_port(void) { return m_pipe; }
	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_index; }

private:
	const int32_t	m_index;
	TcpServer*		m_server;
	thread_t		m_thread;
	event::Looper*	m_looper;
	event::Pipe		m_pipe;

	typedef std::set< Connection* > ConnectionList;
	ConnectionList	m_connections;

	thread_api::signal_t	m_thread_ready;

private:
	/// work thread function
	static void _work_thread_entry(void* param){
		((WorkThread*)param)->_work_thread();
	}
	void _work_thread(void);

	//// on work thread receive new connection
	static void _on_command_entry(event::Looper::event_id_t id, socket_t fd, event::Looper::event_t event, void* param){
		((WorkThread*)param)->_on_command();
	}
	void _on_command(void);

public:
	WorkThread(TcpServer* server, int32_t index);
	~WorkThread();
};

}
}

#endif

