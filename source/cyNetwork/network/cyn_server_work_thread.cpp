/*
Copyright(C) thecodeway.com
*/

#include <cy_network.h>
#include "cyn_server_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
ServerWorkThread::ServerWorkThread(int32_t index, TcpServer* server, const char* name, DebugInterface* debuger)
	: m_index(index)
	, m_server(server)
	, m_debuger(debuger)
{

	//run the work thread
	char temp[MAX_PATH] = { 0 };
	std::snprintf(temp, MAX_PATH, "%s_%d", (name ? name : "worker"), m_index);
	m_name = temp;

	//run work thread
	m_work_thread = new WorkThread();
	m_work_thread->setOnStartFunction(std::bind(&ServerWorkThread::_on_workthread_start, this));
	m_work_thread->setOnMessageFunction(std::bind(&ServerWorkThread::_on_workthread_message, this, std::placeholders::_1));

	m_work_thread->start(m_name.c_str());
}

//-------------------------------------------------------------------------------------
ServerWorkThread::~ServerWorkThread()
{
	delete m_work_thread;
}

//-------------------------------------------------------------------------------------
void ServerWorkThread::send_message(uint16_t id, uint16_t size, const char* message)
{
	assert(m_work_thread);

	m_work_thread->send_message(id, size, message);
}

//-------------------------------------------------------------------------------------
void ServerWorkThread::send_message(const Packet* message)
{
	assert(m_work_thread);
	m_work_thread->send_message(message);
}

//-------------------------------------------------------------------------------------
void ServerWorkThread::send_message(const Packet** message, int32_t counts)
{
	assert(m_work_thread);
	m_work_thread->send_message(message, counts);
}

//-------------------------------------------------------------------------------------
bool ServerWorkThread::is_in_workthread(void) const
{
	return sys_api::thread_get_current_id() == m_work_thread->get_looper()->get_thread_id();
}

//-------------------------------------------------------------------------------------
ConnectionPtr ServerWorkThread::get_connection(int32_t connection_id)
{
	assert(is_in_workthread());

	ConnectionMap::iterator it = m_connections.find(connection_id);
	if (it == m_connections.end()) return 0;
	return it->second;
}

//-------------------------------------------------------------------------------------
bool ServerWorkThread::_on_workthread_start(void)
{
	CY_LOG(L_INFO, "Work thread \"%s\" start...", m_name.c_str());
	TcpServer::WorkThreadStartCallback& onWorkThreadStart = m_server->m_listener.onWorkThreadStart;

	if(onWorkThreadStart)
		onWorkThreadStart(m_server, get_index(), m_work_thread->get_looper());
	return true;
}

//-------------------------------------------------------------------------------------
void ServerWorkThread::_on_workthread_message(Packet* message)
{
	assert(is_in_workthread());
	assert(message);
	assert(m_server);

	const TcpServer::Listener& server_listener = m_server->m_listener;

	uint16_t msg_id = message->get_packet_id();
	if (msg_id == NewConnectionCmd::ID)
	{
		assert(message->get_packet_size() == sizeof(NewConnectionCmd));
		NewConnectionCmd newConnectionCmd;
		memcpy(&newConnectionCmd, message->get_packet_content(), sizeof(NewConnectionCmd));

		//create tcp connection 
		ConnectionPtr conn = std::make_shared<Connection>(m_server->get_next_connection_id(), newConnectionCmd.sfd, m_work_thread->get_looper(), this);
		
		//bind onMessage function
		if (server_listener.onMessage) {
			conn->setOnMessageFunction([this](ConnectionPtr connection) {
				m_server->m_listener.onMessage(m_server, get_index(), connection);
			});
		}

		//bind onClose function
		conn->setOnCloseFunction([this](ConnectionPtr connection) {
			if(m_server->m_listener.onClose)
				m_server->m_listener.onClose(m_server, get_index(), connection);
			//shutdown this connection next tick
			m_server->shutdown_connection(connection);
		});
		

		//notify server listener 
		if (server_listener.onConnected) {
			server_listener.onConnected(m_server, get_index(), conn);
		}
		
		m_connections.insert(std::make_pair(conn->get_id(), conn));
	}
	else if (msg_id == CloseConnectionCmd::ID)
	{
		assert(message->get_packet_size() == sizeof(CloseConnectionCmd));
		CloseConnectionCmd closeConnectionCmd;
		memcpy(&closeConnectionCmd, message->get_packet_content(), sizeof(CloseConnectionCmd));

		ConnectionMap::iterator it = m_connections.find(closeConnectionCmd.conn_id);
		if (it == m_connections.end()) return;

		ConnectionPtr conn = it->second;
		Connection::State curr_state = conn->get_state();

		if (curr_state == Connection::kConnected)
		{
			//shutdown,and wait 
			conn->shutdown();
		}
		else if (curr_state == Connection::kDisconnected)
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
			ConnectionPtr conn = it->second;
			if (conn->get_state() == Connection::kConnected)
			{
				conn->shutdown();
			}
		}
		//just wait...
	}
	else if (msg_id == DebugCmd::ID)
	{
		assert(message->get_packet_size() == sizeof(DebugCmd));
		DebugCmd debugCmd;
		memcpy(&debugCmd, message->get_packet_content(), sizeof(DebugCmd));

		_debug(debugCmd);
	}
	else
	{
		//extra message
		if (server_listener.onWorkThreadCommand) {
			server_listener.onWorkThreadCommand(m_server, get_index(), message);
		}
	}
}

//-------------------------------------------------------------------------------------
void ServerWorkThread::join(void)
{
	m_work_thread->join();
}

//-------------------------------------------------------------------------------------
void ServerWorkThread::_debug(DebugCmd&)
{
	assert(is_in_workthread());
	assert(m_server);

	if (!m_debuger || !(m_debuger->isEnable())) return;

	char key_temp[MAX_PATH] = { 0 };

	//Debug ConnectionMap
	std::snprintf(key_temp, MAX_PATH, "ServerWorkThread:%s:connection_map_counts", m_name.c_str());
	m_debuger->updateDebugValue(key_temp, (int32_t)m_connections.size());

	//Debug Looper
	Looper* looper = m_work_thread->get_looper();
	looper->debug(m_debuger, m_name.c_str());

	//Debug all connections
	int index = 0;
	ConnectionMap::iterator it, end = m_connections.end();
	for (it = m_connections.begin(); it != end; ++it, ++index) {
		it->second->debug(m_debuger);
	}

}

}
