/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include "cyn_tcp_server.h"
#include "cyn_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpServer::TcpServer(void* cb_param)
	: m_acceptor_socket(0)
	, m_acceptor_thread(0)
	, m_work_thread_counts(0)
	, m_next_work(0)
	, m_callback_param(cb_param)
	, m_connection_cb(0)
	, m_message_cb(0)
	, m_close_cb(0)
{
	//zero work thread pool
	memset(m_work_thread_pool, 0, sizeof(m_work_thread_pool[0])*MAX_WORK_THREAD_COUNTS);
}

//-------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
	if (m_acceptor_socket)
		delete m_acceptor_socket;
}

//-------------------------------------------------------------------------------------
bool TcpServer::start(const Address& bind_addr, 
	bool enable_reuse_port,
	int32_t work_thread_counts)
{
	assert(m_acceptor_socket == 0);

	CY_LOG(L_INFO, "TcpServer start with %d workthread(s)", work_thread_counts);

	if (work_thread_counts<1 || work_thread_counts > MAX_WORK_THREAD_COUNTS) {
		CY_LOG(L_ERROR, "param thread counts error");
		return false;
	}

	//store bind address
	m_address = Address(bind_addr);

	//create a non blocking socket
	socket_t sfd = socket_api::create_socket();
	if (sfd == INVALID_SOCKET) {
		CY_LOG(L_ERROR, "create socket error");
		return false;
	}
	m_acceptor_socket = new Socket(sfd);

	//set socket close on exe flag, the file  descriptor will be closed open across an execve.
	socket_api::set_close_onexec(m_acceptor_socket->get_fd(), true);

	//set accept socket option
	if (enable_reuse_port) {
		//http://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t
		m_acceptor_socket->set_reuse_port(true);
		m_acceptor_socket->set_reuse_addr(true);
	}

	//bind address
	if (!(m_acceptor_socket->bind(m_address))){
		CY_LOG(L_ERROR, "bind to address %s:%d failed", m_address.get_ip(), m_address.get_port());
		delete m_acceptor_socket;
		m_acceptor_socket = 0;
		return false;
	}

	//start work thread pool
	m_work_thread_counts = work_thread_counts;
	for (int32_t i = 0; i < m_work_thread_counts; i++)
	{
		//run the thread
		m_work_thread_pool[i] = new WorkThread(this, i);
	}

	//start listen thread
	m_acceptor_thread = thread_api::thread_create(_accept_thread_entry, this);
	return true;
}

//-------------------------------------------------------------------------------------
void TcpServer::stop(void)
{
	//this function can't run in work thread
	for (int32_t i = 0; i < m_work_thread_counts; i++){
		WorkThread* work = m_work_thread_pool[i];
		if (thread_api::thread_get_id(work->get_thread()) == thread_api::thread_get_current_id())
		{
			CY_LOG(L_ERROR, "you can't stop server in work thread.");
			return;
		}
	}

	//is shutdown in processing?
	if (m_shutdown_ing.get_and_set(1) > 0)return;

	//touch the listen socket, cause the accept thread quit
	socket_t s = socket_api::create_socket();
	socket_api::connect(s, Address(m_address.get_port(), true).get_sockaddr_in());
	socket_api::close_socket(s);

	//first shutdown all connection
	for (int32_t i = 0; i < m_work_thread_counts; i++){
		WorkThread* work = m_work_thread_pool[i];
		Pipe& cmd_pipe = work->get_cmd_port();

		int32_t cmd = WorkThread::kShutdownCmd;
		cmd_pipe.write((const char*)&(cmd), sizeof(int32_t));
	}

	//wait all thread quit
	for (int32_t i = 0; i < m_work_thread_counts; i++){
		WorkThread* work = m_work_thread_pool[i];

		thread_api::thread_join(work->get_thread());
		delete work;
	}

	//wait accept thread quit
	thread_api::thread_join(m_acceptor_thread);

	//close the listen socket
	delete m_acceptor_socket; 
	m_acceptor_socket = 0;

	//OK!
	return;
}

//-------------------------------------------------------------------------------------
void TcpServer::_accept_thread(void)
{
	//begin listen
	m_acceptor_socket->listen();

	//enter accept loop...
	for (;;)
	{
		//blocking call
		socket_t connfd = socket_api::accept(m_acceptor_socket->get_fd(), 0);
		//is shutdown in processing?
		if (m_shutdown_ing.get() > 0) break;

		if (connfd == INVALID_SOCKET)
		{
			//log error
			CY_LOG(L_ERROR, "accept socket error");
			continue;
		}

		//send it to one of work thread
		WorkThread* work = m_work_thread_pool[_get_next_work_thread()];
		Pipe& cmd_pipe = work->get_cmd_port();

		//write new connection command(cmd, socket_t)
		int32_t cmd = WorkThread::kNewConnectionCmd;
		cmd_pipe.write((const char*)&(cmd), sizeof(int32_t));
		cmd_pipe.write((const char*)&(connfd), sizeof(socket_t));
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::join(void)
{
	assert(m_acceptor_socket);
	assert(m_acceptor_thread);

	//join accept thread
	thread_api::thread_join(m_acceptor_thread);
}

//-------------------------------------------------------------------------------------
void TcpServer::shutdown_connection(Connection* conn)
{
	assert(m_acceptor_socket);
	assert(m_acceptor_thread);

	int32_t work_index = conn->get_work_thread_index();
	assert(work_index >= 0 && work_index < m_work_thread_counts);
	
	WorkThread* work = m_work_thread_pool[work_index];

	Pipe& cmd_pipe = work->get_cmd_port();

	intptr_t conn_ptr = (intptr_t)conn;
	int32_t shutdown_ing = m_shutdown_ing.get();

	//write close conn command
	int32_t cmd = WorkThread::kCloseConnectionCmd;
	cmd_pipe.write((const char*)&(cmd), sizeof(int32_t));
	cmd_pipe.write((const char*)&(conn_ptr), sizeof(conn_ptr));
	cmd_pipe.write((const char*)&(shutdown_ing), sizeof(shutdown_ing));
}

}
