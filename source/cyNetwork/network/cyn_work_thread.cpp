/*
Copyright(C) thecodeway.com
*/

#include <cy_network.h>
#include "cyn_work_thread.h"

namespace cyclone
{
namespace network
{

//-------------------------------------------------------------------------------------
using namespace cyclone::event;  //using cyclone event namespace

//-------------------------------------------------------------------------------------
WorkThread::WorkThread(TcpServer* server, int32_t index)
	: m_server(server)
	, m_looper(0)
	, m_index(index)
{
	m_thread_ready = thread_api::signal_create();

	//run the work thread
	m_thread = thread_api::thread_create(_work_thread_entry, this);

	//wait work thread ready signal
	thread_api::signal_wait(m_thread_ready);
	thread_api::signal_destroy(m_thread_ready);
}

//-------------------------------------------------------------------------------------
WorkThread::~WorkThread()
{

}

//-------------------------------------------------------------------------------------
void WorkThread::_work_thread(void)
{
	//create work event looper
	m_looper = Looper::create_looper();

	//register pipe read event
	m_looper->register_event(m_pipe.get_read_port(), Looper::kRead, this,
		_on_command_entry, 0);

	// set work thread ready signal
	thread_api::signal_notify(m_thread_ready);

	//enter loop ...
	m_looper->loop();
}

//-------------------------------------------------------------------------------------
void WorkThread::_on_command(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());

	int32_t cmd;
	//read cmd
	if (sizeof(cmd) != m_pipe.read((char*)&cmd, sizeof(cmd)))
	{//error
		return;
	}

	if (cmd == kNewConnectionCmd)
	{
		//get new connection
		socket_t sfd;
		sockaddr_in peer_addr;

		if (sizeof(sfd) != m_pipe.read((char*)&sfd, sizeof(sfd)) ||
			sizeof(peer_addr) != m_pipe.read((char*)&peer_addr, sizeof(peer_addr)))
		{	//error
			return;
		}

		//create tcp connection 
		Connection* conn = new Connection(sfd, peer_addr, m_server, get_index(), m_looper);
		m_connections.insert(conn);

		//established the connection
		conn->established();
	}
	else if (cmd == kCloseConnectionCmd)
	{
		intptr_t conn_ptr;
		if (sizeof(conn_ptr) != m_pipe.read((char*)&conn_ptr, sizeof(conn_ptr)))
		{//error
			return;
		}

		Connection* conn = (Connection*)conn_ptr;
		Connection::State curr_state = conn->get_state();

		if (curr_state == Connection::kConnected)
		{
			//shutdown,and wait 
			conn->shutdown();
		}

		else if (curr_state == Connection::kDisconnected)
		{
			//delete the event channel
			m_looper->delete_event(conn->get_event_id());

			//delete the connection object
			m_connections.erase(conn);
			delete conn;
		}
		else
		{
			//kDisconnecting...
			//shutdown is in process, do nothing...
		}
		
	}
}

}
}
