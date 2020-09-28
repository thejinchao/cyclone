/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include "cyn_tcp_server.h"
#include "cyn_server_master_thread.h"
#include "cyn_server_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpServer::TcpServer(const char* name, void* param)
	: m_param(param)
	, m_master_thread(nullptr)
	, m_workthread_counts(0)
	, m_next_workthread_id(0)
	, m_running(0)
	, m_shutdown_ing(0)
	, m_next_connection_id(kStartConnectionID)  //start from 1
	, m_name(name ? name : "server")
{
	m_listener.on_master_thread_start = nullptr;
	m_listener.on_master_thread_command = nullptr;

	m_listener.on_work_thread_start = nullptr;
	m_listener.on_work_thread_command = nullptr;

	m_listener.on_connected = nullptr;
	m_listener.on_message = nullptr;
	m_listener.on_close = nullptr;

	m_master_thread = new ServerMasterThread(this);
}

//-------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
	assert(m_running.load()==0);
	if (m_master_thread)
	{
		delete m_master_thread;
		m_master_thread = nullptr;
	}
}

//-------------------------------------------------------------------------------------
bool TcpServer::bind(const Address& bind_addr, bool enable_reuse_port)
{
	//is running already?
	if (m_running > 0) return false;

	return m_master_thread->bind_socket(bind_addr, enable_reuse_port);
}

//-------------------------------------------------------------------------------------
bool TcpServer::start(int32_t work_thread_counts)
{
	CY_LOG(L_INFO, "TcpServer start with %d work thread(s)", work_thread_counts);

	if (work_thread_counts<1 || work_thread_counts > MAX_WORK_THREAD_COUNTS) {
		CY_LOG(L_ERROR, "param thread counts error");
		return false;
	}

	//is running already?
	if (m_running.exchange(1) > 0) return false;

	//start work thread pool
	m_workthread_counts = work_thread_counts;
	for (int32_t i = 0; i < m_workthread_counts; i++) {
		//run the thread
		m_work_thread_pool.push_back(new ServerWorkThread(i, this, m_name.c_str()));
	}

	//start master thread
	if (!m_master_thread->start()) {
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
Address TcpServer::get_bind_address(size_t index)
{
	return m_master_thread->get_bind_address(index);
}

//-------------------------------------------------------------------------------------
void TcpServer::stop_listen(size_t index)
{
	assert(m_running.load()>0 && m_master_thread);
	assert(index<m_master_thread->get_bind_socket_size());
	if (index >= m_master_thread->get_bind_socket_size()) return;

	//send stop listen cmd to master thread
	ServerMasterThread::StopListenCmd cmd;
	cmd.index = index;
	m_master_thread->send_message(ServerMasterThread::StopListenCmd::ID, sizeof(cmd), (const char*)&cmd);
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

	//shutdown the the master thread
	ServerMasterThread::ShutdownCmd shutdownCmd;
	m_master_thread->send_message(ServerMasterThread::ShutdownCmd::ID, sizeof(shutdownCmd), (const char*)&shutdownCmd);

	//shutdown all connection
	for (auto work : m_work_thread_pool){
		work->send_message(ServerWorkThread::ShutdownCmd::ID, 0, 0);
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_accept_socket(socket_t fd)
{
	//send it to one of work thread		
	int32_t index = (m_next_workthread_id++) % m_workthread_counts;
	ServerWorkThread* work = m_work_thread_pool[(size_t)index];

	//send new connection to work thread(cmd, socket_t)		
	ServerWorkThread::NewConnectionCmd newConnectionCmd;
	newConnectionCmd.sfd = fd;
	work->send_message(ServerWorkThread::NewConnectionCmd::ID, sizeof(newConnectionCmd), (const char*)&newConnectionCmd);

	CY_LOG(L_TRACE, "accept a socket, send to work thread %d ", index);
}

//-------------------------------------------------------------------------------------
void TcpServer::join(void)
{
	//wait accept thread quit
	if (m_master_thread) {
		m_master_thread->join();
	}

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
	ServerWorkThread* work = (ServerWorkThread*)(conn->get_owner());

	ServerWorkThread::CloseConnectionCmd closeConnectionCmd;
	closeConnectionCmd.conn_id = conn->get_id();
	closeConnectionCmd.shutdown_ing = m_shutdown_ing;
	work->send_message(ServerWorkThread::CloseConnectionCmd::ID, sizeof(closeConnectionCmd), (const char*)&closeConnectionCmd);
}

//-------------------------------------------------------------------------------------
void TcpServer::send_master_message(uint16_t id, uint16_t size, const char* message)
{
	if (!m_master_thread) return;

	m_master_thread->send_message(id, size, message);
}

//-------------------------------------------------------------------------------------
void TcpServer::send_master_message(const Packet* message)
{
	if (!m_master_thread) return;

	m_master_thread->send_message(message);
}

//-------------------------------------------------------------------------------------
void TcpServer::send_work_message(int32_t work_thread_index, const Packet* message)
{
	assert(work_thread_index >= 0 && work_thread_index < m_workthread_counts);

	ServerWorkThread* work = m_work_thread_pool[(size_t)work_thread_index];
	work->send_message(message);
}

//-------------------------------------------------------------------------------------
void TcpServer::send_work_message(int32_t work_thread_index, const Packet** message, int32_t counts)
{
	assert(work_thread_index >= 0 && work_thread_index < m_workthread_counts && counts>0);

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

	//send to master thread
	ServerMasterThread::DebugCmd acceptDebugCmd;
	m_master_thread->send_message(ServerMasterThread::DebugCmd::ID, sizeof(acceptDebugCmd), (const char*)&acceptDebugCmd);
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_socket_connected(int32_t work_thread_index, ConnectionPtr conn)
{
	if (m_listener.on_connected) {
		m_listener.on_connected(this, work_thread_index, conn);
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_socket_message(int32_t work_thread_index, ConnectionPtr conn)
{
	if (m_listener.on_message) {
		m_listener.on_message(this, work_thread_index, conn);
	}
}

//-------------------------------------------------------------------------------------
void TcpServer::_on_socket_close(int32_t work_thread_index, ConnectionPtr conn)
{
	if (m_listener.on_close) {
		m_listener.on_close(this, work_thread_index, conn);
	}

	//shutdown this connection next tick
	shutdown_connection(conn);
}

}
