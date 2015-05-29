/*
Copyright(C) thecodeway.com
*/
#include <cy_event.h>
#include <cy_network.h>

#include "cyn_tcp_client.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpClient::TcpClient(Looper* looper)
	: m_state(kDisconnected)
	, m_socket(0)
	, m_socket_event_id(0)
	, m_connect_timeout_ms(0)
	, m_looper(looper)
	, m_connection_cb(0)
	, m_message_cb(0)
	, m_close_cb(0)
{
	//looper muste be setted
	assert(looper);
}

//-------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{

}

//-------------------------------------------------------------------------------------
bool TcpClient::connect(const Address& addr, int32_t timeOutSeconds)
{
	//check status
	if (m_state != kDisconnected) return false;

	//create socket
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

	//set event callback
	m_socket_event_id = m_looper->register_event(sfd, Looper::kRead | Looper::kWrite, this,
		_on_socket_read_entry, _on_socket_write_entry);

	//start connect to server
	if (!socket_api::connect(sfd, addr.get_sockaddr_in()))
	{
		CY_LOG(L_ERROR, "connect to server error");
		return false;
	}

	m_state = kConnecting;
	m_socket = sfd;
	m_serverAddr = addr;
	m_connect_timeout_ms = timeOutSeconds;

#ifdef CY_SYS_WINDOWS
	//for select mode in windows, the non-block fd of connect socket wouldn't be readable or writeable if connection failed
	//but... it's ugly...
	m_looper->register_timer_event(timeOutSeconds, this, _on_connection_timer_entry);
#endif
	return true;
}

//-------------------------------------------------------------------------------------
bool TcpClient::_on_connection_timer(Looper::event_id_t id)
{
	if (m_state == kConnecting){
		//is still waiting connect? abort!
		_check_connect_status(true);
	}

	//remove the timer
	m_looper->disable_all(id);
	m_looper->delete_event(id);
	return false;
}

//-------------------------------------------------------------------------------------
void TcpClient::_check_connect_status(bool abort)
{
	if (abort || socket_api::get_socket_error(m_socket) != 0) 
	{
		// abort
		m_state = kDisconnected;
		m_looper->disable_all(m_socket_event_id);
		socket_api::close_socket(m_socket);
		m_socket = 0;
		m_socket_event_id = 0;

		//logic callback
		if (m_connection_cb) {
			uint32_t retry_sleep_ms = m_connection_cb(this, false);

			//retry connection?
			if (retry_sleep_ms>0) {
				m_looper->register_timer_event(retry_sleep_ms, this, _on_retry_connect_timer_entry);
			}
		}
	}
	else
	{
		//connect success!
		m_state = kConnected;

		//logic callback
		if (m_connection_cb) {
			m_connection_cb(this, true);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::disconnect(void)
{
	//TODO: disconnect...
}

//-------------------------------------------------------------------------------------
bool TcpClient::_on_retry_connect_timer(Looper::event_id_t id)
{
	if (!connect(m_serverAddr, m_connect_timeout_ms))
	{
		//logic callback
		if (m_connection_cb) {
			uint32_t retry_sleep_ms = m_connection_cb(this, false);

			//retry connection?
			if (retry_sleep_ms>0) {
				m_looper->register_timer_event(retry_sleep_ms, this, _on_retry_connect_timer_entry);
			}
		}
	}

	//remove the timer
	m_looper->disable_all(id);
	m_looper->delete_event(id);

	return false;
}

//-------------------------------------------------------------------------------------
bool TcpClient::_on_socket_read(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	(void)id;
	(void)fd;
	(void)event;

	if (m_state == kConnecting)
	{
		_check_connect_status(false);
	}
	else if (m_state == kConnected) 
	{
		//TODO: read packet...
	}
	return false;
}

//-------------------------------------------------------------------------------------
bool TcpClient::_on_socket_write(Looper::event_id_t id, socket_t fd, Looper::event_t event)
{
	(void)fd;
	(void)event;

	if (m_state == kConnecting)
	{
		_check_connect_status(false);

		//disable write event
		m_looper->disable_write(id);
	}
	else if (m_state == kConnected)
	{
		//TODO: write packet...
	}
	return false;
}


}

