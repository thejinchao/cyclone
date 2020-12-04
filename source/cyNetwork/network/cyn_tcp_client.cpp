/*
Copyright(C) thecodeway.com
*/
#include <cy_event.h>
#include <cy_network.h>

#include "cyn_tcp_client.h"

namespace cyclone
{

#define RELEASE_EVENT(looper, id) \
	if (id != Looper::INVALID_EVENT_ID) { \
		looper->delete_event(id); \
		id = Looper::INVALID_EVENT_ID; \
	}
//-------------------------------------------------------------------------------------
TcpClient::TcpClient(Looper* looper, void* param, int id)
	: m_id(id)
	, m_socket(INVALID_SOCKET)
	, m_socket_event_id(Looper::INVALID_EVENT_ID)
	, m_retry_timer_id(Looper::INVALID_EVENT_ID)
	, m_looper(looper)
	, m_param(param)
	, m_connection(nullptr)
{
	//looper must be set
	assert(looper);

	m_connection_lock = sys_api::mutex_create();

	m_listener.on_connected = nullptr;
	m_listener.on_message = nullptr;
	m_listener.on_close = nullptr;
}

//-------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	sys_api::mutex_destroy(m_connection_lock);
	m_connection_lock = nullptr;

	RELEASE_EVENT(m_looper, m_socket_event_id);
	RELEASE_EVENT(m_looper, m_retry_timer_id);

	if (m_connection) {
		assert(m_connection->get_state() == TcpConnection::kDisconnected);
	}
}

//-------------------------------------------------------------------------------------
bool TcpClient::connect(const Address& addr)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_socket == INVALID_SOCKET);

	sys_api::auto_mutex lock(m_connection_lock);

	//create socket
	m_socket = socket_api::create_socket();
	//set socket to non-block and close-onexec
	socket_api::set_nonblock(m_socket, true);
	socket_api::set_close_onexec(m_socket, true);
	//set other socket option
	socket_api::set_keep_alive(m_socket, true);
	socket_api::set_linger(m_socket, false, 0);

	m_serverAddr = addr;

	//set event callback
	m_socket_event_id = m_looper->register_event(m_socket, Looper::kRead | Looper::kWrite, this,
		std::bind(&TcpClient::_on_socket_read_write, this),
		std::bind(&TcpClient::_on_socket_read_write, this)
		);

	//start connect to server
	if (!socket_api::connect(m_socket, addr.get_sockaddr_in()))
	{
		CY_LOG(L_ERROR, "connect to server error, errno=%d", socket_api::get_lasterror());
		return false;
	}

	CY_LOG(L_DEBUG, "begin connect to %s:%d", addr.get_ip(), addr.get_port());
	return true;
}

//-------------------------------------------------------------------------------------
TcpConnection::State TcpClient::get_connection_state(void) const
{
	sys_api::auto_mutex lock(m_connection_lock);

	if (m_connection) return m_connection->get_state();
	else return (m_socket==INVALID_SOCKET) ? TcpConnection::kDisconnected : TcpConnection::kConnecting;
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_connect_status_changed(bool timeout)
{
	assert(m_connection == nullptr);

	if (timeout || socket_api::get_socket_error(m_socket) != 0) {
		//logic callback
		uint32_t retry_sleep_ms = 0;
		if (m_listener.on_connected) {
			retry_sleep_ms = m_listener.on_connected(shared_from_this(), nullptr, false);
		}
		CY_LOG(L_DEBUG, "connect to %s:%d failed! ", m_serverAddr.get_ip(), m_serverAddr.get_port());
		_abort_connect(retry_sleep_ms);
	}
	else {
		//connect success!
		
		//remove from event system, taked by TcpConnection
		RELEASE_EVENT(m_looper, m_socket_event_id);

		//established the connection
		m_connection = std::make_shared<TcpConnection>(m_id, m_socket, m_looper, this);
		CY_LOG(L_DEBUG, "connect to %s:%d success", m_serverAddr.get_ip(), m_serverAddr.get_port());

		//bind callback functions
		if (m_listener.on_message) {
			m_connection->set_on_message([this](TcpConnectionPtr conn) {
				m_listener.on_message(shared_from_this(), conn);
			});
		}

		if(m_listener.on_close) {
			m_connection->set_on_close([this](TcpConnectionPtr conn) {
				CY_LOG(L_DEBUG, "disconnect from %s:%d", m_serverAddr.get_ip(), m_serverAddr.get_port());
				m_listener.on_close(shared_from_this(), conn);
			});
		}

		//send cached message
		if (!m_sendCache.empty()) {
			m_connection->send((const char*)m_sendCache.normalize(), m_sendCache.size());
			m_sendCache.reset();
		}
		//logic callback
		if (m_listener.on_connected) {
			m_listener.on_connected(shared_from_this(), m_connection, true);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_abort_connect(uint32_t retry_sleep_ms)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(get_connection_state() == TcpConnection::kConnecting);

	RELEASE_EVENT(m_looper, m_socket_event_id);
	RELEASE_EVENT(m_looper, m_retry_timer_id);

	//close current socket
	socket_api::close_socket(m_socket);
	m_socket = INVALID_SOCKET;

	if (retry_sleep_ms>0) {
		//retry connection? create retry the timer
		m_retry_timer_id = m_looper->register_timer_event(retry_sleep_ms, this,
			std::bind(&TcpClient::_on_retry_connect_timer, this, std::placeholders::_1));
		CY_LOG(L_DEBUG, "try connect to %s:%d after %d mill seconds", m_serverAddr.get_ip(), m_serverAddr.get_port(), retry_sleep_ms);
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::disconnect(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	m_sendCache.reset();
	switch (get_connection_state())
	{
	case TcpConnection::kDisconnected:
		break;

	case TcpConnection::kConnecting:
		_abort_connect(0u);
		break;

	default:
		assert(m_connection != nullptr);
		m_connection->shutdown();
		break;
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_retry_connect_timer(Looper::event_id_t id)
{
	assert(id == m_retry_timer_id);
	assert(m_connection == nullptr);

	//remove the timer
	RELEASE_EVENT(m_looper, m_retry_timer_id);

	//connect again
	if (!connect(m_serverAddr)) {
		//failed at once!, logic callback
		if (m_listener.on_connected) {
			uint32_t retry_sleep_ms = m_listener.on_connected(shared_from_this(), nullptr, false);

			//retry connection?
			if (retry_sleep_ms>0) {
				m_retry_timer_id = m_looper->register_timer_event(retry_sleep_ms, this,
					std::bind(&TcpClient::_on_retry_connect_timer, this, std::placeholders::_1));
			}
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_socket_read_write(void)
{
	if (get_connection_state() == TcpConnection::kConnecting) {
		_on_connect_status_changed(false);
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::send(const char* buf, size_t len)
{
	switch (get_connection_state())
	{
	case TcpConnection::kConnecting:
	{
		assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
		m_sendCache.memcpy_into(buf, len);
	}
		break;
	case TcpConnection::kConnected:
	{
		m_connection->send(buf, len);
	}
		break;
	default:
		break;
	}
	
}

}

