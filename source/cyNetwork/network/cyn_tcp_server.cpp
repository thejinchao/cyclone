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
TcpServer::TcpServer(const Address& addr, void* cb_param)
	: m_address(addr)
	, m_acceptor_socket(socket_api::create_non_blocking_socket())
	, m_acceptor_thread(0)
	, m_work_thread_counts(0)
	, m_next_work(0)
	, m_connection_cb(0)
	, m_message_cb(0)
	, m_close_cb(0)
	, m_callback_param(cb_param)
{
	//set accept socket option

	//
	//http://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t
	//
	m_acceptor_socket.set_reuse_port(true);
	m_acceptor_socket.set_reuse_addr(true);

	//bind address
	m_acceptor_socket.bind(addr);

	//zero work thread pool
	memset(m_work_thread_pool, 0, sizeof(m_work_thread_pool[0])*MAX_WORK_THREAD_COUNTS);
}

//-------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
	
}

//-------------------------------------------------------------------------------------
void TcpServer::start(int32_t work_thread_counts)
{
	CY_LOG(L_INFO, "TcpServer start with %d workthread(s)", work_thread_counts);

	if (work_thread_counts<1 || work_thread_counts > MAX_WORK_THREAD_COUNTS) {
		CY_LOG(L_ERROR, "param thread counts error");
		return;
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
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_accept_function(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	//call accept and create peer socket
	Address peer_addr;
	socket_t s = m_acceptor_socket.accept(peer_addr);
	if (s<0)
	{
		//TODO: log error
		return;
	}

	//send it to one of work thread
	WorkThread* work = m_work_thread_pool[_get_next_work_thread()];
	Pipe& cmd_pipe = work->get_cmd_port();

	//write new connection command(cmd, socket_t, sockaddr_in)
	int32_t cmd = WorkThread::kNewConnectionCmd;
	cmd_pipe.write((const char*)&(cmd), sizeof(int32_t));
	cmd_pipe.write((const char*)&(s), sizeof(s));
	cmd_pipe.write((const char*)&(peer_addr.get_sockaddr_in()), sizeof(struct sockaddr_in));
}

//-------------------------------------------------------------------------------------
void TcpServer::_accept_thread(void)
{
	//create a event looper
	Looper* looper = Looper::create_looper();

	//registe listen event
	looper->register_event(m_acceptor_socket.get_fd(),
		Looper::kRead,
		this,
		_on_accept_function_entry,
		0);

	//begin listen
	m_acceptor_socket.listen();

	//enter event loop...
	looper->loop();
}

//-------------------------------------------------------------------------------------
void TcpServer::join(void)
{
	//join accept thread
	thread_api::thread_join(m_acceptor_thread);
}

//-------------------------------------------------------------------------------------
void TcpServer::shutdown_connection(Connection* conn)
{
	int32_t work_index = conn->get_work_thread_index();
	assert(work_index >= 0 && work_index < m_work_thread_counts);
	
	WorkThread* work = m_work_thread_pool[work_index];

	Pipe& cmd_pipe = work->get_cmd_port();

	intptr_t conn_ptr = (intptr_t)conn;

	//write close conn command
	int32_t cmd = WorkThread::kCloseConnectionCmd;
	cmd_pipe.write((const char*)&(cmd), sizeof(int32_t));
	cmd_pipe.write((const char*)&(conn_ptr), sizeof(conn_ptr));
}

}
