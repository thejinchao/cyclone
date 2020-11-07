/*
Copyright(C) thecodeway.com
*/

#include <cy_network.h>
#include "cyn_tcp_server_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpServerWorkThread::TcpServerWorkThread(TcpServer* server, int32_t index)
	: m_server(server)
	, m_index(index)
{
	//run work thread
	m_work_thread = new WorkThread();
	m_work_thread->set_on_start(std::bind(&TcpServerWorkThread::_on_workthread_start, this));
	m_work_thread->set_on_message(std::bind(&TcpServerWorkThread::_on_workthread_message, this, std::placeholders::_1));

	char temp[MAX_PATH] = { 0 };
	std::snprintf(temp, MAX_PATH, "tcp_work_%d", m_index);
	m_work_thread->start(temp);
}

//-------------------------------------------------------------------------------------
TcpServerWorkThread::~TcpServerWorkThread()
{
	delete m_work_thread;
}

//-------------------------------------------------------------------------------------
void TcpServerWorkThread::send_thread_message(uint16_t id, uint16_t size, const char* message)
{
	assert(m_work_thread);

	m_work_thread->send_message(id, size, message);
}

//-------------------------------------------------------------------------------------
void TcpServerWorkThread::send_thread_message(const Packet* message)
{
	assert(m_work_thread);
	m_work_thread->send_message(message);
}

//-------------------------------------------------------------------------------------
void TcpServerWorkThread::send_thread_message(const Packet** message, int32_t counts)
{
	assert(m_work_thread);
	m_work_thread->send_message(message, counts);
}

//-------------------------------------------------------------------------------------
bool TcpServerWorkThread::is_in_workthread(void) const
{
	return sys_api::thread_get_current_id() == m_work_thread->get_looper()->get_thread_id();
}

//-------------------------------------------------------------------------------------
TcpConnectionPtr TcpServerWorkThread::get_connection(int32_t connection_id)
{
	assert(is_in_workthread());

	ConnectionMap::iterator it = m_connections.find(connection_id);
	if (it == m_connections.end()) return 0;
	return it->second;
}

//-------------------------------------------------------------------------------------
bool TcpServerWorkThread::_on_workthread_start(void)
{
	CY_LOG(L_INFO, "Tcp work thread %d start...", m_index);

	if (m_server->m_listener.on_work_thread_start) {
		m_server->m_listener.on_work_thread_start(m_server, get_index(), m_work_thread->get_looper());
	}
	return true;
}

//-------------------------------------------------------------------------------------
void TcpServerWorkThread::_on_workthread_message(Packet* message)
{
	assert(is_in_workthread());
	assert(message);
	assert(m_server);

	uint16_t msg_id = message->get_packet_id();
	if (msg_id == NewConnectionCmd::ID)
	{
		assert(message->get_packet_size() == sizeof(NewConnectionCmd));
		NewConnectionCmd newConnectionCmd;
		memcpy(&newConnectionCmd, message->get_packet_content(), sizeof(NewConnectionCmd));

		//create tcp connection 
		TcpConnectionPtr conn = std::make_shared<TcpConnection>(m_server->get_next_connection_id(), newConnectionCmd.sfd, m_work_thread->get_looper(), this);
		CY_LOG(L_DEBUG, "receive new connection, id=%d, peer_addr=%s:%d", conn->get_id(), conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());

		//bind onMessage function
		conn->set_on_message([this](TcpConnectionPtr connection) {
			m_server->_on_socket_message(this->get_index(), connection);
		});

		//bind onClose function
		conn->set_on_close([this](TcpConnectionPtr connection) {
			m_server->_on_socket_close(this->get_index(), connection);
		});

		//notify server listener 
		m_server->_on_socket_connected(get_index(), conn);
		
		m_connections.insert(std::make_pair(conn->get_id(), conn));
	}
	else if (msg_id == CloseConnectionCmd::ID)
	{
		assert(message->get_packet_size() == sizeof(CloseConnectionCmd));
		CloseConnectionCmd closeConnectionCmd;
		memcpy(&closeConnectionCmd, message->get_packet_content(), sizeof(CloseConnectionCmd));

		ConnectionMap::iterator it = m_connections.find(closeConnectionCmd.conn_id);
		if (it == m_connections.end()) return;

		TcpConnectionPtr conn = it->second;
		TcpConnection::State curr_state = conn->get_state();

		CY_LOG(L_DEBUG, "receive close connection cmd, id=%d, state=%d", conn->get_id(), conn->get_state());
		if (curr_state == TcpConnection::kConnected)
		{
			//shutdown,and wait 
			conn->shutdown();
		}
		else if (curr_state == TcpConnection::kDisconnected)
		{
			//delete the connection object
			m_connections.erase(conn->get_id());
		}
		else
		{
			//kDisconnecting...
			//shutdown is in process, do nothing...
		}

		//if all connection is shutdown, and server is in shutdown process, quit the loop
		if (m_connections.empty() && closeConnectionCmd.shutdown_ing > 0) {
			//push loop quit command
			m_work_thread->get_looper()->push_stop_request();
			return;
		}
	}
	else if (msg_id == ShutdownCmd::ID)
	{
		CY_LOG(L_DEBUG, "receive shutdown cmd");
		//all connection is disconnect, just quit the loop
		if (m_connections.empty()) {
			//push loop request command
			m_work_thread->get_looper()->push_stop_request();
			return;
		}

		//send shutdown command to all connection
		ConnectionMap::iterator it, end = m_connections.end();
		for (it = m_connections.begin(); it != end; ++it)
		{
			TcpConnectionPtr conn = it->second;
			if (conn->get_state() == TcpConnection::kConnected)
			{
				conn->shutdown();
			}
		}
		//just wait...
	}
	else
	{
		//extra message
		if (m_server->m_listener.on_work_thread_command) {
			m_server->m_listener.on_work_thread_command(m_server, get_index(), message);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpServerWorkThread::join(void)
{
	m_work_thread->join();
}

}
