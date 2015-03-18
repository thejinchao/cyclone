/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include "cyn_connection.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Connection::Connection(socket_t sfd,
		TcpServer* server,
		int32_t work_thread_index,
		Looper* looper)
	: m_socket(sfd)
	, m_state(kConnecting)
	, m_local_addr(false, sfd) //create local address
	, m_peer_addr(true, sfd) //create peer address
	, m_looper(looper)
	, m_event_id(0)
	, m_server(server)
	, m_work_thread_index(work_thread_index)
	, m_readBuf(kDefaultReadBufSize)
	, m_writeBuf(kDefaultWriteBufSize)
{
	//set socket to non-block and close-onexec
	socket_api::set_nonblock(sfd, true);
	socket_api::set_close_onexec(sfd, true);
	//set other socket option
	m_socket.set_keep_alive(true);
	m_socket.set_linger(false, 0);
}

//-------------------------------------------------------------------------------------
Connection::~Connection()
{

}

//-------------------------------------------------------------------------------------
Connection::State Connection::get_state(void) const
{ 
	//TODO: atom operation
	return m_state; 
}

//-------------------------------------------------------------------------------------
void Connection::established(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());

	m_state = kConnected;

	//register socket event
	m_event_id = m_looper->register_event(m_socket.get_fd(),
		Looper::kRead,			//care read event only
		this,
		_on_socket_read_entry,
		_on_socket_write_entry);

	//logic callback
	if (m_server && m_server->get_connection_callback()) {
		m_server->get_connection_callback()(m_server, this);
	}
}

//-------------------------------------------------------------------------------------
void Connection::send(const char* buf, size_t len)
{
	if (thread_api::thread_get_current_id() == m_looper->get_thread_id())
	{
		_send(buf, len);
	}
	else
	{
		//TODO: write to output buf
	}
}

//-------------------------------------------------------------------------------------
void Connection::_send(const char* buf, size_t len)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected);

	bool faultError = false;
	size_t remaining = len;
	ssize_t nwrote = 0;

	if (m_state != kConnected)
	{
		//TODO: log error, give up send message
		CY_LOG(L_ERROR, "send message state error, state=%d", m_state);
		return;
	}

	//nothing in write buf, send it diretly
	if (!(m_looper->is_write(m_event_id)) && m_writeBuf.empty())
	{
		nwrote = socket_api::write(m_socket.get_fd(), buf, len);
		if (nwrote >= 0)
		{
			remaining = len - (size_t)nwrote;
			if (remaining == 0)
			{
				//TODO: notify logic layer
				//if (m_server && m_server->get_write_complete_callback()) {
				//	m_server->get_write_complete_callback()(m_server, this);
				//}
			}
		}
		else
		{
			nwrote = 0;
			int err = socket_api::get_lasterror();

#ifdef CY_SYS_WINDOWS
			if(err != WSAEWOULDBLOCK)
#else
			if(err != EWOULDBLOCK)
#endif
			{
				CY_LOG(L_ERROR, "socket send error, err=%d", err);

#ifdef CY_SYS_WINDOWS
				if (err == WSAESHUTDOWN || err == WSAENETRESET)
#else
				if (err == EPIPE || err == ECONNRESET)
#endif
				{
					faultError = true;
				}
			}
		}
	}

	if (!faultError && remaining > 0)
	{
		//write to write buffer
		m_writeBuf.memcpy_into(buf + nwrote, remaining);

		//enable write event, wait socket ready
		m_looper->enable_write(m_event_id);
	}
}

//-------------------------------------------------------------------------------------
void Connection::shutdown(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state==kDisconnecting);

	//set the state to disconnecting...
	m_state = kDisconnecting;

	//something still working? wait 
	if (m_looper->is_write(m_event_id) && !m_writeBuf.empty()) return;
	
	//ok, we can close the socket now
	socket_api::shutdown(m_socket.get_fd());
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
bool Connection::_on_socket_read(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());

	ssize_t len = m_readBuf.read_socket(m_socket.get_fd());

	if (len > 0)
	{
		//notify logic layer...
		if (m_server && m_server->get_message_callback()) {
			m_server->get_message_callback()(m_server, this);
		}
	}
	else if (len == 0)
	{
		//the connection was closed by peer, close now!
		_on_socket_close();
	}
	else
	{
		//error!
		_on_socket_error();
	}
	return false;
}

//-------------------------------------------------------------------------------------
bool Connection::_on_socket_write(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);
	
	if (m_looper->is_write(m_event_id))
	{
		assert(!m_writeBuf.empty());

		ssize_t len = m_writeBuf.write_socket(m_socket.get_fd());
		if (len > 0) {
			if (m_writeBuf.empty()) {
				m_looper->disable_write(m_event_id);

				//TODO: notify logic layer
				//if (m_server && m_server->get_write_complete_callback()) {
				//	m_server->get_write_complete_callback()(m_server, this);
				//}

				//disconnecting? this is the last message send to client, we can shut it down again
				if (m_state == kDisconnecting) {
					shutdown();
				}
			}
		}
		else
		{
			//log error
			CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::get_lasterror());
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_close(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);

	//disable all event
	m_state = kDisconnected;
	m_looper->disable_all(m_event_id);
	m_writeBuf.reset();

	//logic callback
	if (m_server && m_server->get_close_callback()) {
		m_server->get_close_callback()(m_server, this);
	}

	//shutdown this connection, this is the last time you see me.
	if (m_server) 
		m_server->shutdown_connection(this);
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_error(void)
{
	_on_socket_close();
}

}
