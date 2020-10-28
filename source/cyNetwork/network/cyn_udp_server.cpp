/*
Copyright(C) thecodeway.com
*/
#include "cyn_udp_server.h"
#include "internal/cyn_udp_server_master_thread.h"
#include "internal/cyn_udp_server_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
UdpServer::UdpServer()
	: m_master_thread(nullptr)
	, m_workthread_counts(0)
	, m_running(0)
	, m_shutdown_ing(0)
{
	m_master_thread = new UdpServerMasterThread(this, std::bind(&UdpServer::_on_udp_message_received, this, 
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
}

//-------------------------------------------------------------------------------------
UdpServer::~UdpServer()
{
	delete m_master_thread;
}

//-------------------------------------------------------------------------------------
bool UdpServer::bind(const Address& bind_addr)
{
	//is running already?
	if (m_running > 0) return false;

	//bind socket
	return m_master_thread->bind_socket(bind_addr);
}

//-------------------------------------------------------------------------------------
bool UdpServer::start(int32_t work_thread_counts)
{
	if (work_thread_counts<1 || work_thread_counts > MAX_WORK_THREAD_COUNTS) {
		CY_LOG(L_ERROR, "param thread counts error");
		return false;
	}

	//is running already?
	if (m_running.exchange(1) > 0) return false;

	//start work thread pool
	m_workthread_counts = work_thread_counts;
	for (int32_t i = 0; i < m_workthread_counts; i++) {
		//create work thread
		m_work_thread_pool.push_back(new UdpServerWorkThread(this, i));
	}

	CY_LOG(L_INFO, "Udp server start with %zd work thread(s)", m_work_thread_pool.size());

	//start all work thread
	for (UdpServerWorkThread* workThread : m_work_thread_pool) {
		if (!workThread->start()) {
			return false;
		}
	}

	//start master thread
	if (!m_master_thread->start()) {
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
void UdpServer::join(void)
{
	//wait master thread quit
	if (m_master_thread) {
		m_master_thread->join();
	}

	//wait all thread quit
	for (UdpServerWorkThread* workThread : m_work_thread_pool) {
		workThread->join();
		delete workThread;
	}
	m_work_thread_pool.clear();

	m_running = 0;
	CY_LOG(L_TRACE, "udp server stop!");
}

//-------------------------------------------------------------------------------------
void UdpServer::stop(void)
{
	//not running?
	if (m_running == 0) return;
	//is shutdown in processing?
	if (m_shutdown_ing.exchange(1) > 0)return;

	//this function can't run in work thread
	for (auto work : m_work_thread_pool) {
		if (work->is_in_workthread()) {
			CY_LOG(L_ERROR, "you can't stop server in work thread.");
			return;
		}
	}

	//shutdown the the master thread
	m_master_thread->send_thread_message(UdpServerMasterThread::kShutdownCmdID, 0, nullptr);

	//shutdown all connection
	for (UdpServerWorkThread* workThread : m_work_thread_pool) {
		workThread->send_thread_message(UdpServerWorkThread::kShutdownCmdID, 0, nullptr);
	}
}

//-------------------------------------------------------------------------------------
void UdpServer::shutdown_connection(UdpConnectionPtr conn)
{
	const sockaddr_in& peer_addr = conn->get_peer_addr().get_sockaddr_in();
	int32_t thread_index = Address::hash_value(peer_addr) % m_workthread_counts;
	
	UdpServerWorkThread* work_thread = m_work_thread_pool[thread_index];

	UdpServerWorkThread::CloseConnectionCmd closeConnectionCmd;
	memcpy(&closeConnectionCmd.peer_address, &peer_addr, sizeof(peer_addr));
	closeConnectionCmd.shutdown_ing = m_shutdown_ing.load();

	work_thread->send_thread_message(UdpServerWorkThread::CloseConnectionCmd::ID, sizeof(closeConnectionCmd), (const char*)&closeConnectionCmd);
}

//-------------------------------------------------------------------------------------
void UdpServer::_on_udp_message_received(const char* buf, int32_t len, const sockaddr_in& peer_address, const sockaddr_in& local_address)
{
	//not running?
	if (m_running == 0) return;

	//thread index
	int32_t thread_index = Address::hash_value(peer_address) % m_workthread_counts;

	//send to work thread
	UdpServerWorkThread::ReceiveUdpMessage receiveUdpMessage;
	memcpy(&receiveUdpMessage.local_address, &local_address, sizeof(sockaddr_in));
	memcpy(&receiveUdpMessage.peer_address, &peer_address, sizeof(sockaddr_in));

	m_work_thread_pool[thread_index]->send_thread_message(UdpServerWorkThread::kReceiveUdpMessage, 
		(uint16_t)sizeof(receiveUdpMessage), (const char*)&receiveUdpMessage, (uint16_t)len, buf);
}

//-------------------------------------------------------------------------------------
void UdpServer::_on_socket_connected(int32_t work_thread_index, UdpConnectionPtr conn)
{
	if (m_listener.on_connected) {
		m_listener.on_connected(this, work_thread_index, conn);
	}
}

//-------------------------------------------------------------------------------------
void UdpServer::_on_socket_message(int32_t work_thread_index, UdpConnectionPtr conn)
{
	if (m_listener.on_message) {
		m_listener.on_message(this, work_thread_index, conn);
	}
}

//-------------------------------------------------------------------------------------
void UdpServer::_on_socket_close(int32_t work_thread_index, UdpConnectionPtr conn)
{
	if (m_listener.on_close) {
		m_listener.on_close(this, work_thread_index, conn);
	}

	//shutdown this connection next tick
	shutdown_connection(conn);
}

}
