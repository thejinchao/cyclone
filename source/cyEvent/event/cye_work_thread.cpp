/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>

#include "cye_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
WorkThread::WorkThread()
	: m_listener(0)
	, m_thread(0)
	, m_looper(0)
	, m_message_queue_lock(0)
{
}

//-------------------------------------------------------------------------------------
WorkThread::~WorkThread()
{
	//TODO: stop the thread
}

//-------------------------------------------------------------------------------------
void WorkThread::start(const char* name, Listener* listener)
{
	assert(m_thread==0);
	assert(name && listener);

	m_listener = listener;

	work_thread_param param;
	param._this = this;

	m_message_queue_lock = sys_api::mutex_create();

	//run the work thread
	strncpy(m_name, (name ? name : "worker"), MAX_PATH);
	m_thread = sys_api::thread_create(_work_thread_entry, &param, m_name);

	//wait work thread ready signal
	while (param._ready.get() == 0) sys_api::thread_sleep(1);	//BUSY LOOP!
}

//-------------------------------------------------------------------------------------
void WorkThread::_work_thread_entry(void* param)
{
	work_thread_param* thread_param = (work_thread_param*)param;
	thread_param->_this->_work_thread(thread_param);
}

//-------------------------------------------------------------------------------------
void WorkThread::_work_thread(work_thread_param* param)
{
	//create work event looper
	m_looper = Looper::create_looper();

	//register pipe read event
	m_looper->register_event(m_pipe.get_read_port(), Looper::kRead, this,
		_on_message_entry, 0);

	// set work thread ready signal
	param->_ready.set(1);
	param = 0;//we don't use it again!

	//we start!
	if (m_listener)
	{
		if (!(m_listener->on_workthread_start())) 
		{
			Looper::destroy_looper(m_looper);
			m_looper = 0;
			return;
		}
	}

	//enter loop ...
	m_looper->loop();

	//delete the looper
	Looper::destroy_looper(m_looper);
	m_looper = 0;
}

//-------------------------------------------------------------------------------------
bool WorkThread::_on_message(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	for (;;)
	{
		uint16_t size;
		if (m_pipe.read((char*)&size, sizeof(size)) <= 0) break;

		Packet* packet = 0;
		{
			sys_api::auto_mutex lock(m_message_queue_lock);
			packet = m_message_queue.front();
			m_message_queue.pop_front();
		}
		//call listener
		if (m_listener->on_workthread_message(packet)) {
			return true;
		}

		delete packet;
	}
	return false;
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(uint16_t id, uint16_t size, const char* msg)
{
	Packet* message = new Packet();
	message->build(MESSAGE_HEAD_SIZE, id, size, msg);

	{
		sys_api::auto_mutex lock(this->m_message_queue_lock);
		m_message_queue.push_back(message);
		
		m_pipe.write((const char*)&size, sizeof(size));
	}
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(const Packet* message)
{
	Packet* packet = new Packet(*message);
	uint16_t size = message->get_packet_size();

	{
		sys_api::auto_mutex lock(this->m_message_queue_lock);
		m_message_queue.push_back(packet);

		m_pipe.write((const char*)&size, sizeof(size));
	}
}

//-------------------------------------------------------------------------------------
void WorkThread::join(void)
{
	sys_api::thread_join(m_thread);
}


}

