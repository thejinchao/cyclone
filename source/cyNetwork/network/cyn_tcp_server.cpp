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
TcpServer::TcpServer(Listener* listener, const char* name, DebugInterface* debuger)
	: m_accept_looper(0)
	, m_acceptor_thread(0)
	, m_work_thread_counts(0)
	, m_next_work(0)
	, m_listener(listener)
	, m_running(0)
	, m_shutdown_ing(0)
	, m_next_connection_id(0)
	, m_name(name ? name : "server")
	, m_debuger(debuger)
{

}

//-------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
	for (auto sfd : m_acceptor_sockets) {
		socket_api::close_socket(sfd);
	}
	m_acceptor_sockets.clear();
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
	m_acceptor_sockets.push_back(sfd);
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
	m_acceptor_thread = sys_api::thread_create(
		std::bind(&TcpServer::_accept_thread, this), this, "accept");

	//write debug variable
	if (m_debuger && m_debuger->is_enable()) {
		char key_value[256] = { 0 };
		snprintf(key_value, 256, "TcpServer:%s:thread_counts", m_name.c_str());
		m_debuger->set_debug_value(key_value, m_work_thread_counts);
	}
	return true;
}

//-------------------------------------------------------------------------------------
Address TcpServer::get_bind_address(size_t index)
{
	Address address;
	if (index >= m_acceptor_sockets.size()) return address;

	sockaddr_in addr;
	socket_api::getsockname(m_acceptor_sockets[index], addr);

	return Address(addr);
}

//-------------------------------------------------------------------------------------
void TcpServer::stop(void)
{
	//not running?
	if (m_running == 0) return;

	//this function can't run in work thread
	for (auto work : m_work_thread_pool){
		if (work->is_in_workthread()){
			CY_LOG(L_ERROR, "you can't stop server in work thread.");
			return;
		}
	}

	//is shutdown in processing?
	if (m_shutdown_ing.exchange(1) > 0)return;

	//touch the first listen socket, cause the accept thread quit
	Address address(get_bind_address(0).get_port(), true);
	socket_t s = socket_api::create_socket();
	socket_api::connect(s, address.get_sockaddr_in());
	socket_api::close_socket(s);

	//first shutdown all connection
	for (auto work : m_work_thread_pool){
		work->send_message(ServerWorkThread::ShutdownCmd::ID, 0, 0);
	}

	//wait all thread quit
	for (auto work : m_work_thread_pool){
		work->join();
		delete work;
	}
	m_work_thread_pool.clear();

	//wait accept thread quit
	sys_api::thread_join(m_acceptor_thread);

	//close the listen socket
	for (auto sfd : m_acceptor_sockets){
		socket_api::close_socket(sfd);
	}
	m_acceptor_sockets.clear();

	//OK!
	return;
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_accept_function(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	(void)id;
	(void)event;

	//is shutdown in processing?		
	if (m_shutdown_ing > 0) {
		//push loop request command
		m_accept_looper->push_stop_request();
		return;
	}

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
void TcpServer::_accept_thread(void)
{
	//create a event looper		
	m_accept_looper = Looper::create_looper();

	//registe listen event	
	int32_t counts = 0;
	for (auto sfd : m_acceptor_sockets)
	{
		m_accept_looper->register_event(sfd,
			Looper::kRead,
			this,
			std::bind(&TcpServer::_on_accept_function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			0);

		//begin listen
		socket_api::listen(sfd);
		counts++;
	}

	CY_LOG(L_TRACE, "accept thread run, listen %d port(s)", counts);
	m_accept_looper->loop();

	//it's time to disppear...
	Looper::destroy_looper(m_accept_looper);
	m_accept_looper = nullptr;
}

//-------------------------------------------------------------------------------------
void TcpServer::join(void)
{
	assert(m_acceptor_thread);

	//join accept thread
	sys_api::thread_join(m_acceptor_thread);
}

//-------------------------------------------------------------------------------------
void TcpServer::shutdown_connection(Connection* conn)
{
	assert(m_acceptor_thread);

	ServerWorkThread* work = (ServerWorkThread*)(conn->get_listener());

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
Connection* TcpServer::get_connection(int32_t work_thread_index, int32_t conn_id)
{
	assert(work_thread_index >= 0 && work_thread_index < m_work_thread_counts);
	ServerWorkThread* work = m_work_thread_pool[(size_t)work_thread_index];
	assert(work->is_in_workthread());

	return work->get_connection(conn_id);
}

//-------------------------------------------------------------------------------------
void TcpServer::debug(void)
{
	ServerWorkThread::DebugCmd cmd;

	for (auto work : m_work_thread_pool) {
		work->send_message(ServerWorkThread::DebugCmd::ID, sizeof(cmd), (const char*)&cmd);
	}
}

}
