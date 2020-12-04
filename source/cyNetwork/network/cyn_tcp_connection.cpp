/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include "cyn_tcp_connection.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
TcpConnection::TcpConnection(int32_t id, socket_t sfd, Looper* looper, Owner* owner)
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
	, m_on_message(nullptr)
	, m_on_send_complete(nullptr)
	, m_on_close(nullptr)
	, m_readbuf_minmax_size(kDefaultReadBufSize)
	, m_writebuf_minmax_size(kDefaultWriteBufSize)
	, m_read_statistics(nullptr)
	, m_write_statistics(nullptr)
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
		std::bind(&TcpConnection::_on_socket_read, this),
		std::bind(&TcpConnection::_on_socket_write, this)
	);
}

//-------------------------------------------------------------------------------------
TcpConnection::~TcpConnection()
{
	if (m_write_statistics) {
		delete m_write_statistics;
		m_write_statistics = nullptr;
	}

	if (m_read_statistics) {
		delete m_read_statistics;
		m_read_statistics = nullptr;
	}

	assert(get_state()==kDisconnected);
	assert(m_socket == INVALID_SOCKET);
	assert(m_event_id == Looper::INVALID_EVENT_ID);
	assert(m_write_buf_lock == nullptr);
}

//-------------------------------------------------------------------------------------
TcpConnection::State TcpConnection::get_state(void) const
{ 
	return m_state.load(); 
}

//-------------------------------------------------------------------------------------
void TcpConnection::send(const char* buf, size_t len)
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
bool TcpConnection::_is_writeBuf_empty(void) const
{
	sys_api::auto_mutex lock(m_write_buf_lock);
	return  m_write_buf.empty();
}

//-------------------------------------------------------------------------------------
void TcpConnection::_send(const char* buf, size_t len)
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
			if (m_write_statistics) {
				m_write_statistics->push(nwrote);
			}
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
void TcpConnection::shutdown(void)
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
void TcpConnection::_on_socket_read(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	ssize_t len = m_read_buf.read_socket(m_socket);
	m_readbuf_minmax_size.update(m_read_buf.size());
	if (len > 0)
	{
		if (m_read_statistics) {
			m_read_statistics->push(len);
		}
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
void TcpConnection::_on_socket_write(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);
	
	if (!(m_looper->is_write(m_event_id))) return;

	{
		sys_api::auto_mutex lock(m_write_buf_lock);
		m_writebuf_minmax_size.update(m_write_buf.size());
		if (!m_write_buf.empty()) {
			ssize_t len = m_write_buf.write_socket(m_socket);
			if (len <= 0) {
				//log error
				CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::get_lasterror());
			}
			if (m_write_statistics) {
				m_write_statistics->push(len);
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
void TcpConnection::_on_socket_close(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	assert(m_state == kConnected || m_state == kDisconnecting);

	TcpConnectionPtr thisPtr = shared_from_this();

	//disable all event
	m_state = kDisconnected;

	//delete looper event
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
void TcpConnection::_on_socket_error(void)
{
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void TcpConnection::set_name(const char* name)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	m_name = name;
}

//-------------------------------------------------------------------------------------
void TcpConnection::set_param(void* param)
{
	m_param = param;
}

//-------------------------------------------------------------------------------------
void TcpConnection::start_read_statistics(int32_t period_time)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	//already start
	if (m_read_statistics) return;
	m_read_statistics = new PeriodValue <size_t, true>(period_time);
}

//-------------------------------------------------------------------------------------
void TcpConnection::start_write_statistics(int32_t period_time)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	//already start
	if (m_write_statistics) return;
	m_write_statistics = new PeriodValue <size_t, true>(period_time);
}

//-------------------------------------------------------------------------------------
std::pair<size_t, int32_t> TcpConnection::get_read_statistics(void) const
{
	if (!m_read_statistics) return std::pair<size_t, int32_t>{0, 0};

	return m_read_statistics->sum_and_counts();
}

//-------------------------------------------------------------------------------------
std::pair<size_t, int32_t> TcpConnection::get_write_statistics(void) const
{
	if (!m_write_statistics) return std::pair<size_t, int32_t>{0, 0};

	return m_write_statistics->sum_and_counts();
}


}
