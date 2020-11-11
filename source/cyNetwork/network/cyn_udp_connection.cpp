#include "cyn_udp_connection.h"
#include <event/cye_looper.h>
#include <network/cyn_udp_server.h>
#include <network/internal/cyn_ikcp.h>

namespace cyclone
{
//-------------------------------------------------------------------------------------
UdpConnection::UdpConnection(Looper* looper, int32_t id)
	: m_id(id)
	, m_socket(INVALID_SOCKET)
	, m_state(kDisconnected)
	, m_looper(looper)
	, m_event_id(Looper::INVALID_EVENT_ID)
	, m_param(nullptr)
	, m_write_buf_lock(nullptr)
	, m_on_message(nullptr)
	, m_on_send_complete(nullptr)
	, m_on_closing(nullptr)
	, m_on_close(nullptr)
	, m_kcp(nullptr)
	, m_update_timer_id(Looper::INVALID_EVENT_ID)
	, m_closing_timer_id(Looper::INVALID_EVENT_ID)
	, m_start_time(0)
{
	//create write buf lock
	m_write_buf_lock = sys_api::mutex_create();
	//get current time
	m_start_time = sys_api::utc_time_now();
}

//-------------------------------------------------------------------------------------
UdpConnection::~UdpConnection()
{
	assert(get_state() == kDisconnected);

	if (m_write_buf_lock) {
		sys_api::mutex_destroy(m_write_buf_lock);
		m_write_buf_lock = nullptr;
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

	//socket ok, set state to Connected
	m_socket = sfd;
	m_peer_addr = peer_address;
	m_state = kConnected;

	//watch writable event
	m_event_id = m_looper->register_event(m_socket,
		Looper::kRead,
		this,
		std::bind(&UdpConnection::_on_socket_read, this),
		std::bind(&UdpConnection::_on_socket_write, this)
	);

	//init kcp
	m_kcp = ikcp_create(KCP_CONV, this);
	m_kcp->output = _kcp_udp_output;

	//set kcp no delay mode
	ikcp_nodelay(m_kcp, 1, 10, 2, 1);
	ikcp_setmtu(m_kcp, UdpServer::MAX_UDP_PACKET_SIZE);

	//create kcp update timer
	m_update_timer_id = m_looper->register_timer_event(KCP_TIMER_FREQ, nullptr, [this](Looper::event_id_t, void*) {
		this->_kcp_update();
	});
	return true;
}

//-------------------------------------------------------------------------------------
UdpConnection::State UdpConnection::get_state(void) const
{
	return m_state.load();
}

//-------------------------------------------------------------------------------------
int UdpConnection::_kcp_udp_output(const char *buf, int len, IKCPCB *kcp, void *user)
{
	UdpConnection* conn = (UdpConnection*)user;

	assert(sys_api::thread_get_current_id() == conn->m_looper->get_thread_id());
	if (len <= 0) return 0;

	int32_t nwrote = (int32_t)socket_api::write(conn->m_socket, buf, (size_t)len);
	if (nwrote <= 0)
	{
		//log error
		CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::get_lasterror());
	}
	
	//CY_LOG(L_DEBUG, ">>>> write to udp socket %d", nwrote);

	//enable write event, wait socket ready
	conn->m_looper->enable_write(conn->m_event_id);
	return nwrote;
}

//-------------------------------------------------------------------------------------
void UdpConnection::_on_udp_input(const char* buf, int32_t len)
{
	//input UDP data to kcp 
	if (buf != nullptr && len > 0) {
		//CY_LOG(L_DEBUG, "<<<< read from udp socket %d", len);

		int32_t ret = ikcp_input(m_kcp, buf, len);
		if (ret < 0) {
			CY_LOG(L_ERROR, "call ikcp_input error, ret=%d", ret);
			return;
		}
	}

	//is still working?
	if (get_state()!=kConnected) return;

	//try read data from kcp
	int32_t kcp_size = ikcp_peeksize(m_kcp);
	if (kcp_size <= 0) return;

	//resize kcp buff
	m_udp_buf.reset();
	if (m_udp_buf.capacity() < (size_t)kcp_size) {
		m_udp_buf.resize((size_t)kcp_size);
	}

	//call kcp receive
	int32_t ret = ikcp_recv(m_kcp, (char*)m_udp_buf.normalize(), kcp_size);
	if (ret > 0) {
		//notify logic layer...
		m_read_buf.memcpy_into(m_udp_buf.normalize(), (size_t)ret);
		if (m_on_message) {
			m_on_message(shared_from_this());
		}
	}
	else {
		CY_LOG(L_TRACE, "ikcp_recv, ret=%d", ret);
	}
}

//-------------------------------------------------------------------------------------
bool UdpConnection::send(const char* buf, int32_t len)
{
	//check size
	if (buf == nullptr || len <= 0) return true;
	if (get_state() != kConnected) return false;

	if (len > UdpServer::MAX_KCP_SEND_SIZE) {
		CY_LOG(L_ERROR, "Can't send kcp package large than max kcp packet size %d>%d", len, UdpServer::MAX_KCP_SEND_SIZE);
		return false;
	}

	// is in working thread?
	if (sys_api::thread_get_current_id() == m_looper->get_thread_id()) {
		return _send(buf, len);
	}
	else
	{
		//write to output buf
		sys_api::auto_mutex lock(m_write_buf_lock);
		//write to write buffer
		m_write_buf.memcpy_into(buf, (size_t)len);
		//enable write event, wait socket ready
		m_looper->enable_write(m_event_id);
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool UdpConnection::_send(const char* buf, int32_t len)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	if (buf == nullptr || len <= 0) return true;

	//nothing in write buf, send it directly
	if (_is_writeBuf_empty())
	{
		int32_t kcp_ret = ikcp_send((ikcpcb*)m_kcp, buf, len);
		if (kcp_ret < 0) {
			CY_LOG(L_ERROR, "call ikcp_send failed, ret=%d", kcp_ret);
			return false;
		}
		//flush
		ikcp_flush(m_kcp);
	}
	else {
		//write to write buffer wait next time socket is ready
		sys_api::auto_mutex lock(m_write_buf_lock);
		m_write_buf.memcpy_into(buf, (size_t)len);
	}

	//enable write event, wait socket ready
	m_looper->enable_write(m_event_id);
	return true;
}

//-------------------------------------------------------------------------------------
bool UdpConnection::_is_writeBuf_empty(void) const
{
	sys_api::auto_mutex lock(m_write_buf_lock);
	return m_write_buf.empty();
}

//-------------------------------------------------------------------------------------
void UdpConnection::_on_socket_read(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	m_udp_buf.reset();
	int32_t udp_len = (int32_t)socket_api::read(m_socket, m_udp_buf.normalize(), m_udp_buf.capacity());
	if (udp_len <= 0) return;

	_on_udp_input((const char*)m_udp_buf.normalize(), udp_len);
}

//-------------------------------------------------------------------------------------
void UdpConnection::_on_socket_write(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());

	if (!(m_looper->is_write(m_event_id))) return;

	{
		sys_api::auto_mutex lock(m_write_buf_lock);
		if (!m_write_buf.empty()) {
			//send kcp data
			int32_t kcp_ret = ikcp_send(m_kcp, (const char*)m_write_buf.normalize(), (int32_t)m_write_buf.size());
			if (kcp_ret < 0) {
				CY_LOG(L_ERROR, "call ikcp_send error, ret=%d", kcp_ret);
				return;
			}
			ikcp_flush(m_kcp);
			m_write_buf.reset();
		}
	}

	int32_t wait_snd = ikcp_waitsnd(m_kcp);
	int32_t snd_wnd = (int32_t)(m_kcp->snd_wnd);

	//no longer need care write-able event
	if (wait_snd <= 0) {
		m_looper->disable_write(m_event_id);
	}

	//kcp send queue is available
	if (get_state() == kConnected) {
		if (m_on_send_complete && wait_snd < snd_wnd * 2) {
			m_on_send_complete(this->shared_from_this());
		}
	}
}

//-------------------------------------------------------------------------------------
void UdpConnection::_kcp_update(void)
{
	//call kcp update function
	uint32_t now = (uint32_t)((sys_api::utc_time_now() - m_start_time) / 1000ll);
	ikcp_update(m_kcp, now);

	if (get_state() == kDisconnecting && ikcp_waitsnd(m_kcp) <=0 ) {
		shutdown(); //all kcp packet sent, shutdown again
		return;
	}

	//try read kcp data force
	_on_udp_input(nullptr, 0);
}

//-------------------------------------------------------------------------------------
void UdpConnection::shutdown(void)
{
	assert(sys_api::thread_get_current_id() == m_looper->get_thread_id());
	if (get_state() == kDisconnected ) return; //closed already?

	//close the socket now
	UdpConnectionPtr thisPtr = shared_from_this();

	//start closing timer
	if (get_state()==kConnected) {
		CY_LOG(L_DEBUG, "[CONN]shutdown UDP connection on Connected state.");
		//flush kcp data to peer
		ikcp_flush(m_kcp);

		//enter Disconnecting state, make sure all kcp message send to peer 
		m_state = kDisconnecting;

		//logic callback
		if (m_on_closing) {
			m_on_closing(thisPtr);
		}
		return;
	}

	CY_LOG(L_DEBUG, "[CONN]shutdown UDP connection on Disconnecting state.");
	//set state to Disconnected
	m_state = kDisconnected;

	if (m_event_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_event_id);
		m_looper->delete_event(m_event_id);
		m_event_id = Looper::INVALID_EVENT_ID;
	}

	if (m_update_timer_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_update_timer_id);
		m_looper->delete_event(m_update_timer_id);
		m_update_timer_id = Looper::INVALID_EVENT_ID;
	}

	if (m_closing_timer_id != Looper::INVALID_EVENT_ID) {
		m_looper->disable_all(m_closing_timer_id);
		m_looper->delete_event(m_closing_timer_id);
		m_closing_timer_id = Looper::INVALID_EVENT_ID;
	}

	if (m_socket != INVALID_SOCKET) {
		socket_api::close_socket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	//logic callback
	if (m_on_close) {
		m_on_close(thisPtr);
	}

	//reset read/write buf
	m_write_buf.reset();
	m_read_buf.reset();

	if (m_kcp) {
		ikcp_release((ikcpcb*)m_kcp);
		m_kcp = nullptr;
	}
}

}
