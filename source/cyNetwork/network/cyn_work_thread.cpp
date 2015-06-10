/*
Copyright(C) thecodeway.com
*/

#include <cy_network.h>
#include "cyn_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
WorkThread::WorkThread(int32_t index, TcpServer* server)
	: m_index(index)
	, m_looper(0)
	, m_server(server)
{
	m_thread_ready = thread_api::signal_create();

	//run the work thread
	m_thread = thread_api::thread_create(_work_thread_entry, this);

	//wait work thread ready signal
	thread_api::signal_wait(m_thread_ready);
}

//-------------------------------------------------------------------------------------
WorkThread::~WorkThread()
{
	thread_api::signal_destroy(m_thread_ready);
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

		if (sizeof(sfd) != m_pipe.read((char*)&sfd, sizeof(sfd)))
		{	//error
			return false;
		}

		//create tcp connection 
		Connection* conn = new Connection(sfd, m_looper, this);
		m_connections.insert(conn);

		//established the connection
		conn->established();
	}
	else if (cmd == kCloseConnectionCmd)
	{
		intptr_t conn_ptr;
		int32_t shutdown_ing;
		if (sizeof(conn_ptr) != m_pipe.read((char*)&conn_ptr, sizeof(conn_ptr)) ||
			sizeof(shutdown_ing) != m_pipe.read((char*)&shutdown_ing, sizeof(shutdown_ing)))
		{//error
			return false;
		}

		Connection* conn = (Connection*)conn_ptr;
		if (m_connections.find(conn) == m_connections.end()) return false;

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
		if (m_connections.empty() && shutdown_ing>0) return true;
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

//-------------------------------------------------------------------------------------
void WorkThread::on_connection_event(Connection::Event event, Connection* conn)
{
	assert(m_server);

	TcpServer::Listener* server_listener = m_server->get_listener();
	switch (event) {
	case Connection::kOnConnection:
		if (server_listener) {
			server_listener->on_connection_callback(m_server, conn);
		}
		break;

	case Connection::kOnMessage:
		if (server_listener) {
			server_listener->on_message_callback(m_server, conn);
		}
		break;

	case Connection::kOnClose:
		if (server_listener) {
			server_listener->on_close_callback(m_server, conn);
		}

		//shutdown this connection
		m_server->shutdown_connection(conn);
		break;
	}
}

}
