/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_TCP_CLIENT_H_
#define _CYCLONE_NETWORK_TCP_CLIENT_H_

#include <cy_core.h>
#include "cyn_connection.h"

namespace cyclone
{

//pre-define 
class TcpClient;
typedef std::shared_ptr<TcpClient> TcpClientPtr;
    
class TcpClient : public std::enable_shared_from_this<TcpClient>, noncopyable, public Connection::Owner
{
public:
	typedef std::function<uint32_t(TcpClientPtr client, ConnectionPtr conn, bool success)> ConnectedCallback;
	typedef std::function<void(TcpClientPtr client, ConnectionPtr conn)> MessageCallback;
	typedef std::function<void(TcpClientPtr client, ConnectionPtr conn)> CloseCallback;

	struct Listener {
		ConnectedCallback onConnected;
		MessageCallback onMessage;
		CloseCallback onClose;
	};
	Listener m_listener;

public:
	//// connect to remote server(NOT thread safe)
	bool connect(const Address& addr);
	//// disconnect(NOT thread safe)
	void disconnect(void);
	//// get server address
	Address get_server_address(void) const { return m_serverAddr; }
	/// send message(thread safe after connected, NOT thread safe when connecting)
	void send(const char* buf, size_t len);
	/// get callback param(thread safe)
	const void* get_param(void) const { return m_param; }
	/// get current connection state(thread safe);
	Connection::State get_connection_state(void) const;
	/// Connection Owner type
	virtual OWNER_TYPE get_connection_owner_type(void) const { return kClient; }

private:
	int m_id;
	socket_t m_socket;
	Looper::event_id_t m_socket_event_id;
	Looper::event_id_t m_retry_timer_id;

	Address	 m_serverAddr;
	Looper*	m_looper;
	void* m_param;
	ConnectionPtr m_connection;
	sys_api::mutex_t m_connection_lock;
	RingBuf m_sendCache;

private:
	/// on read/write callback function
	void _on_socket_read_write(void);
	void _on_retry_connect_timer(Looper::event_id_t id);

private:
	void _on_connect_status_changed(bool timeout);
	void _abort_connect(uint32_t retry_sleep_ms);

public:
	TcpClient(Looper* looper, void* param, int id=0);
	virtual ~TcpClient();
};

}
#endif

