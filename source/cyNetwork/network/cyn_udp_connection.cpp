#include "cyn_udp_connection.h"
#include <event/cye_looper.h>
#include <network/cyn_udp_server.h>

namespace cyclone
{

//-------------------------------------------------------------------------------------
UdpConnection::UdpConnection(Looper* looper, int32_t id)
	: m_id(id)
	, m_socket(INVALID_SOCKET)
	, m_looper(looper)
	, m_event_id(Looper::INVALID_EVENT_ID)
	, m_write_buf_lock(nullptr)
	, m_udp_buf(nullptr)
	, m_on_message(nullptr)
	, m_on_send_complete(nullptr)
	, m_on_close(nullptr)
	, m_closed(false)
{
	//create write buf lock
	m_write_buf_lock = sys_api::mutex_create();
	//prepare read buf
	m_udp_buf = new char[UdpServer::MAX_UDP_READ_SIZE];
}

//-------------------------------------------------------------------------------------
UdpConnection::~UdpConnection()
{
	if (m_udp_buf) {
		delete[] m_udp_buf;
		m_udp_buf = nullptr;
	}

	if (m_event_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_event_id);
		m_looper->delete_event(m_event_id);
	}

	if (m_write_buf_lock) {
		sys_api::mutex_destroy(m_write_buf_lock);
	}

	if (m_socket != INVALID_SOCKET) {
		socket_api::close_socket(m_socket);
	}
}

//-------------------------------------------------------------------------------------
bool UdpConnection::init(const Address& peer_address, const Address* local_address)
{
	//create a socket to send data
	socket_t sfd = socket_api::create_socket(true);
	if (sfd == INVALID_SOCKET) {
		CY_LOG(L_ERROR, "create udp socket error");
		return false;
	}

	//set socket to non-block mode
	if (!socket_api::set_nonblock(sfd, true)) {
		//the process should be stop		
		CY_LOG(L_ERROR, "set socket to non block mode error");
		socket_api::close_socket(sfd);
		return false;
	}

	//bind to local address
	if (local_address) {
		//enable reuse addr
		if (!socket_api::set_reuse_addr(sfd, true)) {
			CY_LOG(L_ERROR, "set reuse address failed");
			socket_api::close_socket(sfd);
			return false;
		}

		if (!socket_api::bind(sfd, local_address->get_sockaddr_in())) {
			CY_LOG(L_ERROR, "bind to address %s:%d failed", local_address->get_ip(), local_address->get_port());
			socket_api::close_socket(sfd);
			return false;
		}
	}

	//bind to remote address
	if (!socket_api::connect(sfd, peer_address.get_sockaddr_in())) {
		CY_LOG(L_ERROR, "bind udp socket to peer address %s:%d failed", peer_address.get_ip(), peer_address.get_port());
		socket_api::close_socket(sfd);
		return false;
	}

	//socket ok
	m_socket = sfd;
	m_peer_addr = peer_address;
	m_closed = false;

	//watch writable event
	m_event_id = m_looper->register_event(m_socket,
		Looper::kRead,
		this,
		std::bind(&UdpConnection::_on_socket_read, this),
		std::bind(&UdpConnection::_on_socket_write, this)
	);

	return true;
}

//-------------------------------------------------------------------------------------
void UdpConnection::_on_udp_input(const char* buf, int32_t len)
{
	m_read_buf.memcpy_into(buf, len);

	//notify logic layer...
	if (m_on_message) {
		m_on_message(shared_from_this());
	}
}

//-------------------------------------------------------------------------------------
void UdpConnection::send(const char* buf, int32_t len)
{
	if (buf == nullptr || len == 0) return;
	if (sys_api::thread_get_current_id() == m_looper->get_thread_id())
	{
		_send(buf, len);
	}
	else
	{
		//write to output buf
		sys_api::auto_mutex lock(m_write_buf_lock);
		//write to write buffer
		m_write_buf.memcpy_into(buf, len);
		//enable write event, wait socket ready
		m_looper->enable_write(m_event_id);
	}
}

//-------------------------------------------------------------------------------------
bool UdpConnection::_is_writeBuf_empty(void) const
{
	sys_api::auto_mutex lock(m_write_buf_lock);
	return m_write_buf.empty();
}

//-------------------------------------------------------------------------------------
void UdpConnection::_send(const char* buf, int32_t len)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	if (len <= 0) return;

	ssize_t nwrote = 0;
	size_t remaining = len;

	//nothing in write buf, send it directly
	if (!(m_looper->is_write(m_event_id)) && _is_writeBuf_empty())
	{
		nwrote = socket_api::write(m_socket, buf, len);
		if (nwrote > 0)
		{
			remaining = len - (size_t)nwrote;
		}
		else
		{
			//log error
			CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::get_lasterror());
		}
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
void UdpConnection::_on_socket_read(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	int32_t udp_len = (int32_t)socket_api::read(m_socket, m_udp_buf, UdpServer::MAX_UDP_READ_SIZE);
	if (udp_len <= 0) return;

	_on_udp_input(m_udp_buf, udp_len);
}

//-------------------------------------------------------------------------------------
void UdpConnection::_on_socket_write(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	if (!(m_looper->is_write(m_event_id))) return;

	{
		sys_api::auto_mutex lock(m_write_buf_lock);
		if (!m_write_buf.empty()) 
		{
			ssize_t nwrote = socket_api::write(m_socket, (const char*)m_write_buf.normalize(), m_write_buf.size());
			//ssize_t nwrote = socket_api::sendto(m_socket, (const char*)m_write_buf.normalize(), m_write_buf.size(), m_peer_addr.get_sockaddr_in());
			if (nwrote > 0)
			{
				m_write_buf.discard(nwrote);
			} 
			else 
			{
				//log error
				CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::get_lasterror());
			}
		}

		//still remain some data, wait next socket write time
		if (!m_write_buf.empty()) 
		{
			return;
		}

		//no longer need care write-able event
		m_looper->disable_write(m_event_id);
	}
}

//-------------------------------------------------------------------------------------
void UdpConnection::shutdown(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	if (m_closed) return; //closed already?

	//close the socket now
	UdpConnectionPtr thisPtr = shared_from_this();
	m_closed = true;

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

}
