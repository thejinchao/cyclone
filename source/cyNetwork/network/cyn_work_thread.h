/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_WORK_THREAD_H_
#define _CYCLONE_NETWORK_WORK_THREAD_H_

namespace cyclone
{

class WorkThread
{
public:
	enum { kNewConnectionCmd=1, kCloseConnectionCmd=2, kShutdownCmd=3 };
	//// thread safe
	Pipe& get_cmd_port(void) { return m_pipe; }
	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_index; }
	/// wait thread to termeinate(thread safe)
	thread_t get_thread(void) const { return m_thread; }
	/// get param
	void* get_param(void) const { return m_param; }

private:
	const int32_t	m_index;
	thread_t		m_thread;
	Looper*			m_looper;
	Pipe			m_pipe;
	void*			m_param;

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
	static bool _on_command_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		return ((WorkThread*)param)->_on_command();
	}
	bool _on_command(void);

public:
	WorkThread(int32_t index, void* param);
	~WorkThread();

	//not-copyable
private:
	WorkThread & operator=(const WorkThread &) { return *this; }
};

}

#endif

