/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_UDP_CONNECTION_H_
#define _CYCLONE_NETWORK_UDP_CONNECTION_H_

#include <cy_core.h>
#include <network/cyn_address.h>
#include <event/cye_looper.h>

//pre-define
struct IKCPCB;

namespace cyclone
{
//pre-define
class UdpConnection;
typedef std::shared_ptr<UdpConnection> UdpConnectionPtr;

class UdpConnection : public std::enable_shared_from_this<UdpConnection>, noncopyable
{
public:
	typedef std::function<void(UdpConnectionPtr conn)> EventCallback;

	// connection status
	enum State { kConnected, kDisconnecting, kDisconnected };

public:
	/// init udp connection
	bool init(const Address& peer_address, const Address* local_address = nullptr);
	/// get id(thread safe)
	int32_t get_id(void) const { return m_id; }
	/// get peer address (thread safe)
	const Address& get_peer_addr(void) const { return m_peer_addr; }
	/// get input stream buf (NOT thread safe, call it in work thread)
	RingBuf& get_input_buf(void) { return m_read_buf; }
	/// send message(thread safe), 
	bool send(const char* buf, size_t len);
	/// shutdown the connection
	void shutdown(void);
	/// get looper
	Looper* get_looper(void) const { return m_looper; }
	/// get current state(thread safe)
	State get_state(void) const;

	/// set/get param(NOT thread safe)
	void set_param(void* param) { m_param = param; }
	void* get_param(void) { return m_param; }

	///set callback function
	void set_on_message(EventCallback callback) { m_on_message = callback; }
	void set_on_send_complete(EventCallback callback) { m_on_send_complete = callback; }
	void set_on_closing(EventCallback callback) { m_on_closing = callback; }
	void set_on_close(EventCallback callback) { m_on_close = callback; }

private:
	friend class UdpServerWorkThread;
	// send kcp data (work thread)
	bool _send(const char* buf, size_t len);
	//call by udp work thread, received udp socket message
	void _on_udp_input(const char* buf, int32_t len);
	//// on socket read event
	void _on_socket_read(void);
	//// on socket write event
	void _on_socket_write(void);
	//// is write buf empty(thread safe)
	bool _is_writeBuf_empty(void) const;

private:
	int32_t m_id;
	socket_t m_socket; //socket for send data
	std::atomic<State> m_state;
	Address m_peer_addr;
	Looper* m_looper;
	Looper::event_id_t m_event_id;
	RingBuf m_read_buf;
	void* m_param;

	RingBuf m_write_buf;
	sys_api::mutex_t m_write_buf_lock;	//for multi thread lock

	RingBuf m_udp_buf;

	EventCallback m_on_message;
	EventCallback m_on_send_complete;
	EventCallback m_on_closing;
	EventCallback m_on_close;

private:
	//kcp data
	enum { KCP_CONV = 0xC0DE };
	enum { KCP_TIMER_FREQ = 10 }; //every 10 millisecond
	enum { KCP_DISCONNECTING_TIME = 1000 }; //wait another 1000 millisecond after shutdown to make sure kcp message sent to peer

	IKCPCB* m_kcp;
	Looper::event_id_t m_update_timer_id;
	Looper::event_id_t m_closing_timer_id;
	int64_t m_start_time;

	// send udp data
	static int _kcp_udp_output(const char *buf, int len, IKCPCB *kcp, void *user);
	void _kcp_update(void);

public:
	UdpConnection(Looper* looper, int32_t id=0);
	~UdpConnection();
};

}

#endif

