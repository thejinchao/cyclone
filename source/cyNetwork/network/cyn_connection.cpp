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
Connection::Connection(int32_t id, socket_t sfd, Looper* looper, Owner* owner)
	: m_id(id)
	, m_socket(sfd)
	, m_state(kConnected)
	, m_looper(looper)
	, m_event_id(Looper::INVALID_EVENT_ID)
	, m_owner(owner)
	, m_param(nullptr)
	, m_read_buf(kDefaultReadBufSize)
	, m_write_buf(kDefaultWriteBufSize)
	, m_write_buf_lock(nullptr)
	, m_max_sendbuf_len(0)
	, m_debuger(nullptr)
{
	//set socket to non-block and close-onexec
	socket_api::set_nonblock(sfd, true);
	socket_api::set_close_onexec(sfd, true);
	//set other socket option
	socket_api::set_keep_alive(sfd, true);
	socket_api::set_linger(sfd, false, 0);
	//set socket no-delay
	socket_api::set_nodelay(sfd, true);

	//init write buf lock
	m_write_buf_lock = sys_api::mutex_create();

	m_local_addr = Address(false, m_socket); //create local address
	m_peer_addr = Address(true, m_socket); //create peer address

	//set default debug name
	char temp[MAX_PATH] = { 0 };
	std::snprintf(temp, MAX_PATH, "connection_%d", id);
	m_name = temp;

	//register socket event
	m_event_id = m_looper->register_event(m_socket,
		Looper::kRead,			//care read event only
		this,
		std::bind(&Connection::_on_socket_read, this),
		std::bind(&Connection::_on_socket_write, this)
	);
}

//-------------------------------------------------------------------------------------
Connection::~Connection()
{
	_del_debug_value();

	assert(get_state()==kDisconnected);
	assert(m_socket == INVALID_SOCKET);
	assert(m_event_id == Looper::INVALID_EVENT_ID);
	assert(m_write_buf_lock == nullptr);
}

//-------------------------------------------------------------------------------------
Connection::State Connection::get_state(void) const
{ 
	return m_state.load(); 
}

//-------------------------------------------------------------------------------------
void Connection::send(const char* buf, size_t len)
{
	if (buf == nullptr || len == 0) return;

	if (sys_api::thread_get_current_id() == m_looper->get_thread_id())
	{
		_send(buf, len);
	}
	else
	{
		if (get_state() != kConnected)
		{
			//log error, give up send message
			CY_LOG(L_ERROR, "send message state error, state=%d", get_state());
			return;
		}

		//write to output buf
		sys_api::auto_mutex lock(m_write_buf_lock);

		//write to write buffer
		m_write_buf.memcpy_into(buf, len);

		//enable write event, wait socket ready
		m_looper->enable_write(m_event_id);

	}
}

//-------------------------------------------------------------------------------------
bool Connection::_is_writeBuf_empty(void) const
{
	sys_api::auto_mutex lock(m_write_buf_lock);
	return  m_write_buf.empty();
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
		CY_LOG(L_ERROR, "send message state error, state=%d", get_state());
		return;
	}

	//nothing in write buf, send it directly
	if (!(m_looper->is_write(m_event_id)) && _is_writeBuf_empty())
	{
		nwrote = socket_api::write(m_socket, buf, len);
		if (nwrote >= 0)
		{
			remaining = len - (size_t)nwrote;
		}
		else
		{
			nwrote = 0;
			int err = socket_api::get_lasterror();

			if(!socket_api::is_lasterror_WOULDBLOCK())
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

	//shutdown if socket work with fault
	if (faultError) {
		m_looper->disable_all(m_event_id);
		shutdown();

		return;
	}

	if (remaining > 0) {
		sys_api::auto_mutex lock(m_write_buf_lock);
		//write to write buffer
		m_write_buf.memcpy_into(buf + nwrote, remaining);
	}

	//enable write event, wait socket ready
	m_looper->enable_write(m_event_id);
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
	socket_api::shutdown(m_socket);
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_read(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	ssize_t len = m_read_buf.read_socket(m_socket);

	if (len > 0)
	{
		//notify logic layer...
		if (m_on_message) {
			m_on_message(shared_from_this());
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
	
	if (!(m_looper->is_write(m_event_id))) return;

	{
		sys_api::auto_mutex lock(m_write_buf_lock);

		if (!m_write_buf.empty()) {
			if (m_write_buf.size() > m_max_sendbuf_len) {
				m_max_sendbuf_len = m_write_buf.size();
			}

			ssize_t len = m_write_buf.write_socket(m_socket);
			if (len <= 0) {
				//log error
				CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::get_lasterror());
			}
		}

		//still remain some data, wait next socket write time
		if (!m_write_buf.empty()) {
			return;
		}

		//no longer need care write-able event
		m_looper->disable_write(m_event_id);
	}

	//write complete
	if (m_on_send_complete) {
		m_on_send_complete(this->shared_from_this());
	}

	//disconnecting? this is the last message send to client, we can shut it down again
	if (m_state == kDisconnecting) {
		shutdown();
	}
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_close(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);

	ConnectionPtr thisPtr = shared_from_this();

	//disable all event
	m_state = kDisconnected;

	//delete looper event
	m_looper->disable_all(m_event_id);
	m_looper->delete_event(m_event_id);
	m_event_id = Looper::INVALID_EVENT_ID;
	
	//logic callback
	if (m_on_close) {
		m_on_close(thisPtr);
	}

	//reset read/write buf
	m_write_buf.reset();
	m_read_buf.reset();

	//close socket
	socket_api::close_socket(m_socket);
	m_socket = INVALID_SOCKET;

	//destroy write buf lock
	sys_api::mutex_destroy(m_write_buf_lock);
	m_write_buf_lock = nullptr;
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_error(void)
{
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void Connection::set_name(const char* name)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	m_name = name;
}

//-------------------------------------------------------------------------------------
void Connection::set_param(void* param)
{
	m_param = param;
}

//-------------------------------------------------------------------------------------
void Connection::debug(DebugInterface* debuger)
{
	if (!debuger || !(debuger->isEnable())) return;

	m_debuger = debuger;
	char key_temp[MAX_PATH] = { 0 };

	std::snprintf(key_temp, MAX_PATH, "Connection:%s:readbuf_capcity", m_name.c_str());
	debuger->updateDebugValue(key_temp, (int32_t)m_read_buf.capacity());

	std::snprintf(key_temp, MAX_PATH, "Connection:%s:writebuf_capcity", m_name.c_str());
	debuger->updateDebugValue(key_temp, (int32_t)m_write_buf.capacity());

	std::snprintf(key_temp, MAX_PATH, "Connection:%s:max_sendbuf_len", m_name.c_str());
	debuger->updateDebugValue(key_temp, (int32_t)m_max_sendbuf_len);
}

//-------------------------------------------------------------------------------------
void Connection::_del_debug_value(void)
{
	if (!m_debuger || !(m_debuger->isEnable())) return;

	char key_name[MAX_PATH] = { 0 };

	std::snprintf(key_name, MAX_PATH, "Connection:%s:readbuf_capcity", m_name.c_str());
	m_debuger->delDebugValue(key_name);

	std::snprintf(key_name, MAX_PATH, "Connection:%s:writebuf_capcity", m_name.c_str());
	m_debuger->delDebugValue(key_name);

	std::snprintf(key_name, MAX_PATH, "Connection:%s:max_sendbuf_len", m_name.c_str());
	m_debuger->delDebugValue(key_name);
}

}
