/*
Copyright(C) thecodeway.com
*/

#include <cy_network.h>
#include "cyn_tcp_server_master_thread.h"

namespace cyclone
{
//-------------------------------------------------------------------------------------
TcpServerMasterThread::TcpServerMasterThread(TcpServer* server)
	: m_server(server)
{
	assert(m_server);
}

//-------------------------------------------------------------------------------------
TcpServerMasterThread::~TcpServerMasterThread()
{
	assert(m_acceptor_sockets.empty());
}

//-------------------------------------------------------------------------------------
void TcpServerMasterThread::send_thread_message(uint16_t id, uint16_t size, const char* message)
{
	assert(m_master_thread.is_running());

	m_master_thread.send_message(id, size, message);
}

//-------------------------------------------------------------------------------------
void TcpServerMasterThread::send_thread_message(const Packet* message)
{
	assert(m_master_thread.is_running());
	m_master_thread.send_message(message);
}

//-------------------------------------------------------------------------------------
void TcpServerMasterThread::send_thread_message(const Packet** message, int32_t counts)
{
	assert(m_master_thread.is_running());
	m_master_thread.send_message(message, counts);
}

//-------------------------------------------------------------------------------------
bool TcpServerMasterThread::bind_socket(const Address& bind_addr, bool enable_reuse_port)
{
	//must bind socket before run master thread
	assert(!m_master_thread.is_running());
	if (m_master_thread.is_running()) return false;

	//create a non blocking socket
	socket_t sfd = socket_api::create_socket();
	if (sfd == INVALID_SOCKET) {
		CY_LOG(L_ERROR, "create socket error");
		return false;
	}

	//set socket to non-block mode
	if (!socket_api::set_nonblock(sfd, true)) {
		//the process should be stop		
		CY_LOG(L_ERROR, "set socket to non block mode error");
		return false;
	}

	//set socket close on exe flag, the file  descriptor will be closed open across an execve.
	socket_api::set_close_onexec(sfd, true);

	//set accept socket option
#ifdef CY_SYS_WINDOWS
	(void)enable_reuse_port;
#else
	if (enable_reuse_port) {
		//http://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t
		socket_api::set_reuse_port(sfd, true);
		socket_api::set_reuse_addr(sfd, true);
	}
#endif

	//bind address
	if (!(socket_api::bind(sfd, bind_addr.get_sockaddr_in()))) {
		CY_LOG(L_ERROR, "bind to address %s:%d failed", bind_addr.get_ip(), bind_addr.get_port());
		socket_api::close_socket(sfd);
		return false;
	}

	CY_LOG(L_DEBUG, "bind to address %s:%d ok", bind_addr.get_ip(), bind_addr.get_port());
	m_acceptor_sockets.push_back(std::make_tuple(sfd, Looper::INVALID_EVENT_ID));

	return true;
}

//-------------------------------------------------------------------------------------
bool TcpServerMasterThread::start(void)
{
	//already running?
	assert(!m_master_thread.is_running());
	if (m_master_thread.is_running()) return false;

	m_master_thread.set_on_start(std::bind(&TcpServerMasterThread::_on_thread_start, this));
	m_master_thread.set_on_message(std::bind(&TcpServerMasterThread::_on_thread_message, this, std::placeholders::_1));
	m_master_thread.start("tcp_master");
	return true;
}

//-------------------------------------------------------------------------------------
Address TcpServerMasterThread::get_bind_address(size_t index)
{
	Address address;
	if (index >= m_acceptor_sockets.size()) return address;

	sockaddr_in addr;
	socket_api::getsockname(std::get<0>(m_acceptor_sockets[index]), addr);

	return Address(addr);
}

//-------------------------------------------------------------------------------------
bool TcpServerMasterThread::_on_thread_start(void)
{
	int32_t counts = 0;
	for (auto& listen_socket : m_acceptor_sockets)
	{
		socket_t sfd = std::get<0>(listen_socket);
		auto& event_id = std::get<1>(listen_socket);

		//register accept event
		event_id = m_master_thread.get_looper()->register_event(sfd,
			Looper::kRead,
			this,
			std::bind(&TcpServerMasterThread::_on_accept_event, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			0);

		//begin listen
		socket_api::listen(sfd);
		counts++;
	}

	CY_LOG(L_DEBUG, "tcp master thread run, listen %d port(s)", counts);

	if (m_server->m_listener.on_master_thread_start)
	{
		m_server->m_listener.on_master_thread_start(m_server, m_master_thread.get_looper());
	}
	return true;
}

//-------------------------------------------------------------------------------------
void TcpServerMasterThread::_on_thread_message(Packet* message)
{
	//accept thread command
	assert(message);

	uint16_t msg_id = message->get_packet_id();
	if (msg_id == ShutdownCmd::ID) {
		Looper* looper = m_master_thread.get_looper();

		//close all listen socket(s)
		for (auto listen_socket : m_acceptor_sockets) {
			auto& sfd = std::get<0>(listen_socket);
			auto& event_id = std::get<1>(listen_socket);

			if (event_id != Looper::INVALID_EVENT_ID) {
				looper->delete_event(event_id);
				event_id = Looper::INVALID_EVENT_ID;
			}
			if (sfd != INVALID_SOCKET) {
				socket_api::close_socket(sfd);
				sfd = INVALID_SOCKET;
			}
		}
		m_acceptor_sockets.clear();

		//stop looper
		looper->push_stop_request();
	}
	else if (msg_id == StopListenCmd::ID) {
		assert(message->get_packet_size() == sizeof(StopListenCmd));
		StopListenCmd stopListenCmd;
		memcpy(&stopListenCmd, message->get_packet_content(), sizeof(stopListenCmd));

		Looper* looper = m_master_thread.get_looper();
		auto& listen_socket = m_acceptor_sockets[stopListenCmd.index];
		auto& sfd = std::get<0>(listen_socket);
		auto& event_id = std::get<1>(listen_socket);

		//disable event
		if (event_id != Looper::INVALID_EVENT_ID) {
			looper->delete_event(event_id);
			event_id = Looper::INVALID_EVENT_ID;
		}
		//close socket
		if (sfd != INVALID_SOCKET) {
			socket_api::close_socket(sfd);
			sfd = INVALID_SOCKET;
		}
	}
	else if (msg_id >= kCustomCmdID_Begin) {
		//extra message
		if (m_server->m_listener.on_master_thread_command) {
			m_server->m_listener.on_master_thread_command(m_server, message);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpServerMasterThread::_on_accept_event(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	(void)id;
	(void)event;

	//is shutdown in processing?		
	if (m_server->m_shutdown_ing.load() > 0) return;

	//call accept and create peer socket		
	socket_t connfd = socket_api::accept(fd, 0);
	if (connfd == INVALID_SOCKET)
	{
		//log error		
		CY_LOG(L_ERROR, "accept socket error");
		return;
	}

	m_server->_on_accept_socket(connfd);
}

//-------------------------------------------------------------------------------------
void TcpServerMasterThread::join(void)
{
	m_master_thread.join();
}

}

