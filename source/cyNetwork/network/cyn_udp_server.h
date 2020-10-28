/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_UDP_SERVER_H_
#define _CYCLONE_NETWORK_UDP_SERVER_H_

#include <cy_core.h>
#include <cy_event.h>
#include <network/cyn_address.h>
#include <network/cyn_udp_connection.h>

namespace cyclone
{
//pre-define 
class UdpServerMasterThread;
class UdpServerWorkThread;

class UdpServer : noncopyable
{
public:
	enum { MAX_UDP_READ_SIZE = 32 * 1024 }; //32KB
	enum { MAX_WORK_THREAD_COUNTS = 32 };

	typedef std::function<void(UdpServer* server, Looper* looper)> MasterThreadStartCallback;
	typedef std::function<void(UdpServer* server, int32_t thread_index, Looper* looper)> WorkThreadStartCallback;
	typedef std::function<void(UdpServer* server, int32_t thread_index, UdpConnectionPtr conn)> EventCallback;

	struct Listener {
		MasterThreadStartCallback on_master_thread_start;
		WorkThreadStartCallback on_work_thread_start;

		EventCallback on_connected;
		EventCallback on_message;
		EventCallback on_close;
	};

	Listener m_listener;
public:
	/// bind a udp port, return is bind success
	// NOT thread safe, and this function must be called before start the server
	bool bind(const Address& bind_addr);
	/// start the server
	/// (thread safe, but you wouldn't want call it again...)
	bool start(int32_t work_thread_counts);
	/// wait server to terminate(thread safe)
	void join(void);
	/// stop the server gracefully 
	//(NOT thread safe, you can't call this function in any work thread)
	void stop(void);
	/// shutdown one of connection(thread safe)
	void shutdown_connection(UdpConnectionPtr conn);

private:
	// master thread
	UdpServerMasterThread* m_master_thread;

	/// work thread pool
	typedef std::vector< UdpServerWorkThread* > ServerWorkThreadArray;
	ServerWorkThreadArray m_work_thread_pool;
	int32_t m_workthread_counts;

	atomic_int32_t m_running;
	atomic_int32_t m_shutdown_ing;

	enum { kStartConnectionID = 1 };
	atomic_int32_t m_next_connection_id;

private:
	// called by work thread
	friend class UdpServerWorkThread;

	void _on_socket_connected(int32_t work_thread_index, UdpConnectionPtr conn);
	void _on_socket_message(int32_t work_thread_index, UdpConnectionPtr conn);
	void _on_socket_close(int32_t work_thread_index, UdpConnectionPtr conn);

	// get next connection id
	int32_t _get_next_connection_id(void) { return m_next_connection_id++; }

private:
	// called by udp master thread
	void _on_udp_message_received(const char* buf, int32_t len, const sockaddr_in& peer_address, const sockaddr_in& local_address);

public:
	UdpServer();
	~UdpServer();
};

}

#endif

