/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_UDP_SERVER_WORK_THREAD_H_
#define _CYCLONE_NETWORK_UDP_SERVER_WORK_THREAD_H_

#include <cy_core.h>
#include <cy_event.h>
#include <network/cyn_address.h>
#include <network/cyn_udp_connection.h>

namespace cyclone
{
//pre-define
class WorkThread;
class UdpServer;

class UdpServerWorkThread : noncopyable
{
public:
	enum {
		kReceiveUdpMessage = 1,
		kCloseConnectionCmd = 2,
		kShutdownCmdID = 3,
	};

	struct ReceiveUdpMessage
	{
		enum { ID = kReceiveUdpMessage };
		sockaddr_in local_address;
		sockaddr_in peer_address;
	};

	struct CloseConnectionCmd
	{
		enum { ID = kCloseConnectionCmd	};
		sockaddr_in peer_address;
		int32_t shutdown_ing; //this work thread will be shutdown
	};

public: // call by UdpServer Only
	// start master thread
	bool start(void);
	//// join work thread(thread safe)
	void join(void);
	//// is current thread in work thread (thread safe)
	bool is_in_workthread(void) const;
	//// send message to this work thread (thread safe)
	void send_thread_message(uint16_t id, uint16_t size_part1, const char* msg_part1, uint16_t size_part2=0, const char* msg_part2=nullptr);

private:
	UdpServer* m_server;
	const int32_t m_index;
	WorkThread*	m_thread;

	//Connection Map
	typedef std::unordered_map< Address, UdpConnectionPtr > ConnectionMap;
	ConnectionMap m_connections;

private:
	/// work thread function start
	bool _on_thread_start(void);
	/// work thread message
	void _on_workthread_message(Packet*);
	/// on receive udp message from master thread
	void _on_receive_udp_message(const sockaddr_in& local_addr, const sockaddr_in& peer_addr, const char* buf, int32_t len);

public:
	UdpServerWorkThread(UdpServer* server, int32_t index);
	~UdpServerWorkThread();
};

}

#endif
