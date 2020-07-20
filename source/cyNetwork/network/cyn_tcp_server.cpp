/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include "cyn_tcp_server.h"
#include "cyn_server_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpServer::TcpServer(const char* name, DebugInterface* debuger, void* param)
	: m_param(param)
	, m_work_thread_counts(0)
	, m_next_work(0)
	, m_running(0)
	, m_shutdown_ing(0)
	, m_next_connection_id(0)
	, m_name(name ? name : "server")
	, m_debuger(debuger)
{
	m_listener.onWorkThreadStart = nullptr;
	m_listener.onWorkThreadCommand = nullptr;
	m_listener.onConnected = nullptr;
	m_listener.onMessage = nullptr;
	m_listener.onClose = nullptr;
}

//-------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
	assert(m_running.load()==0);
	assert(m_acceptor_sockets.empty());
}

//-------------------------------------------------------------------------------------
bool TcpServer::bind(const Address& bind_addr, bool enable_reuse_port)
{
	//is running already?
	if (m_running > 0) return false;

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
	if (!(socket_api::bind(sfd, bind_addr.get_sockaddr_in()))){
		CY_LOG(L_ERROR, "bind to address %s:%d failed", bind_addr.get_ip(), bind_addr.get_port());
		socket_api::close_socket(sfd);
		return false;
	}

	CY_LOG(L_TRACE, "bind to address %s:%d ok", bind_addr.get_ip(), bind_addr.get_port());
	m_acceptor_sockets.push_back(std::make_tuple(sfd, Looper::INVALID_EVENT_ID));
	return true;
}

//-------------------------------------------------------------------------------------
bool TcpServer::start(int32_t work_thread_counts)
{
	CY_LOG(L_INFO, "TcpServer start with %d workthread(s)", work_thread_counts);

	if (work_thread_counts<1 || work_thread_counts > MAX_WORK_THREAD_COUNTS) {
		CY_LOG(L_ERROR, "param thread counts error");
		return false;
	}

	if (m_acceptor_sockets.empty()) {
		CY_LOG(L_ERROR, "at least listen one port!");
		return false;
	}

	//is running already?
	if (m_running.exchange(1) > 0) return false;

	//start work thread pool
	m_work_thread_counts = work_thread_counts;
	for (int32_t i = 0; i < m_work_thread_counts; i++) {
		//run the thread
		m_work_thread_pool.push_back(new ServerWorkThread(i, this, m_name.c_str(), m_debuger));
	}

	//start listen thread
	m_accept_thread.setOnStartFunction(std::bind(&TcpServer::_on_accept_start, this));
	m_accept_thread.setOnMessageFunction(std::bind(&TcpServer::_on_accept_message, this, std::placeholders::_1));
	m_accept_thread.start("accept");

	//write debug variable
	if (m_debuger && m_debuger->isEnable()) {
		char key_value[256] = { 0 };
		std::snprintf(key_value, 256, "TcpServer:%s:thread_counts", m_name.c_str());
		m_debuger->updateDebugValue(key_value, m_work_thread_counts);
	}
	return true;
}

//-------------------------------------------------------------------------------------
Address TcpServer::get_bind_address(size_t index)
{
	Address address;
	if (index >= m_acceptor_sockets.size()) return address;

	sockaddr_in addr;
	socket_api::getsockname(std::get<0>(m_acceptor_sockets[index]), addr);

	return Address(addr);
}

//-------------------------------------------------------------------------------------
void TcpServer::stop_listen(size_t index)
{
	assert(m_running.load()>0);
	assert(index<m_acceptor_sockets.size());
	if (index >= m_acceptor_sockets.size()) return;

	StopListenCmd cmd;
	cmd.index = index;
	m_accept_thread.send_message(StopListenCmd::ID, sizeof(cmd), (const char*)&cmd);
}

//-------------------------------------------------------------------------------------
void TcpServer::stop(void)
{
	//not running?
	if (m_running == 0) return;
	//is shutdown in processing?
	if (m_shutdown_ing.exchange(1) > 0)return;

	//this function can't run in work thread
	for (auto work : m_work_thread_pool){
		if (work->is_in_workthread()){
			CY_LOG(L_ERROR, "you can't stop server in work thread.");
			return;
		}
	}

	//shutdown the the accept thread
	ShutdownCmd shutdownCmd;
	m_accept_thread.send_message(ShutdownCmd::ID, sizeof(shutdownCmd), (const char*)&shutdownCmd);

	//shutdown all connection
	for (auto work : m_work_thread_pool){
		work->send_message(ServerWorkThread::ShutdownCmd::ID, 0, 0);
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_accept_event(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	(void)id;
	(void)event;

	//is shutdown in processing?		
	if (m_shutdown_ing.load() > 0) return;

	//call accept and create peer socket		
	socket_t connfd = socket_api::accept(fd, 0);
	if (connfd == INVALID_SOCKET)
	{
		//log error		
		CY_LOG(L_ERROR, "accept socket error");
		return;
	}

	//send it to one of work thread		
	int32_t index = _get_next_work_thread();
	ServerWorkThread* work = m_work_thread_pool[(size_t)index];
	
	//write new connection command(cmd, socket_t)		
	ServerWorkThread::NewConnectionCmd newConnectionCmd;
	newConnectionCmd.sfd = connfd;
	work->send_message(ServerWorkThread::NewConnectionCmd::ID, sizeof(newConnectionCmd), (const char*)&newConnectionCmd);

	CY_LOG(L_TRACE, "accept a socket, send to work thread %d ", index);
}

//-------------------------------------------------------------------------------------
bool TcpServer::_on_accept_start(void)
{
	int32_t counts = 0;
	for (auto& listen_socket : m_acceptor_sockets)
	{
		socket_t sfd = std::get<0>(listen_socket);
		auto& event_id = std::get<1>(listen_socket);

		//register accept event
		event_id = m_accept_thread.get_looper()->register_event(sfd,
			Looper::kRead,
			this,
			std::bind(&TcpServer::_on_accept_event, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			0);

		//begin listen
		socket_api::listen(sfd);
		counts++;
	}

	CY_LOG(L_TRACE, "accept thread run, listen %d port(s)", counts);
	return true;
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_accept_message(Packet* message)
{
	//accept thread command
	assert(message);

	uint16_t msg_id = message->get_packet_id();
	if (msg_id == ShutdownCmd::ID) {
		Looper* looper = m_accept_thread.get_looper();

		//close all listen socket(s)
		for (auto listen_socket : m_acceptor_sockets){
			auto& sfd = std::get<0>(listen_socket);
			auto& event_id = std::get<1>(listen_socket);

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
		m_acceptor_sockets.clear();

		//stop looper
		looper->push_stop_request();
	}
	else if (msg_id == DebugCmd::ID) {
		//TODO: debug accept thread
	}
	else if (msg_id == StopListenCmd::ID) {
		assert(message->get_packet_size() == sizeof(StopListenCmd));
		StopListenCmd stopListenCmd;
		memcpy(&stopListenCmd, message->get_packet_content(), sizeof(stopListenCmd));

		Looper* looper = m_accept_thread.get_looper();
		auto& listen_socket = m_acceptor_sockets[stopListenCmd.index];
		auto& sfd = std::get<0>(listen_socket);
		auto& event_id = std::get<1>(listen_socket);

		//disable event
		if (event_id != Looper::INVALID_EVENT_ID) {
			looper->disable_all(event_id);
			looper->delete_event(event_id);
			event_id = Looper::INVALID_EVENT_ID;
		}
		//close socket
		if (sfd != INVALID_SOCKET) {
			socket_api::close_socket(sfd);
			sfd = INVALID_SOCKET;
		}
	}

}

//-------------------------------------------------------------------------------------
void TcpServer::join(void)
{
	//wait accept thread quit
	m_accept_thread.join();

	//wait all thread quit
	for (auto work : m_work_thread_pool) {
		work->join();
		delete work;
	}
	m_work_thread_pool.clear();
	m_running = 0;

	CY_LOG(L_TRACE, "accept thread stop!");
}

//-------------------------------------------------------------------------------------
void TcpServer::shutdown_connection(ConnectionPtr conn)
{
	ServerWorkThread* work = (ServerWorkThread*)(conn->get_param());

	ServerWorkThread::CloseConnectionCmd closeConnectionCmd;
	closeConnectionCmd.conn_id = conn->get_id();
	closeConnectionCmd.shutdown_ing = m_shutdown_ing;
	work->send_message(ServerWorkThread::CloseConnectionCmd::ID, sizeof(closeConnectionCmd), (const char*)&closeConnectionCmd);
}

//-------------------------------------------------------------------------------------
void TcpServer::send_work_message(int32_t work_thread_index, const Packet* message)
{
	assert(work_thread_index >= 0 && work_thread_index < m_work_thread_counts);

	ServerWorkThread* work = m_work_thread_pool[(size_t)work_thread_index];
	work->send_message(message);
}

//-------------------------------------------------------------------------------------
void TcpServer::send_work_message(int32_t work_thread_index, const Packet** message, int32_t counts)
{
	assert(work_thread_index >= 0 && work_thread_index < m_work_thread_counts && counts>0);

	ServerWorkThread* work = m_work_thread_pool[(size_t)work_thread_index];
	work->send_message(message, counts);
}

//-------------------------------------------------------------------------------------
void TcpServer::debug(void)
{
	//send to work thread
	ServerWorkThread::DebugCmd cmd;
	for (auto work : m_work_thread_pool) {
		work->send_message(ServerWorkThread::DebugCmd::ID, sizeof(cmd), (const char*)&cmd);
	}

	//send to accept thread
	DebugCmd acceptDebugCmd;
	m_accept_thread.send_message(DebugCmd::ID, sizeof(acceptDebugCmd), (const char*)&acceptDebugCmd);
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_socket_connected(int32_t work_thread_index, ConnectionPtr conn)
{
	if (m_listener.onConnected) {
		m_listener.onConnected(this, work_thread_index, conn);
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_socket_message(int32_t work_thread_index, ConnectionPtr conn)
{
	if (m_listener.onMessage) {
		m_listener.onMessage(this, work_thread_index, conn);
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_socket_close(int32_t work_thread_index, ConnectionPtr conn)
{
	if (m_listener.onClose) {
		m_listener.onClose(this, work_thread_index, conn);
	}

	//shutdown this connection next tick
	shutdown_connection(conn);
}

}
