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
TcpServer::TcpServer(Listener* listener)
	: m_acceptor_thread(0)
	, m_work_thread_counts(0)
	, m_next_work(0)
	, m_listener(listener)
{
	//zero accept socket 
	memset(m_acceptor_socket, 0, sizeof(Socket*)*MAX_BIND_PORT_COUNTS);

	//zero work thread pool
	memset(m_work_thread_pool, 0, sizeof(m_work_thread_pool[0])*MAX_WORK_THREAD_COUNTS);
}

//-------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
	for (int i = 0; i < MAX_BIND_PORT_COUNTS; i++)
	{
		if (m_acceptor_socket[i])
			delete m_acceptor_socket[i];
	}
}

//-------------------------------------------------------------------------------------
bool TcpServer::bind(const Address& bind_addr, bool enable_reuse_port)
{
	//is running already?
	if (m_running.get() > 0) return false;

	//find a empty listen socket slot
	int index = -1;
	for (int i = 0; i < MAX_BIND_PORT_COUNTS; i++)
	{
		if (m_acceptor_socket[i] == 0)
		{
			index = i;
			break;
		}
	}
	if (index < 0) return false;

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

	Socket* acceptor_socket = new Socket(sfd);

	//set accept socket option
	if (enable_reuse_port) {
		//http://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t
		acceptor_socket->set_reuse_port(true);
		acceptor_socket->set_reuse_addr(true);
	}

	//bind address
	if (!(socket_api::bind(sfd, bind_addr.get_sockaddr_in()))){
		CY_LOG(L_ERROR, "bind to address %s:%d failed", bind_addr.get_ip(), bind_addr.get_port());
		delete acceptor_socket;
		return false;
	}

	m_acceptor_socket[index] = acceptor_socket;
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

	if (m_acceptor_socket[0] == 0) {
		CY_LOG(L_ERROR, "at least listen one port!");
		return false;
	}

	//is running already?
	if (m_running.get_and_set(1) > 0) return false;

	//start work thread pool
	m_work_thread_counts = work_thread_counts;
	for (int32_t i = 0; i < m_work_thread_counts; i++)
	{
		//run the thread
		m_work_thread_pool[i] = new WorkThread(i, this);
	}

	//start listen thread
	m_acceptor_thread = thread_api::thread_create(_accept_thread_entry, this);
	return true;
}

//-------------------------------------------------------------------------------------
Address TcpServer::get_bind_address(int index)
{
	Address address;
	if (index < 0 || index >= MAX_BIND_PORT_COUNTS) return address;
	if (m_acceptor_socket[index] == 0) return address;

	sockaddr_in addr;
	socket_api::getsockname(m_acceptor_socket[index]->get_fd(), addr);

	return Address(addr);
}

//-------------------------------------------------------------------------------------
void TcpServer::stop(void)
{
	//not running?
	if (m_running.get() == 0) return;

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

	//touch the first listen socket, cause the accept thread quit
	Address address(get_bind_address(0).get_port(), true);
	socket_t s = socket_api::create_socket();
	socket_api::connect(s, address.get_sockaddr_in());
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
	for (int i = 0; i < MAX_BIND_PORT_COUNTS; i++)
	{
		if (m_acceptor_socket[i] == 0) break;

		delete m_acceptor_socket[i];
		m_acceptor_socket[i] = 0;
	}

	//OK!
	return;
}

//-------------------------------------------------------------------------------------
bool TcpServer::_on_accept_function(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	(void)id;
	(void)event;

	//is shutdown in processing?		
	if (m_shutdown_ing.get() > 0) return true;

	//call accept and create peer socket		
	socket_t connfd = socket_api::accept(fd, 0);
	if (connfd == INVALID_SOCKET)
	{
		//log error		
		CY_LOG(L_ERROR, "accept socket error");
		return false;
	}

	//send it to one of work thread		
	WorkThread* work = m_work_thread_pool[_get_next_work_thread()];
	Pipe& cmd_pipe = work->get_cmd_port();

	//write new connection command(cmd, socket_t, sockaddr_in)		
	int32_t cmd = WorkThread::kNewConnectionCmd;
	cmd_pipe.write((const char*)&(cmd), sizeof(int32_t));
	cmd_pipe.write((const char*)&(connfd), sizeof(connfd));
	return false;
}

//-------------------------------------------------------------------------------------
void TcpServer::_accept_thread(void)
{
	//create a event looper		
	Looper* looper = Looper::create_looper();

	//registe listen event	
	for (int i = 0; i < MAX_BIND_PORT_COUNTS; i++)
	{
		if(m_acceptor_socket[i] == 0) break;

		socket_t sfd = m_acceptor_socket[i]->get_fd();

		looper->register_event(sfd,
			Looper::kRead,
			this,
			_on_accept_function_entry,
			0);

		//begin listen
		socket_api::listen(sfd);
	}

	looper->loop();

	//it's time to disppear...
	Looper::destroy_looper(looper);
	return;
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

	WorkThread* work = (WorkThread*)(conn->get_listener());

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
