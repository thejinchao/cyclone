/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_TCP_CLIENT_H_
#define _CYCLONE_NETWORK_TCP_CLIENT_H_

#include <cy_core.h>

namespace cyclone
{

//pre-define 
class Connection;

class TcpClient : public Connection::Listener, noncopyable
{
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
	const void* get_callback_param(void) const { return m_param; }
	/// get current connection state(thread safe);
	Connection::State get_connection_state(void) const;

public:
	class Listener
	{
	public:
		virtual uint32_t on_connected(TcpClient* client, Connection* conn, bool success) = 0;
		virtual void     on_message(TcpClient* client, Connection* conn) = 0;
		virtual void     on_close(TcpClient* client) = 0;
	};

	Listener* get_listener(void) { return m_listener; }

private:
	socket_t m_socket;
	Looper::event_id_t m_socket_event_id;
	Looper::event_id_t m_retry_timer_id;

	Address	 m_serverAddr;
	Looper*	m_looper;
	Listener* m_listener;
	void* m_param;
	Connection* m_connection;
	sys_api::mutex_t m_connection_lock;
	RingBuf m_sendCache;

public:
	//// called by connection(in work thread)
	virtual void on_connection_event(Connection::Event event, Connection* conn);

private:
	/// on read/write callback function
	void _on_socket_read_write(void);
	void _on_retry_connect_timer(Looper::event_id_t id);

private:
	void _on_connect_status_changed(bool timeout);
	void _abort_connect(uint32_t retry_sleep_ms);

public:
	TcpClient(Looper* looper, Listener* listener, void* param);
	virtual ~TcpClient();
};

}
#endif

