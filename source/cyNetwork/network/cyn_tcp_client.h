/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_TCP_CLIENT_H_
#define _CYCLONE_NETWORK_TCP_CLIENT_H_

#include <cy_core.h>

namespace cyclone
{

class TcpClient
{
public:
	enum States { kDisconnected, kConnecting, kConnected };

	//// connect to remote server(NOT thread safe)
	bool connect(const Address& addr, int32_t timeOutSeconds);
	//// disconnect(NOT thread safe)
	void disconnect(void);
	//// get server address
	Address get_server_address(void) const { return m_serverAddr; }

	/// send message(thread safe)
	void send(const char* buf, size_t len);

	/// get input stream buf (NOT thread safe, call it in work thread)
	RingBuf& get_input_buf(void) { return m_readBuf; }

public:
	typedef uint32_t(*on_connection_callback)(TcpClient* client, bool success);
	typedef void(*on_message_callback)(TcpClient* client);
	typedef void(*on_close_callback)(TcpClient* client);

	/// Set/Get connection callback. (NOT thread safe)
	void set_connection_callback(on_connection_callback cb) { m_connection_cb = cb; }
	on_connection_callback get_connection_callback(void) { return m_connection_cb; }

	/// Set message callback. (NOT thread safe)
	void set_message_callback(on_message_callback cb)  { m_message_cb = cb; }
	on_message_callback get_message_callback(void)  { return m_message_cb; }

	//// Set close callback. (NOT thread safe)
	void set_close_callback(on_close_callback cb)  { m_close_cb = cb; }
	on_close_callback get_close_callback(void) { return m_close_cb; }

private:
	States	m_state;

	socket_t m_socket;
	Looper::event_id_t m_socket_event_id;

	Address	 m_serverAddr;
	uint32_t m_connect_timeout_ms;
	Looper*	m_looper;

	on_connection_callback	m_connection_cb;
	on_message_callback		m_message_cb;
	on_close_callback		m_close_cb;

	enum { kDefaultReadBufSize = 1024, kDefaultWriteBufSize = 1024 };
	RingBuf m_readBuf;
	RingBuf m_writeBuf;

private:
	/// on read/write callback function
	static bool _on_socket_read_entry(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param){
		return ((TcpClient*)param)->_on_socket_read(id, fd, event);
	}
	bool _on_socket_read(Looper::event_id_t id, socket_t fd, Looper::event_t event);

	static bool _on_socket_write_entry(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param){
		return ((TcpClient*)param)->_on_socket_write(id, fd, event);
	}
	bool _on_socket_write(Looper::event_id_t id, socket_t fd, Looper::event_t event);

	static bool _on_connection_timer_entry(Looper::event_id_t id, void* param){
		return ((TcpClient*)param)->_on_connection_timer(id);
	}
	bool _on_connection_timer(Looper::event_id_t id);

	static bool _on_retry_connect_timer_entry(Looper::event_id_t id, void* param){
		return ((TcpClient*)param)->_on_retry_connect_timer(id);
	}
	bool _on_retry_connect_timer(Looper::event_id_t id);

	//// on socket close
	void _on_socket_close(void);

	//// on socket error
	void _on_socket_error(void);

private:
	void _check_connect_status(bool abort);
	void _send(const char* buf, size_t len);

public:
	TcpClient(Looper* looper);
	~TcpClient();
};

}
#endif

