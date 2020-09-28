﻿/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_CONNECTION_H_
#define _CYCLONE_NETWORK_CONNECTION_H_

#include <cyclone_config.h>
#include <network/cyn_address.h>

namespace cyclone
{

class Connection;
typedef std::shared_ptr<Connection> ConnectionPtr;

class Connection : public std::enable_shared_from_this<Connection>, noncopyable
{
public:
	typedef std::function<void(ConnectionPtr conn)> EventCallback;
	class Owner {
	public:
		enum OWNER_TYPE { kServer=0, kClient };
		virtual OWNER_TYPE get_connection_owner_type(void) const = 0;
	};
public:
	//connection state(kConnecting should not appear in Connection)
	//                                                                 shutdown()
	//  O -------------------> kConnected ---------------------------------------------------------------> kDisconnecting
	//                             |                                                                              |
	//                             |      _on_socket_close()                         _on_socket_close()           |
	//                              ---------------------------> kDisconnected <----------------------------------
	//
	enum State { kConnecting, kConnected, kDisconnecting, kDisconnected };

	/// get id(thread safe)
	int32_t get_id(void) const { return m_id; }

	/// get current state(thread safe)
	State get_state(void) const;

	/// get peer address (thread safe)
	const Address& get_peer_addr(void) const { return m_peer_addr; }

	/// get local address (thread safe)
	const Address& get_local_addr(void) const { return m_local_addr; }

	/// get input stream buf (NOT thread safe, call it in work thread)
	RingBuf& get_input_buf(void) { return m_read_buf; }

	/// send message(thread safe)
	void send(const char* buf, size_t len);

	/// get native socket
	socket_t get_socket(void) { return m_socket; }

	/// set/get connection debug name(NOT thread safe)
	void set_name(const char* name);
	const char* get_name(void) const { return m_name.c_str(); }

	/// get owner
	Owner* get_owner(void) { return m_owner; }

	/// set/get param(NOT thread safe)
	void set_param(void* param);
	void* get_param(void) { return m_param; }

	/// get looper
	Looper* get_looper(void) const { return m_looper; }

	///set callback function
	void set_on_message(EventCallback callback) { m_on_message = callback; }
	void set_on_send_complete(EventCallback callback) { m_on_send_complete = callback; }
	void set_on_close(EventCallback callback) { m_on_close = callback; }

	/// shutdown the connection
	void shutdown(void);

private:
	int32_t m_id;
	socket_t m_socket;
	std::atomic<State> m_state;
	Address m_local_addr;
	Address m_peer_addr;
	Looper* m_looper;
	Looper::event_id_t m_event_id;
	Owner* m_owner;
	void* m_param;

	enum { kDefaultReadBufSize=1024, kDefaultWriteBufSize=1024 };
	
	RingBuf m_read_buf;

	RingBuf m_write_buf;
	sys_api::mutex_t m_write_buf_lock;	//for multi thread lock

	EventCallback m_on_message;
	EventCallback m_on_send_complete;
	EventCallback m_on_close;

	std::string m_name;

	size_t m_max_sendbuf_len;

private:
	//// on socket read event
	void _on_socket_read(void);

	//// on socket read event
	void _on_socket_write(void);

	//// on socket close
	void _on_socket_close(void);

	//// on socket error
	void _on_socket_error(void);

	/// send message (not thread safe, must int work thread)
	void _send(const char* buf, size_t len);

	//// is write buf empty(thread safe)
	bool _is_writeBuf_empty(void) const;

public:
	Connection(int32_t id, socket_t sfd, Looper* looper, Owner* owner);
	~Connection();
};

}

#endif

