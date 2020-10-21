/*
Copyright(C) thecodeway.com
*/
#include "cyn_udp_server.h"
#include "internal/cyn_udp_server_work_thread.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
UdpServer::UdpServer(int32_t max_thread_counts)
	: m_running(0)
	, m_max_work_thread_counts(max_thread_counts)
	, m_socket_counts(0)
{
	if (max_thread_counts<=0 || max_thread_counts > MAX_WORK_THREAD_COUNTS) 
	{
		CY_LOG(L_ERROR, "Wrong thread counts: %d", max_thread_counts);
		m_max_work_thread_counts = sys_api::get_cpu_counts();
	}
}

//-------------------------------------------------------------------------------------
UdpServer::~UdpServer()
{

}

//-------------------------------------------------------------------------------------
int32_t UdpServer::bind(const Address& bind_addr)
{
	//is running already?
	if (m_running > 0) return false;
	size_t index = m_socket_counts % m_max_work_thread_counts;

	//pick work thread
	UdpServerWorkThread* workThread = nullptr;
	if (m_work_thread_pool.size() <= index) {
		//need create work thread
		workThread = new UdpServerWorkThread((int32_t)index, this);
		m_work_thread_pool.push_back(workThread);
	}
	else {
		workThread = m_work_thread_pool[index];
	}

	//bind socket
	bool bind_success = workThread->bind_socket(bind_addr);
	if (bind_success) {
		m_socket_counts++;
	}

	return bind_success ? (int32_t)index : -1;
}

//-------------------------------------------------------------------------------------
bool UdpServer::start(void)
{
	//is running already?
	if (m_running.exchange(1) > 0) return false;
	CY_LOG(L_INFO, "UdpServer start with %zd work thread(s) and %d socket(s)", m_work_thread_pool.size(), m_socket_counts);

	//start all server
	for (UdpServerWorkThread* workThread : m_work_thread_pool) {
		if (!workThread->start()) {
			return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------------------------
void UdpServer::join(void)
{
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

	//this function can't run in work thread
	for (auto work : m_work_thread_pool) {
		if (work->is_in_workthread()) {
			CY_LOG(L_ERROR, "you can't stop server in work thread.");
			return;
		}
	}

	//shutdown all connection
	for (UdpServerWorkThread* workThread : m_work_thread_pool) {
		workThread->send_thread_message(UdpServerWorkThread::ShutdownCmd::ID, 0, nullptr);
	}
}

//-------------------------------------------------------------------------------------
void UdpServer::sendto(int32_t thread_index, int32_t socket_index, const char* buf, size_t len, const Address& peer_address)
{
	assert(m_running.load() != 0);
	assert(thread_index >= 0 && thread_index < (int32_t)m_work_thread_pool.size());

	//not running?
	if (m_running == 0) return;
	if (thread_index < 0 || thread_index >= (int32_t)m_work_thread_pool.size()) return;

	//must in work thread
	UdpServerWorkThread* workThread = m_work_thread_pool[thread_index];
	assert(workThread->is_in_workthread());

	//send to 
	workThread->sendto(socket_index, buf, len, peer_address);
}

}
