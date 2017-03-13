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
	param._ready = 0;

	//run the work thread
	strncpy(m_name, (name ? name : "worker"), MAX_PATH);
	m_thread = sys_api::thread_create(
		std::bind(&WorkThread::_work_thread, this, std::placeholders::_1), &param, m_name);

	//wait work thread ready signal
	while (param._ready == 0) sys_api::thread_yield();	//BUSY LOOP!
}

//-------------------------------------------------------------------------------------
void WorkThread::_work_thread(void* param)
{
	work_thread_param* thread_param = (work_thread_param*)param;

	//create work event looper
	m_looper = Looper::create_looper();

	//register pipe read event
	m_looper->register_event(m_pipe.get_read_port(), Looper::kRead, this,
		std::bind(&WorkThread::_on_message, this), 0);

	// set work thread ready signal
	thread_param->_ready = 1;
	thread_param = nullptr;//we don't use it again!

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
void WorkThread::_on_message(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	for (;;)
	{
		int32_t counts;
		if (m_pipe.read((char*)&counts, sizeof(counts)) <= 0) break;
		assert(counts > 0);

		for (int32_t i = 0; i < counts; i++) {
			Packet* packet = 0;
			if (!m_message_queue.pop(packet)) {
				assert(false && "WorkThread message queue error");
				break;
			}

			//call listener
			m_listener->on_workthread_message(packet);

			Packet::free_packet(packet);
		}
	}
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(uint16_t id, uint16_t size, const char* msg)
{
	Packet* packet = Packet::alloc_packet();
	packet->build(MESSAGE_HEAD_SIZE, id, size, msg);

	m_message_queue.push(packet);
		
	int32_t counts = 1;
	m_pipe.write((const char*)&counts, sizeof(counts));
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(const Packet* message)
{
	Packet* packet = Packet::alloc_packet(message);
	m_message_queue.push(packet);
	
	int32_t counts = 1;
	m_pipe.write((const char*)&counts, sizeof(counts));
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(const Packet** message, int32_t counts)
{
	for (int32_t i = 0; i < counts; i++){
		Packet* packet = Packet::alloc_packet(message[i]);
		m_message_queue.push(packet);
	}
	m_pipe.write((const char*)&counts, sizeof(counts));
}

//-------------------------------------------------------------------------------------
void WorkThread::join(void)
{
	sys_api::thread_join(m_thread);
}


}

