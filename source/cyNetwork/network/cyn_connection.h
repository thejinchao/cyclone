/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_CONNECTION_H_
#define _CYCLONE_NETWORK_CONNECTION_H_

#include <cyclone_config.h>
#include <network/cyn_address.h>

namespace cyclone
{

class Connection
{
public:
	//connection  event
	enum Event {
		kOnConnection,
		kOnMessage,
		kOnClose
	};

	class Listener
	{
	public:
		virtual void on_connection_event(Event event, Connection* conn) = 0;
	};

public:
	//connection state
	//                         established()                         shutdown()
	//  O ----> kConnecting ---------------------> kConnected ----------------------> kDisconnecting---
	//               |                                                                                 |
	//               |      _on_socket_close()                         _on_socket_close()              |
	//                ---------------------------> kDisconnected <-------------------------------------
	//
	enum State { kDisconnected, kConnecting, kConnected, kDisconnecting };

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

	/// set/get proxy param (thread safe)
	void set_proxy(void* param);
	void* get_proxy(void);

	/// set/get connection debug name(NOT thread safe)
	void set_name(const char* name);
	const char* get_name(void) const { return m_name; }

	/// get listener
	Listener* get_listener(void) { return m_listener; }

	/// debug
	void debug(DebugInterface* debuger);
	void del_debug_value(DebugInterface* debuger);

public:
	void established(void);
	void shutdown(void);

	Looper::event_id_t get_event_id(void) const { return m_event_id; }

private:
	int32_t m_id;
	Socket m_socket;
	State m_state;
	Address m_local_addr;
	Address m_peer_addr;
	Looper* m_looper;
	Looper::event_id_t m_event_id;

	enum { kDefaultReadBufSize=1024, kDefaultWriteBufSize=1024 };
	
	RingBuf m_readBuf;

	RingBuf m_writeBuf;
	sys_api::mutex_t m_writeBufLock;	//for multithread lock

	Listener* m_listener;
	atomic_ptr_t m_proxy;

	char m_name[MAX_PATH];

	size_t m_max_sendbuf_len;

private:
	//// on socket read event
	static void _on_socket_read_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		((Connection*)param)->_on_socket_read();
	}
	void _on_socket_read(void);

	//// on socket read event
	static void _on_socket_write_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		((Connection*)param)->_on_socket_write();
	}
	void _on_socket_write(void);

private:
	//// on socket close
	void _on_socket_close(void);

	//// on socket error
	void _on_socket_error(void);

	/// send message (not thread safe, must int work thread)
	void _send(const char* buf, size_t len);

	//// is write buf empty(thread safe)
	bool _is_writeBuf_empty(void) const;

public:
	Connection(int32_t id, socket_t sfd, Looper* looper, Listener* listener);
	~Connection();
};

}

#endif

