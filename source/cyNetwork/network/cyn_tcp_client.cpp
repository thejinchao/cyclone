/*
Copyright(C) thecodeway.com
*/
#include <cy_event.h>
#include <cy_network.h>

#include "cyn_tcp_client.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpClient::TcpClient(Looper* looper, Listener* listener, void* param)
	: m_socket(0)
	, m_socket_event_id(Looper::INVALID_EVENT_ID)
	, m_connect_timeout_ms(0)
	, m_looper(looper)
	, m_listener(listener)
	, m_param(param)
	, m_connection(0)
	, m_retry_timer_id(Looper::INVALID_EVENT_ID)
#ifdef CY_SYS_WINDOWS
	, m_connection_timer_id(Looper::INVALID_EVENT_ID)
#endif
{
	//looper muste be setted
	assert(looper);

	m_connection_lock = sys_api::mutex_create();
}

//-------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	sys_api::mutex_destroy(m_connection_lock);
	m_connection_lock = 0;

	if (m_socket_event_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_socket_event_id);
		m_looper->delete_event(m_socket_event_id);
		m_socket_event_id = Looper::INVALID_EVENT_ID;
	}

	if (m_connection) {
		delete m_connection;
		m_connection = 0;
	}

#ifdef CY_SYS_WINDOWS
	//close connection wait timer(if we close client before wait timer happen)
	if (m_connection_timer_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_connection_timer_id);
		m_looper->delete_event(m_connection_timer_id);
		m_connection_timer_id = Looper::INVALID_EVENT_ID;
	}
#endif

	if (m_retry_timer_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_retry_timer_id);
		m_looper->delete_event(m_retry_timer_id);
		m_retry_timer_id = Looper::INVALID_EVENT_ID;
	}
}

//-------------------------------------------------------------------------------------
bool TcpClient::connect(const Address& addr, uint32_t timeOutSeconds)
{
	sys_api::auto_mutex lock(m_connection_lock);

	//check status
	if (m_connection != 0) return false;

	//create socket
	m_socket = socket_api::create_socket();
	m_serverAddr = addr;
	m_connect_timeout_ms = timeOutSeconds;
	m_connection = new Connection(0, m_socket, m_looper, this);

	//set event callback
	m_socket_event_id = m_looper->register_event(m_socket, Looper::kRead | Looper::kWrite, this,
		std::bind(&TcpClient::_on_socket_read_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
		std::bind(&TcpClient::_on_socket_read_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
		);

	//start connect to server
	if (!socket_api::connect(m_socket, addr.get_sockaddr_in()))
	{
		CY_LOG(L_ERROR, "connect to server error");
		return false;
	}

#ifdef CY_SYS_WINDOWS
	//for select mode in windows, the non-block fd of connect socket wouldn't be readable or writeable if connection failed
	//but... it's ugly...
	m_connection_timer_id = m_looper->register_timer_event(timeOutSeconds, this, 
		std::bind(&TcpClient::_on_connection_timer, this, std::placeholders::_1));
#endif
	return true;
}

#ifdef CY_SYS_WINDOWS
//-------------------------------------------------------------------------------------
void TcpClient::_on_connection_timer(Looper::event_id_t id)
{
	if (get_connection_state() == Connection::kConnecting){
		//is still waiting connect? abort!
		_check_connect_status(true);
	}

	//remove the timer
	m_looper->disable_all(id);
	m_looper->delete_event(id);
	m_connection_timer_id = Looper::INVALID_EVENT_ID;
}
#endif

//-------------------------------------------------------------------------------------
Connection::State TcpClient::get_connection_state(void) const
{
	sys_api::auto_mutex lock(m_connection_lock);

	if (m_connection) return m_connection->get_state();
	else return Connection::kDisconnected;
}

//-------------------------------------------------------------------------------------
void TcpClient::_check_connect_status(bool abort)
{
	if (abort || socket_api::get_socket_error(m_socket) != 0) 
	{
		// abort
		m_looper->disable_all(m_socket_event_id);
		m_socket_event_id = Looper::INVALID_EVENT_ID;

		//logic callback
		if (m_listener) {
			uint32_t retry_sleep_ms = m_listener->on_connection_callback(this, false);

			//retry connection?
			if (retry_sleep_ms>0) {
				//remove the timer
				m_retry_timer_id = m_looper->register_timer_event(retry_sleep_ms, this, 
					std::bind(&TcpClient::_on_retry_connect_timer, this, std::placeholders::_1));
			}
		}

		//remove connection
		{
			sys_api::auto_mutex lock(m_connection_lock);

			delete m_connection;
			m_socket = 0;
			m_connection = 0;
		}
	}
	else
	{
		//connect success!
		
		//remove from event system, taked by Connection
		m_looper->disable_all(m_socket_event_id);
		m_looper->delete_event(m_socket_event_id);
		m_socket_event_id = Looper::INVALID_EVENT_ID;

		//established the connection
		m_connection->established();

		//logic callback
		if (m_listener) {
			m_listener->on_connection_callback(this, true);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::disconnect(void)
{
	m_connection->shutdown();
}

//-------------------------------------------------------------------------------------
void TcpClient::on_connection_event(Connection::Event event, Connection* conn)
{
	switch (event) {
	case Connection::kOnConnection:
		break;

	case Connection::kOnMessage:
		if (m_listener) {
			m_listener->on_message_callback(this, conn);
		}
		break;

	case Connection::kOnClose:
		if (m_listener) {
			m_listener->on_close_callback(this);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_retry_connect_timer(Looper::event_id_t id)
{
	//remove the timer
	m_looper->disable_all(id);
	m_looper->delete_event(id);
	m_retry_timer_id = Looper::INVALID_EVENT_ID;

	//connect again
	if (!connect(m_serverAddr, m_connect_timeout_ms))
	{
		//failed at once!, logic callback
		if (m_listener) {
			uint32_t retry_sleep_ms = m_listener->on_connection_callback(this, false);

			//retry connection?
			if (retry_sleep_ms>0) {
				m_retry_timer_id = m_looper->register_timer_event(retry_sleep_ms, this,
					std::bind(&TcpClient::_on_retry_connect_timer, this, std::placeholders::_1));

			}
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_socket_read_write(Looper::event_id_t /*id*/, socket_t /*fd*/, Looper::event_t /*event*/)
{
	if (get_connection_state() == Connection::kConnecting)
	{
		_check_connect_status(false);
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::send(const char* buf, size_t len)
{
	if (get_connection_state() == Connection::kConnected) {
		m_connection->send(buf, len);
	}
}


}

