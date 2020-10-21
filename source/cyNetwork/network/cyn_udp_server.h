/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_UDP_SERVER_H_
#define _CYCLONE_NETWORK_UDP_SERVER_H_

#include <cy_core.h>
#include <cy_event.h>
#include <network/cyn_address.h>

namespace cyclone
{
//pre-define 
class UdpServerWorkThread;

class UdpServer : noncopyable
{
public:
	typedef std::function<void(UdpServer* server, int32_t thread_index, Looper* looper)> WorkThreadStartCallback;
	typedef std::function<void(UdpServer* server, int32_t thread_index, int32_t socket_index, RingBuf& ring_buf, const Address& peer_address)> EventCallback;

	struct Listener {
		WorkThreadStartCallback on_work_thread_start;
		EventCallback on_message;
	};

	Listener m_listener;
public:
	/// bind a udp port, return thread index, return -1 means bind failed
	// NOT thread safe, and this function must be called before start the server
	int32_t bind(const Address& bind_addr);
	/// start the server
	/// (thread safe, but you wouldn't want call it again...)
	bool start(void);
	/// wait server to terminate(thread safe)
	void join(void);
	/// stop the server gracefully 
	//(NOT thread safe, you can't call this function in any work thread)
	void stop(void);

public:
	void sendto(int32_t thread_index, int32_t socket_index, const char* buf, size_t len, const Address& peer_address);

private:
	enum { MAX_WORK_THREAD_COUNTS = 32 };

	atomic_int32_t m_running;
	int32_t m_max_work_thread_counts;
	int32_t m_socket_counts;

	/// work thread pool
	typedef std::vector< UdpServerWorkThread* > ServerWorkThreadArray;
	ServerWorkThreadArray m_work_thread_pool;

public:
	UdpServer(int32_t max_thread_counts);
	~UdpServer();
};

}

#endif

