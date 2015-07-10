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

class TcpClient : public Connection::Listener
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
	/// get current connection state(thread safe);
	Connection::State get_connection_state(void) const;

public:
public:
	class Listener
	{
	public:
		virtual uint32_t on_connection_callback(TcpClient* client, bool success) = 0;
		virtual void on_message_callback(TcpClient* client, Connection* conn) = 0;
		virtual void on_close_callback(TcpClient* client) = 0;
	};

	Listener* get_listener(void) { return m_listener; }

private:
	socket_t m_socket;
	Looper::event_id_t m_socket_event_id;

	Address	 m_serverAddr;
	uint32_t m_connect_timeout_ms;
	Looper*	m_looper;

	Listener* m_listener;

	void* m_param;

	Connection* m_connection;
	sys_api::mutex_t m_connection_lock;  //it's UGLY!

	Looper::event_id_t m_retry_timer_id;

public:
	//// called by connection(in work thread)
	virtual void on_connection_event(Connection::Event event, Connection* conn);

private:
	/// on read/write callback function
	static bool _on_socket_read_write_entry(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param){
		return ((TcpClient*)param)->_on_socket_read_write(id, fd, event);
	}
	bool _on_socket_read_write(Looper::event_id_t id, socket_t fd, Looper::event_t event);

#ifdef CY_SYS_WINDOWS
	static bool _on_connection_timer_entry(Looper::event_id_t id, void* param){
		return ((TcpClient*)param)->_on_connection_timer(id);
	}
	bool _on_connection_timer(Looper::event_id_t id);
	Looper::event_id_t m_connection_timer_id;
#endif

	static bool _on_retry_connect_timer_entry(Looper::event_id_t id, void* param){
		return ((TcpClient*)param)->_on_retry_connect_timer(id);
	}
	bool _on_retry_connect_timer(Looper::event_id_t id);

private:
	void _check_connect_status(bool abort);

public:
	TcpClient(Looper* looper, Listener* listener, void* param);
	~TcpClient();
};

}
#endif

