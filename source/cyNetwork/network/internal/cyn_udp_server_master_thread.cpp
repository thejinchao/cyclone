#include "cyn_udp_server_master_thread.h"

#include <network/cyn_udp_server.h>

namespace cyclone
{

//-------------------------------------------------------------------------------------
UdpServerMasterThread::UdpServerMasterThread(UdpServer* server, OnUdpMessageReceived udp_message_callback)
	: m_thread(nullptr)
	, m_server(server)
	, m_read_buf(nullptr)
	, m_udp_message_callback(udp_message_callback)
{
	assert(m_udp_message_callback);

	m_thread = new WorkThread();
	m_read_buf = new char[UdpServer::MAX_UDP_PACKET_SIZE+1];
}

//-------------------------------------------------------------------------------------
UdpServerMasterThread::~UdpServerMasterThread()
{
	delete[] m_read_buf; 
	m_read_buf = nullptr;

	delete m_thread;
}

//-------------------------------------------------------------------------------------
bool UdpServerMasterThread::bind_socket(const Address& bind_addr)
{
	//must bind socket before run master thread
	assert(!(m_thread->is_running()));
	if (m_thread->is_running()) return false;

	//create a non blocking udp socket
	socket_t sfd = socket_api::create_socket(true);
	if (sfd == INVALID_SOCKET) {
		CY_LOG(L_ERROR, "create udp socket error");
		return false;
	}

	//set socket to non-block mode
	if (!socket_api::set_nonblock(sfd, true)) {
		//the process should be stop		
		CY_LOG(L_ERROR, "set socket to non block mode error");
		return false;
	}

	//enable reuse addr
	if (!socket_api::set_reuse_addr(sfd, true)) {
		CY_LOG(L_ERROR, "set reuse address failed");
		socket_api::close_socket(sfd);
		return false;
	}

	//bind address
	if (!socket_api::bind(sfd, bind_addr.get_sockaddr_in())) {
		CY_LOG(L_ERROR, "bind to address %s:%d failed", bind_addr.get_ip(), bind_addr.get_port());
		socket_api::close_socket(sfd);
		return false;
	}

	CY_LOG(L_DEBUG, "bind to address %s:%d ok", bind_addr.get_ip(), bind_addr.get_port());

	ReceiveSocket socket;
	socket.index = (int32_t)m_receive_sockets.size();
	socket.sfd = sfd;
	socket.bind_addr = bind_addr;
	socket.event_id = Looper::INVALID_EVENT_ID;
	m_receive_sockets.push_back(socket);
	return true;
}

//-------------------------------------------------------------------------------------
bool UdpServerMasterThread::start(void)
{
	//already running?
	assert(!(m_thread->is_running()));
	if (m_thread->is_running()) return false;

	m_thread->set_on_start(std::bind(&UdpServerMasterThread::_on_thread_start, this));
	m_thread->set_on_message(std::bind(&UdpServerMasterThread::_on_workthread_message, this, std::placeholders::_1));

	//start thread
	m_thread->start("udp_master");
	return true;
}

//-------------------------------------------------------------------------------------
void UdpServerMasterThread::join(void)
{
	assert(m_thread);
	m_thread->join();
}

//-------------------------------------------------------------------------------------
void UdpServerMasterThread::send_thread_message(uint16_t id, uint16_t size, const char* message)
{
	assert(m_thread);
	m_thread->send_message(id, size, message);
}

//-------------------------------------------------------------------------------------
void UdpServerMasterThread::send_thread_message(const Packet* message)
{
	assert(m_thread->is_running());
	m_thread->send_message(message);
}

//-------------------------------------------------------------------------------------
void UdpServerMasterThread::send_thread_message(const Packet** message, int32_t counts)
{
	assert(m_thread->is_running());
	m_thread->send_message(message, counts);
}

//-------------------------------------------------------------------------------------
bool UdpServerMasterThread::_on_thread_start(void)
{
	CY_LOG(L_DEBUG, "udp master thread run, total %zd receive socket(s)", m_receive_sockets.size());

	Looper* looper = m_thread->get_looper();

	for (ReceiveSocket& socket : m_receive_sockets)
	{
		//register receive socket read event
		socket.event_id = looper->register_event(socket.sfd,
			Looper::kRead,
			reinterpret_cast<void*>(static_cast<std::intptr_t>(socket.index)),
			std::bind(&UdpServerMasterThread::_on_read_event, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
			0);
	}

	if (m_server->m_listener.on_master_thread_start) {
		m_server->m_listener.on_master_thread_start(m_server, looper);
	}
	return true;
}

//-------------------------------------------------------------------------------------
void UdpServerMasterThread::_on_read_event(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param)
{
	int32_t index = static_cast<int32_t>(reinterpret_cast<intptr_t>(param));
	
	assert(index >= 0 && index < (int32_t)m_receive_sockets.size());
	if (index < 0 || index >= (int32_t)m_receive_sockets.size())return;

	ReceiveSocket& socket = m_receive_sockets[index];

	//call receive from
	sockaddr_in peer_addr;
	int32_t udp_len = (int32_t)socket_api::recvfrom(socket.sfd, m_read_buf, UdpServer::MAX_UDP_PACKET_SIZE, peer_addr);
	if (udp_len <= 0) {
		CY_LOG(L_ERROR, "socket_api::recvfrom error, err=%d", socket_api::get_lasterror());
		return;
	}

	//push to work thread
	if (m_udp_message_callback) {
		m_udp_message_callback(m_read_buf, udp_len, peer_addr, socket.bind_addr.get_sockaddr_in());
	}
}

//-------------------------------------------------------------------------------------
void UdpServerMasterThread::_on_workthread_message(Packet* message)
{
	//master thread command
	assert(message);

	uint16_t msg_id = message->get_packet_id();
	if (msg_id == kShutdownCmdID) {
		Looper* looper = m_thread->get_looper();

		//close all listen socket(s)
		for (auto& socket : m_receive_sockets) {
			socket_t sfd = socket.sfd;
			Looper::event_id_t& event_id = socket.event_id;

			if (event_id != Looper::INVALID_EVENT_ID) {
				looper->disable_all(event_id);
				looper->delete_event(event_id);
				event_id = Looper::INVALID_EVENT_ID;
			}
			if (sfd != INVALID_SOCKET) {
				socket_api::close_socket(sfd);
				sfd = INVALID_SOCKET;
			}
		}
		m_receive_sockets.clear();

		//stop looper
		looper->push_stop_request();
	}
}

}