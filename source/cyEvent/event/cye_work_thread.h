/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_WORK_THREAD_H_
#define _CYCLONE_EVENT_WORK_THREAD_H_

namespace cyclone
{
//pre-define
class Packet;

class WorkThread
{
public:
	class Listener
	{
	public:
		virtual void on_workthread_start(void) = 0;
		virtual bool on_workthread_message(Packet*) = 0;
	};

public:
	enum { MESSAGE_HEAD_SIZE = 4 };

	//// run thread
	void start(const char* name, Listener* listener);

	//// send message to this work thread (thread safe)
	void send_message(uint16_t id, uint16_t size, const char* message);
	void send_message(const Packet* message);

	//// get work thread looper (thread safe)
	Looper* get_looper(void) const { return m_looper; }

	//// get work thread name (thread safe)
	const char* get_name(void) const { return m_name; }

	//// join work thread(thread safe)
	void join(void);

private:
	char			m_name[MAX_PATH];
	Listener*		m_listener;
	thread_t		m_thread;
	Looper*			m_looper;
	Pipe			m_pipe;

	typedef std::list<Packet*> MessageQueue;
	MessageQueue		m_message_queue;
	sys_api::mutex_t	m_message_queue_lock;

private:
	/// work thread param
	struct work_thread_param
	{
		WorkThread*		_this;
		atomic_int32_t	_ready;
	};
	/// work thread function
	static void _work_thread_entry(void* param);
	void _work_thread(work_thread_param* param);

	//// on work thread receive message
	static bool _on_message_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		return ((WorkThread*)param)->_on_message();
	}
	bool _on_message(void);

public:
	WorkThread();
	~WorkThread();
};

}

#endif
