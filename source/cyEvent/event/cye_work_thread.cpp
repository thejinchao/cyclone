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
	: m_thread(nullptr)
	, m_looper(nullptr)
	, m_is_queue_empty(true)
	, m_on_start(nullptr)
	, m_on_message(nullptr)
{
}

//-------------------------------------------------------------------------------------
WorkThread::~WorkThread()
{
	//TODO: stop the thread
}

//-------------------------------------------------------------------------------------
void WorkThread::start(const char* name)
{
	assert(m_thread== nullptr);
	assert(name);

	work_thread_param param;
	param._this = this;
	param._ready = 0;

	//run the work thread
	m_name = name ? name : "worker";
	m_thread = sys_api::thread_create(
		std::bind(&WorkThread::_work_thread, this, std::placeholders::_1), &param, m_name.c_str());

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
	if (m_on_start && !m_on_start()) {
		Looper::destroy_looper(m_looper);
		m_looper = nullptr;
		return;
	}

	//enter loop ...
	m_looper->loop();

	//delete the looper
	Looper::destroy_looper(m_looper);
	m_looper = nullptr;
}

//-------------------------------------------------------------------------------------
void WorkThread::_on_message(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	for (;;) {
		int8_t dummy;
		if (m_pipe.read((char*)&dummy, sizeof(dummy)) <= 0) break;

		for (;;) {
			Packet* packet = nullptr;
			if (!m_message_queue.pop(packet)) break;

			//call listener
			if (m_on_message) {
				m_on_message(packet);
			}

			Packet::free_packet(packet);
		}
	}

	//set empty flag
	m_is_queue_empty = true;
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(uint16_t id, uint16_t size_part1, const char* msg_part1, uint16_t size_part2, const char* msg_part2)
{
	Packet* packet = Packet::alloc_packet();
	packet->build_from_memory(MESSAGE_HEAD_SIZE, id, size_part1, msg_part1, size_part2, msg_part2);

	m_message_queue.push(packet);

	_wakeup();
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(const Packet* message)
{
	Packet* packet = Packet::alloc_packet(message);
	m_message_queue.push(packet);
	
	_wakeup();
}

//-------------------------------------------------------------------------------------
void WorkThread::send_message(const Packet** message, int32_t counts)
{
	for (int32_t i = 0; i < counts; i++){
		Packet* packet = Packet::alloc_packet(message[i]);
		m_message_queue.push(packet);
	}

	_wakeup();
}

//-------------------------------------------------------------------------------------
void WorkThread::join(void)
{
	if (m_thread != nullptr) {
		sys_api::thread_join(m_thread);
		m_thread = nullptr;
	}
}

//-------------------------------------------------------------------------------------
void WorkThread::_wakeup(void)
{
	if (atomic_compare_exchange(m_is_queue_empty, true, false)) {
		int8_t dummy = 0;
		m_pipe.write((const char*)&dummy, sizeof(dummy));
	}
}

}

