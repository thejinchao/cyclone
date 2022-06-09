/*
Copyright(C) thecodeway.com
*/
#pragma once

namespace cyclone
{
//pre-define
class Packet;

class WorkThread : noncopyable
{
public:
	typedef std::function<bool(void)> StartCallback;
	typedef std::function<void(Packet*)> MessageCallback;

public:
	enum { MESSAGE_HEAD_SIZE = 4 };

	//// run thread
	void start(const char* name);

	//// is work thread running?
	bool is_running(void) const { return m_thread != nullptr; }

	//// set callback function
	void set_on_start(StartCallback func) { m_on_start = func; }
	void set_on_message(MessageCallback func) { m_on_message = func; }

	//// send message to this work thread (thread safe)
	void send_message(uint16_t id, uint16_t size_part1, const char* msg_part1, uint16_t size_part2 = 0, const char* msg_part2 = nullptr);
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
	thread_t		m_thread;
	Looper*			m_looper;
	atomic_bool_t	m_is_queue_empty;
	Pipe			m_pipe;

	typedef LockFreeQueue<Packet*> MessageQueue;
	MessageQueue		m_message_queue;

	StartCallback	m_on_start;
	MessageCallback	m_on_message;

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

	/// wakeup the looper
	void _wakeup(void);
public:
	WorkThread();
	virtual ~WorkThread();
};

}
