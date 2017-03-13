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
Connection::Connection(int32_t id, socket_t sfd, Looper* looper, Listener* listener)
	: m_id(id)
	, m_socket(sfd)
	, m_state(kConnecting)
	, m_looper(looper)
	, m_event_id(0)
	, m_readBuf(kDefaultReadBufSize)
	, m_writeBuf(kDefaultWriteBufSize)
	, m_listener(listener)
	, m_proxy(0)
	, m_max_sendbuf_len(0)
{
	//set socket to non-block and close-onexec
	socket_api::set_nonblock(sfd, true);
	socket_api::set_close_onexec(sfd, true);
	//set other socket option
	m_socket.set_keep_alive(true);
	m_socket.set_linger(false, 0);

	//init write buf lock
	m_writeBufLock = sys_api::mutex_create();

	//set default debug name
	snprintf(m_name, MAX_PATH, "connection_%d", id);
}

//-------------------------------------------------------------------------------------
Connection::~Connection()
{
	sys_api::mutex_destroy(m_writeBufLock);
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
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	m_state = kConnected;

	m_local_addr = Address(false, m_socket.get_fd()); //create local address
	m_peer_addr = Address(true, m_socket.get_fd()); //create peer address

	//register socket event
	m_event_id = m_looper->register_event(m_socket.get_fd(),
		Looper::kRead,			//care read event only
		this,
		std::bind(&Connection::_on_socket_read, this),
		std::bind(&Connection::_on_socket_write,this)
	);

	//logic callback
	if (m_listener) {
		m_listener->on_connection_event(kOnConnection, this);
	}
}

//-------------------------------------------------------------------------------------
void Connection::send(const char* buf, size_t len)
{
	if (sys_api::thread_get_current_id() == m_looper->get_thread_id())
	{
		_send(buf, len);
	}
	else
	{
		if (get_state() != kConnected)
		{
			//log error, give up send message
			CY_LOG(L_ERROR, "send message state error, state=%d", m_state);
			return;
		}

		//write to output buf
		sys_api::auto_mutex lock(m_writeBufLock);

		//write to write buffer
		m_writeBuf.memcpy_into(buf, len);

		//enable write event, wait socket ready
		m_looper->enable_write(m_event_id);

	}
}

//-------------------------------------------------------------------------------------
bool Connection::_is_writeBuf_empty(void) const
{
	sys_api::auto_mutex lock(m_writeBufLock);
	return  m_writeBuf.empty();
}

//-------------------------------------------------------------------------------------
void Connection::_send(const char* buf, size_t len)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	bool faultError = false;
	size_t remaining = len;
	ssize_t nwrote = 0;

	if (m_state != kConnected)
	{
		//log error, give up send message
		CY_LOG(L_ERROR, "send message state error, state=%d", m_state);
		return;
	}

	//nothing in write buf, send it diretly
	if (!(m_looper->is_write(m_event_id)) && _is_writeBuf_empty())
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
		sys_api::auto_mutex lock(m_writeBufLock);

		//write to write buffer
		m_writeBuf.memcpy_into(buf + nwrote, remaining);

		//enable write event, wait socket ready
		m_looper->enable_write(m_event_id);
	}

	//shutdown if socket work with fault
	if (faultError) {
		m_looper->disable_all(m_event_id);
		shutdown();
	}
}

//-------------------------------------------------------------------------------------
void Connection::shutdown(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state==kDisconnecting);

	//set the state to disconnecting...
	m_state = kDisconnecting;

	//something still working? wait 
	if (m_looper->is_write(m_event_id) && !_is_writeBuf_empty()) return;
	
	//ok, we can close the socket now
	socket_api::shutdown(m_socket.get_fd());
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_read(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	ssize_t len = m_readBuf.read_socket(m_socket.get_fd());

	if (len > 0)
	{
		//notify logic layer...
		if (m_listener) {
			m_listener->on_connection_event(kOnMessage, this);
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
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);
	
	if (m_looper->is_write(m_event_id))
	{
		sys_api::auto_mutex lock(m_writeBufLock);
		assert(!m_writeBuf.empty());

		if (m_writeBuf.size() > m_max_sendbuf_len) {
			m_max_sendbuf_len = m_writeBuf.size();
		}

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
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_close(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);

	//disable all event
	m_state = kDisconnected;
	m_looper->disable_all(m_event_id);
	m_writeBuf.reset();

	//logic callback
	if (m_listener) {
		m_listener->on_connection_event(kOnClose, this);
	}
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_error(void)
{
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void Connection::set_proxy(void* proxy)
{
	m_proxy = proxy;
}

//-------------------------------------------------------------------------------------
void* Connection::get_proxy(void)
{
	return m_proxy;
}

//-------------------------------------------------------------------------------------
void Connection::set_name(const char* name)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	strncpy(m_name, name, MAX_PATH);
}

//-------------------------------------------------------------------------------------
void Connection::debug(DebugInterface* debuger)
{
	if (!debuger || !(debuger->is_enable())) return;

	char key_temp[MAX_PATH] = { 0 };

	snprintf(key_temp, MAX_PATH, "Connection:%s:readbuf_capcity", m_name);
	debuger->set_debug_value(key_temp, (int32_t)m_readBuf.capacity());

	snprintf(key_temp, MAX_PATH, "Connection:%s:writebuf_capcity", m_name);
	debuger->set_debug_value(key_temp, (int32_t)m_writeBuf.capacity());

	snprintf(key_temp, MAX_PATH, "Connection:%s:max_sendbuf_len", m_name);
	debuger->set_debug_value(key_temp, (int32_t)m_max_sendbuf_len);
}

//-------------------------------------------------------------------------------------
void Connection::del_debug_value(DebugInterface* debuger)
{
	if (!debuger || !(debuger->is_enable())) return;

	char key_name[MAX_PATH] = { 0 };

	snprintf(key_name, MAX_PATH, "Connection:%s:readbuf_capcity", m_name);
	debuger->del_debug_value(key_name);

	snprintf(key_name, MAX_PATH, "Connection:%s:writebuf_capcity", m_name);
	debuger->del_debug_value(key_name);

	snprintf(key_name, MAX_PATH, "Connection:%s:max_sendbuf_len", m_name);
	debuger->del_debug_value(key_name);
}

}
