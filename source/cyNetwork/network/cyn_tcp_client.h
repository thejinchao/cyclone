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

class TcpClient
{
public:
	//// connect to remote server(NOT thread safe)
	bool connect(const Address& addr, int32_t timeOutSeconds);
	//// disconnect(NOT thread safe)
	void disconnect(void);
	//// get server address
	Address get_server_address(void) const { return m_serverAddr; }

	/// send message(thread safe)
	void send(const char* buf, size_t len);

	/// get callback param(thread safe)
	const void* get_callback_param(void) const { return m_param; }

public:
	typedef uint32_t(*on_connection_callback)(TcpClient* client, bool success);
	typedef void(*on_message_callback)(TcpClient* client, Connection* conn);
	typedef void(*on_close_callback)(TcpClient* client);

	/// Set/Get connection callback. (NOT thread safe)
	void set_connection_callback(on_connection_callback cb) { m_connection_cb = cb; }

	/// Set message callback. (NOT thread safe)
	void set_message_callback(on_message_callback cb)  { m_message_cb = cb; }

	//// Set close callback. (NOT thread safe)
	void set_close_callback(on_close_callback cb)  { m_close_cb = cb; }

private:
	socket_t m_socket;
	Looper::event_id_t m_socket_event_id;

	Address	 m_serverAddr;
	uint32_t m_connect_timeout_ms;
	Looper*	m_looper;

	on_connection_callback	m_connection_cb;
	on_message_callback		m_message_cb;
	on_close_callback		m_close_cb;

	Connection* m_connection;
	void* m_param;

public:
	//// called by connection(in work thread)
	static void _on_connection_event_entry(Connection::Event event, Connection* conn, void* param){
		((TcpClient*)param)->_on_connection_event(event, conn);
	}
	void _on_connection_event(Connection::Event event, Connection* conn);

private:
	/// on read/write callback function
	static bool _on_socket_read_write_entry(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param){
		return ((TcpClient*)param)->_on_socket_read_write(id, fd, event);
	}
	bool _on_socket_read_write(Looper::event_id_t id, socket_t fd, Looper::event_t event);

	static bool _on_connection_timer_entry(Looper::event_id_t id, void* param){
		return ((TcpClient*)param)->_on_connection_timer(id);
	}
	bool _on_connection_timer(Looper::event_id_t id);

#ifdef CY_SYS_WINDOWS
	static bool _on_retry_connect_timer_entry(Looper::event_id_t id, void* param){
		return ((TcpClient*)param)->_on_retry_connect_timer(id);
	}
	bool _on_retry_connect_timer(Looper::event_id_t id);
#endif

private:
	void _check_connect_status(bool abort);

public:
	TcpClient(Looper* looper, void* param);
	~TcpClient();
};

}
#endif

