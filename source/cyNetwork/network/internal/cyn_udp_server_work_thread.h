/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_UDP_SERVER_WORK_THREAD_H_
#define _CYCLONE_NETWORK_UDP_SERVER_WORK_THREAD_H_

#include <cy_core.h>
#include <cy_event.h>
#include <network/cyn_address.h>

namespace cyclone
{
//pre-define
class WorkThread;
class UdpServer;

class UdpServerWorkThread : noncopyable
{
public:
	enum { kShutdownCmdID=1 };

	struct ShutdownCmd
	{
		enum { ID = kShutdownCmdID };
	};

public:
	// add a binded socket(called by TcpServer only!)
	bool bind_socket(const Address& bind_addr);
	// start master thread
	bool start(void);
	//// join work thread(thread safe)
	void join(void);
	//// is current thread in work thread (thread safe)
	bool is_in_workthread(void) const;
	//
	void sendto(int32_t socket_index, const char* buf, size_t len, const Address& peer_address);

public: // call by UdpServer Only
	//// send message to this work thread (thread safe)
	void send_thread_message(uint16_t id, uint16_t size, const char* message);

private:
	int32_t m_index;
	WorkThread*	m_work_thread;
	UdpServer* m_server;

	typedef std::vector< std::tuple<socket_t, int32_t> > SocketVector;
	SocketVector m_sockets;

	RingBuf m_read_buf;
private:
	/// work thread function start
	bool _on_thread_start(void);
	/// work thread message
	void _on_workthread_message(Packet*);
	/// socket read
	void _on_read_event(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param);

public:
	UdpServerWorkThread(int32_t index, UdpServer* server);
	~UdpServerWorkThread();
};

}

#endif
