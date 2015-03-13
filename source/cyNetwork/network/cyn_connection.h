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

	/// get input stream buf (NOT thread safe, call it in work thread)
	RingBuf& get_input_buf(void) { return m_readBuf; }

	/// send message(thread safe)
	void send(const char* buf, size_t len);

public:
	void established(void);
	void shutdown(void);

	int32_t get_work_thread_index(void) const { return m_work_thread_index;  }
	Looper::event_id_t get_event_id(void) const { return m_event_id; }

private:
	Socket m_socket;
	State m_state;
	const Address m_local_addr;
	const Address m_peer_addr;
	Looper* m_looper;
	Looper::event_id_t m_event_id;
	TcpServer* m_server;
	const int32_t m_work_thread_index;

	enum { kDefaultReadBufSize=1024, kDefaultWriteBufSize=1024 };
	RingBuf m_readBuf;
	RingBuf m_writeBuf;

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

	//// on socket close
	void _on_socket_close(void);

	//// on socket error
	void _on_socket_error(void);

	/// send message (not thread safe, must int work thread)
	void _send(const char* buf, size_t len);

public:
	Connection(socket_t sfd, 
		const Address& peer_addr,
		TcpServer* server,
		int32_t work_thread_index,
		Looper* looper);
	~Connection();
};

}

#endif

