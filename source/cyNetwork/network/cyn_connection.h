/*
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
	RingBuf& get_input_buf(void) { return m_readBuf; }

	/// send message(thread safe)
	void send(const char* buf, size_t len);

	/// get native socket
	socket_t get_socket(void) { return m_socket; }

	/// set/get connection debug name(NOT thread safe)
	void set_name(const char* name);
	const char* get_name(void) const { return m_name.c_str(); }

	/// get param
	void* get_param(void) { return m_param; }

	///set callbackfunction
	void setOnMessageFunction(EventCallback callback) { m_onMessage = callback; }
	void setOnCloseFunction(EventCallback callback) { m_onClose = callback; }

	/// debug
	void debug(DebugInterface* debuger);

	/// shutdown the connection
	void shutdown(void);

private:
	int32_t m_id;
	socket_t m_socket;
	State m_state;
	Address m_local_addr;
	Address m_peer_addr;
	Looper* m_looper;
	Looper::event_id_t m_event_id;
	void* m_param;

	enum { kDefaultReadBufSize=1024, kDefaultWriteBufSize=1024 };
	
	RingBuf m_readBuf;

	RingBuf m_writeBuf;
	sys_api::mutex_t m_writeBufLock;	//for multithread lock

	EventCallback m_onMessage;
	EventCallback m_onClose;

	std::string m_name;

	size_t m_max_sendbuf_len;

	DebugInterface* m_debuger;

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

	//// clean all debug value
	void _del_debug_value(void);

public:
	Connection(int32_t id, socket_t sfd, Looper* looper, void* param);
	~Connection();
};

}

#endif

