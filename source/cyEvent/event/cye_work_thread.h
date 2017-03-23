/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_WORK_THREAD_H_
#define _CYCLONE_EVENT_WORK_THREAD_H_

#include "core/cyc_lf_queue.h"

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
		virtual bool on_workthread_start(void) = 0;
		virtual void on_workthread_message(Packet*) = 0;
	};

public:
	enum { MESSAGE_HEAD_SIZE = 4 };

	//// run thread
	void start(const char* name, Listener* listener);

	//// send message to this work thread (thread safe)
	void send_message(uint16_t id, uint16_t size, const char* message);
	void send_message(const Packet* message);
	void send_message(const Packet** message, int32_t counts);

	//// get work thread looper (thread safe)
	Looper* get_looper(void) const { return m_looper; }

	//// get work thread name (thread safe)
	const char* get_name(void) const { return m_name.c_str(); }

	//// join work thread(thread safe)
	void join(void);

private:
	std::string		m_name;
	Listener*		m_listener;
	thread_t		m_thread;
	Looper*			m_looper;
	Pipe			m_pipe;

	typedef LockFreeQueue<Packet*> MessageQueue;
	MessageQueue		m_message_queue;

private:
	/// work thread param
	struct work_thread_param
	{
		WorkThread*		_this;
		atomic_int32_t	_ready;
	};
	/// work thread function
	void _work_thread(void* param);

	//// on work thread receive message
	void _on_message(void);

public:
	WorkThread();
	virtual ~WorkThread();
};

}

#endif
