/*
Copyright(C) thecodeway.com
*/

#include <cy_network.h>
#include "cyn_work_thread.h"

namespace cyclone
{

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

	//delete the looper
	Looper::destroy_looper(m_looper);
	m_looper = 0;
}

//-------------------------------------------------------------------------------------
bool WorkThread::_on_command(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());

	int32_t cmd;
	//read cmd
	if (sizeof(cmd) != m_pipe.read((char*)&cmd, sizeof(cmd)))
	{//error
		return false;
	}

	if (cmd == kNewConnectionCmd)
	{
		//get new connection
		socket_t sfd;
		sockaddr_in peer_addr;

		if (sizeof(sfd) != m_pipe.read((char*)&sfd, sizeof(sfd)) ||
			sizeof(peer_addr) != m_pipe.read((char*)&peer_addr, sizeof(peer_addr)))
		{	//error
			return false;
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
			return false;
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

		//if all connection is shutdown, and server is in shutdown process, quit the loop
		
	}
	else if (cmd == kShutdownCmd)
	{
		//all connection is disconnect, just quit the loop
		if (m_connections.empty()) return true;

		//send shutdown command to all connection
		ConnectionList::iterator it, end = m_connections.end();
		for (it = m_connections.begin(); it != end; ++it)
		{
			Connection* conn = *it;
			if (conn->get_state() == Connection::kConnected)
			{
				conn->shutdown();
			}
		}
		//just wait...
	}

	return false;
}

}
