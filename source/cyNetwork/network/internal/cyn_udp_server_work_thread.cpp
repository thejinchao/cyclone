#include "cyn_udp_server_work_thread.h"
#include <network/cyn_udp_server.h>

namespace cyclone
{
//-------------------------------------------------------------------------------------
UdpServerWorkThread::UdpServerWorkThread(int32_t index, UdpServer* server)
	: m_index(index)
	, m_work_thread(nullptr)
	, m_server(server)
	, m_read_buf(1024*1024*8-1) // 8MB read cache
{
	m_work_thread = new WorkThread();
}

//-------------------------------------------------------------------------------------
UdpServerWorkThread::~UdpServerWorkThread()
{
	delete m_work_thread;
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::bind_socket(const Address& bind_addr)
{
	//must bind socket before run master thread
	assert(!(m_work_thread->is_running()));
	if (m_work_thread->is_running()) return false;

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

	//bind address
	if (!(socket_api::bind(sfd, bind_addr.get_sockaddr_in()))) {
		CY_LOG(L_ERROR, "bind to address %s:%d failed", bind_addr.get_ip(), bind_addr.get_port());
		socket_api::close_socket(sfd);
		return false;
	}

	CY_LOG(L_TRACE, "bind to address %s:%d ok", bind_addr.get_ip(), bind_addr.get_port());
	m_sockets.push_back(std::make_tuple(sfd, (int32_t)m_sockets.size()));

	return true;
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::start(void)
{
	//already running?
	assert(!(m_work_thread->is_running()));
	if (m_work_thread->is_running()) return false;

	char name[32] = { 0 };
	std::snprintf(name, 32, "udp_%d", m_index);

	m_work_thread->setOnStartFunction(std::bind(&UdpServerWorkThread::_on_thread_start, this));
	m_work_thread->setOnMessageFunction(std::bind(&UdpServerWorkThread::_on_workthread_message, this, std::placeholders::_1));
	m_work_thread->start(name);
	return true;
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::join(void)
{
	m_work_thread->join();
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::send_thread_message(uint16_t id, uint16_t size, const char* message)
{
	assert(m_work_thread);
	m_work_thread->send_message(id, size, message);
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::is_in_workthread(void) const
{
	assert(m_work_thread);
	return sys_api::thread_get_current_id() == m_work_thread->get_looper()->get_thread_id();
}

//-------------------------------------------------------------------------------------
bool UdpServerWorkThread::_on_thread_start(void)
{
	CY_LOG(L_TRACE, "udp work thread run, total %zd socket(s)", m_sockets.size());

	for (auto& s : m_sockets)
	{
		socket_t sfd = std::get<0>(s);
		int32_t socket_index = std::get<1>(s);

		//register accept event
		m_work_thread->get_looper()->register_event(sfd,
			Looper::kRead,
			reinterpret_cast<void*>(static_cast<std::intptr_t>(socket_index)),
			std::bind(&UdpServerWorkThread::_on_read_event, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
			0);
	}

	if (m_server->m_listener.on_work_thread_start) {
		m_server->m_listener.on_work_thread_start(m_server, m_index, m_work_thread->get_looper());
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
	if (msg_id == ShutdownCmd::ID)
	{
		CY_LOG(L_DEBUG, "receive shutdown cmd");
		//push loop request command
		m_work_thread->get_looper()->push_stop_request();
		return;
	}
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::_on_read_event(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param)
{
	m_read_buf.reset();

	int32_t socket_index = static_cast<int32_t>(reinterpret_cast<intptr_t>(param));

	sockaddr_in addr;
	ssize_t len = m_read_buf.recvfrom_socket(fd, addr);

	Address peerAddress(addr);
	if (len>0 && m_server->m_listener.on_message) {
		m_server->m_listener.on_message(m_server, m_index, socket_index, m_read_buf, peerAddress);
	}

	assert(m_read_buf.empty());
}

//-------------------------------------------------------------------------------------
void UdpServerWorkThread::sendto(int32_t socket_index, const char* buf, size_t len, const Address& peer_address)
{
	assert(is_in_workthread());
	assert(socket_index >= 0 && socket_index < (int32_t)m_sockets.size());

	if (socket_index < 0 || socket_index >= (int32_t)m_sockets.size()) return;

	socket_t sfd = std::get<0>(m_sockets[socket_index]);
	socket_api::sendto(sfd, buf, len, peer_address.get_sockaddr_in());
}

}