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
		const Address& peer_addr,
		TcpServer* server,
		int32_t work_thread_index,
		Looper* looper)
	: m_socket(sfd)
	, m_peer_addr(peer_addr)
	, m_local_addr(sfd) //create local address
	, m_state(kConnecting)
	, m_event_id(0)
	, m_server(server)
	, m_looper(looper)
	, m_readBuf(kDefaultReadBufSize)
	, m_writeBuf(kDefaultWriteBufSize)
	, m_work_thread_index(work_thread_index)
{
	m_socket.set_keep_alive(true);
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
	size_t nwrote = 0;

	if (m_state != kConnected)
	{
		//TODO: log error, give up send message
		return;
	}

	//nothing in write buf, send it diretly
	if (!(m_looper->is_write(m_event_id)) && m_writeBuf.empty())
	{
		nwrote = socket_api::write(m_socket.get_fd(), buf, (ssize_t)len);
		if (nwrote >= 0)
		{
			remaining = (ssize_t)(len - nwrote);
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
			if (errno != EWOULDBLOCK)
			{
				//TODO: log error;
				if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
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
void Connection::_on_socket_read(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());

	ssize_t len = m_readBuf.read_socket((int)m_socket.get_fd());

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
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_write(void)
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
			//TODO: log error
		}
	}
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_close(void)
{
	assert(thread_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);

	//disable all event
	m_state = kDisconnected;
	m_looper->disable_all(m_event_id);

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
	//TODO: log some thing...
	_on_socket_close();
}

}
