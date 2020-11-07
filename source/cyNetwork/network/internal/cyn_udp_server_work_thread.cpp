#include "cyn_udp_server_work_thread.h"
#include <network/cyn_udp_server.h>

namespace cyclone
{
//-------------------------------------------------------------------------------------
UdpServerWorkThread::UdpServerWorkThread(UdpServer* server, int32_t index)
	: m_server(server)
	, m_index(index)
	, m_thread(nullptr)
	, m_clear_locked_address_timer(Looper::INVALID_EVENT_ID)
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

	m_thread->set_on_start(std::bind(&UdpServerWorkThread::_on_thread_start, this));
	m_thread->set_on_message(std::bind(&UdpServerWorkThread::_on_workthread_message, this, std::placeholders::_1));
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
void UdpServerWorkThread::send_thread_message(const Packet* message)
{
	assert(m_thread);
	m_thread->send_message(message);
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::send_thread_message(const Packet** message, int32_t counts)
{
	assert(m_thread);
	m_thread->send_message(message, counts);
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
	CY_LOG(L_DEBUG, "[Work]UDP work thread %d run", m_index);

	//start clear locked address timer
	m_clear_locked_address_timer = m_thread->get_looper()->register_timer_event(1000, nullptr, 
		std::bind(&UdpServerWorkThread::_on_clear_locked_address_timer, this,
		std::placeholders::_1, std::placeholders::_2));

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

		Address peerAddress(closeConnCmd.peer_address);
		ConnectionMap::iterator it = m_connections.find(peerAddress);
		if (it == m_connections.end()) return;

		//shutdown connection and remove from map directly
		it->second->shutdown();
		m_connections.erase(it);
		CY_LOG(L_DEBUG, "[Work]Receive CloseConnectionCmd. close connection, current %zd connections", m_connections.size());

		//insert locked address map
		m_locked_address.insert(std::make_pair(peerAddress, sys_api::utc_time_now()));
		CY_LOG(L_DEBUG, "[Work]Lock address %s:%d, current %zd address locked", peerAddress.get_ip(), (int32_t)peerAddress.get_port(), m_locked_address.size());

		//if all connection is shutdown, and server is in shutdown process, quit the loop
		if (m_connections.empty() && closeConnCmd.shutdown_ing > 0) {
			//push loop quit command
			m_thread->get_looper()->push_stop_request();
			return;
		}
	}
	else if (msg_id == kShutdownCmdID)
	{
		CY_LOG(L_DEBUG, "[Work]Receive shutdown cmd");
		Looper* looper = m_thread->get_looper();

		//shutdown timer
		looper->disable_all(m_clear_locked_address_timer);
		looper->delete_event(m_clear_locked_address_timer);
		m_clear_locked_address_timer = Looper::INVALID_EVENT_ID;

		//all connection is disconnect, just quit the loop
		if (m_connections.empty()) {
			//push loop request command
			looper->push_stop_request();
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
void UdpServerWorkThread::_on_clear_locked_address_timer(Looper::event_id_t, void*)
{
	if (m_locked_address.empty()) return;
	int64_t now = sys_api::utc_time_now();

	LockedAddressMap::iterator it;
	for (it = m_locked_address.begin(); it != m_locked_address.end();) {
		if (now - it->second > UdpServer::ADDRESS_LOCKED_TIME * 1000) {
			Address add = it->first;
			m_locked_address.erase(it++);
			CY_LOG(L_DEBUG, "[Work]Address %s:%d is unlocked, current %zd address locked.", add.get_ip(), add.get_port(), m_locked_address.size());
		}
		else {
			it++;
		}
	}
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::_on_receive_udp_message(const sockaddr_in& local_addr, const sockaddr_in& peer_addr, const char* buf, int32_t len)
{
	Address peerAddress(peer_addr);

	//is address locked?
	LockedAddressMap::iterator it_locked = m_locked_address.find(peerAddress);
	if (it_locked != m_locked_address.end()) {

		//locked address?
		int64_t now = sys_api::utc_time_now();
		if (now - it_locked->second < UdpServer::ADDRESS_LOCKED_TIME * 1000) {
			CY_LOG(L_DEBUG, "[Work]Address is locked now.");
			return;
		}

		//expire already
		m_locked_address.erase(it_locked);
		CY_LOG(L_DEBUG, "[Work]Unlock Address %s:%d, total %zd locked.", peerAddress.get_ip(), peerAddress.get_port(), m_locked_address.size());
	}

	//is connection already exist?
	ConnectionMap::iterator it = m_connections.find(peerAddress);
	if (it == m_connections.end()) {
		//new connection
		UdpConnectionPtr conn = std::make_shared<UdpConnection>(m_thread->get_looper(), m_server->_get_next_connection_id());

		//init udp connection
		Address localAddress(local_addr);
		if (!conn->init(peerAddress, &localAddress)) {
			return;
		}
		it = m_connections.insert(std::make_pair(peerAddress, conn)).first;

		CY_LOG(L_DEBUG, "Accept this connection, Address:%s:%d, id=%d, current connection size=%zd", peerAddress.get_ip(), (int32_t)peerAddress.get_port(), conn->get_id(), m_connections.size());

		//set callback functions
		conn->m_on_message = [this](UdpConnectionPtr connection) {
			this->m_server->_on_socket_message(this->m_index, connection);
		};
		conn->m_on_close = [this](UdpConnectionPtr connection) {
			this->m_server->_on_socket_close(this->m_index, connection);
		};

		//notify server a new connection connected
		m_server->_on_socket_connected(this->m_index, conn);
	}

	//push udp message to UDP Connection
	it->second->_on_udp_input(buf, len);
}

}
