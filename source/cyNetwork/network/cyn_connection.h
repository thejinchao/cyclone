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
	typedef void(*event_callback)(Event event, Connection* conn);

public:
	//connection state
	//                         established()                         shutdown()
	//  O ----> kConnecting ---------------------> kConnected ----------------------> kDisconnecting---
	//               |                                                                                 |
	//               |      _on_socket_close()                         _on_socket_close()              |
	//                ---------------------------> kDisconnected <-------------------------------------
	//
	enum State { kDisconnected, kConnecting, kConnected, kDisconnecting };

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

	/// thread safe
	void* get_param0(void) const { return m_param0; }

	/// set/get another param (NOT thread safe)(UGLY...)
	void set_param1(void* param);
	void* get_param1(void) const { return m_param1; }

public:
	void established(void);
	void shutdown(void);

	Looper::event_id_t get_event_id(void) const { return m_event_id; }

private:
	Socket m_socket;
	State m_state;
	Address m_local_addr;
	Address m_peer_addr;
	Looper* m_looper;
	Looper::event_id_t m_event_id;

	enum { kDefaultReadBufSize=1024, kDefaultWriteBufSize=1024 };
	
	RingBuf m_readBuf;

	RingBuf m_writeBuf;
	thread_api::mutex_t m_writeBufLock;	//for multithread lock

	event_callback m_callback;

	void* m_param0;
	void* m_param1;

private:
	//// on socket read event
	static bool _on_socket_read_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		return ((Connection*)param)->_on_socket_read();
	}
	bool _on_socket_read(void);

	//// on socket read event
	static bool _on_socket_write_entry(Looper::event_id_t, socket_t, Looper::event_t, void* param){
		return ((Connection*)param)->_on_socket_write();
	}
	bool _on_socket_write(void);

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
	Connection(socket_t sfd, Looper* looper, event_callback cb, void* param0, void* param1);
	~Connection();
};

}

#endif

