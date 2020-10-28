#include "cyn_udp_server_work_thread.h"
#include <network/cyn_udp_server.h>

namespace cyclone
{
//-------------------------------------------------------------------------------------
UdpServerWorkThread::UdpServerWorkThread(UdpServer* server, int32_t index)
	: m_server(server)
	, m_index(index)
	, m_thread(nullptr)
{
	m_thread = new WorkThread();
}

//-------------------------------------------------------------------------------------
UdpServerWorkThread::~UdpServerWorkThread()
{
	delete m_thread;
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::start(void)
{
	//already running?
	assert(!(m_thread->is_running()));
	if (m_thread->is_running()) return false;

	char name[32] = { 0 };
	std::snprintf(name, 32, "udp_%d", m_index);

	m_thread->setOnStartFunction(std::bind(&UdpServerWorkThread::_on_thread_start, this));
	m_thread->setOnMessageFunction(std::bind(&UdpServerWorkThread::_on_workthread_message, this, std::placeholders::_1));
	m_thread->start(name);
	return true;
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::join(void)
{
	m_thread->join();
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::send_thread_message(uint16_t id, uint16_t size_part1, const char* msg_part1, uint16_t size_part2, const char* msg_part2)
{
	assert(m_thread);
	m_thread->send_message(id, size_part1, msg_part1, size_part2, msg_part2);
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::is_in_workthread(void) const
{
	assert(m_thread);
	return sys_api::thread_get_current_id() == m_thread->get_looper()->get_thread_id();
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::_on_thread_start(void)
{
	CY_LOG(L_TRACE, "udp work thread %d run", m_index);

	if (m_server->m_listener.on_work_thread_start) {
		m_server->m_listener.on_work_thread_start(m_server, m_index, m_thread->get_looper());
	}
	return true;
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::_on_workthread_message(Packet* message)
{
	assert(is_in_workthread());
	assert(message);
	assert(m_server);

	uint16_t msg_id = message->get_packet_id();
	if (msg_id == kReceiveUdpMessage)
	{
		//udp message head
		ReceiveUdpMessage udpMessage;
		memcpy(&udpMessage, message->get_packet_content(), sizeof(udpMessage));

		//udp message buf
		const char* buf = message->get_packet_content() + sizeof(ReceiveUdpMessage);
		int32_t len = (int32_t)message->get_packet_size() - (int32_t)sizeof(ReceiveUdpMessage);

		_on_receive_udp_message(udpMessage.local_address, udpMessage.peer_address, buf, len);
	}
	else if (msg_id == kCloseConnectionCmd)
	{
		CloseConnectionCmd closeConnCmd;
		memcpy(&closeConnCmd, message->get_packet_content(), sizeof(closeConnCmd));

		ConnectionMap::iterator it = m_connections.find(Address(closeConnCmd.peer_address));
		if (it == m_connections.end()) return;

		//shutdown connection and remove from map directly
		it->second->shutdown();
		m_connections.erase(it);

		//if all connection is shutdown, and server is in shutdown process, quit the loop
		if (m_connections.empty() && closeConnCmd.shutdown_ing > 0) {
			//push loop quit command
			m_thread->get_looper()->push_stop_request();
			return;
		}
	}
	else if (msg_id == kShutdownCmdID)
	{
		CY_LOG(L_DEBUG, "receive shutdown cmd");

		//all connection is disconnect, just quit the loop
		if (m_connections.empty()) {
			//push loop request command
			m_thread->get_looper()->push_stop_request();
			return;
		}

		//send shutdown command to all connection
		ConnectionMap::iterator it, end = m_connections.end();
		for (it = m_connections.begin(); it != end; ++it)
		{
			UdpConnectionPtr conn = it->second;
			conn->shutdown();
		}
		//just wait...
	}
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::_on_receive_udp_message(const sockaddr_in& local_addr, const sockaddr_in& peer_addr, const char* buf, int32_t len)
{
	//is connection already exist?
	ConnectionMap::iterator it = m_connections.find(peer_addr);
	if (it == m_connections.end()) {
		//new connection
		UdpConnectionPtr conn = std::make_shared<UdpConnection>(m_thread->get_looper(), m_server->is_kcp_enable(), m_server->_get_next_connection_id());

		//init udp connection
		Address localAddress(local_addr);
		Address peerAddress(peer_addr);
		if (!conn->init(peerAddress, &localAddress)) {
			return;
		}
		it = m_connections.insert(std::make_pair(peerAddress, conn)).first;

		//notify server a new connection connected
		m_server->_on_socket_connected(this->m_index, conn);

		//set callback functions
		conn->m_on_message = [this](UdpConnectionPtr connection) {
			this->m_server->_on_socket_message(this->m_index, connection);
		};
		conn->m_on_close = [this](UdpConnectionPtr connection) {
			this->m_server->_on_socket_close(this->m_index, connection);
		};
	}

	//push udp message to UDP Connection
	it->second->_on_udp_input(buf, len);
}

}
