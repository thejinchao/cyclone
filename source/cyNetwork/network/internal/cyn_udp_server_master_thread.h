/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_UDP_SERVER_MASTER_THREAD_H_
#define _CYCLONE_NETWORK_UDP_SERVER_MASTER_THREAD_H_

#include <cy_core.h>
#include <cy_event.h>
#include <network/cyn_address.h>

namespace cyclone
{
//pre-define
class UdpServer;

class UdpServerMasterThread : noncopyable
{
public:
	enum { kShutdownCmdID = 1 };

	typedef std::function<void(const char* buf, int32_t len, const sockaddr_in& peer_address, const sockaddr_in& local_address)> OnUdpMessageReceived;

public://called by UdpServer only!
	// add a binded socket
	bool bind_socket(const Address& bind_addr);
	// start master thread
	bool start(void);
	//// join work thread(thread safe)
	void join(void);

	//// send message to this work thread (thread safe)
	void send_thread_message(uint16_t id, uint16_t size, const char* message);
	void send_thread_message(const Packet* message);
	void send_thread_message(const Packet** message, int32_t counts);

private:
	/// work thread function start
	bool _on_thread_start(void);
	/// work thread message
	void _on_workthread_message(Packet*);
	/// socket read
	void _on_read_event(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param);

private:
	WorkThread*	m_thread;
	UdpServer* m_server;

	struct ReceiveSocket
	{
		int32_t				index;
		socket_t			sfd;
		Looper::event_id_t	event_id;
		Address				bind_addr;
	};
	typedef std::vector< ReceiveSocket > SocketVector;
	SocketVector m_receive_sockets;

	char* m_read_buf;
	OnUdpMessageReceived m_udp_message_callback;
public:
	UdpServerMasterThread(UdpServer* server, OnUdpMessageReceived udp_message_callback);
	~UdpServerMasterThread();
};

}

#endif
