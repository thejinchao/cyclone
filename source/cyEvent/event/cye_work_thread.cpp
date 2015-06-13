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

	//run the work thread
	strncpy(m_name, (name ? name : "worker"), MAX_PATH);
	m_thread = thread_api::thread_create(_work_thread_entry, &param, m_name);

	//wait work thread ready signal
	while (param._ready.get() == 0) thread_api::thread_sleep(1);	//BUSY LOOP!
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
		m_listener->on_workthread_start();
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
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());

	Packet message;
	if (!message.build(MESSAGE_HEAD_SIZE, m_pipe)) return false;

	//call
	if (m_listener) {
		return m_listener->on_workthread_message(&message);
	}

	return false;
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(uint16_t id, uint16_t size, const char* msg)
{
	Packet message;
	message.build(MESSAGE_HEAD_SIZE, id, size, msg);

	m_pipe.write(message.get_memory_buf(), message.get_memory_size());
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(Packet* message)
{
	m_pipe.write(message->get_memory_buf(), message->get_memory_size());
}

//-------------------------------------------------------------------------------------
void WorkThread::join(void)
{
	thread_api::thread_join(m_thread);
}


}

