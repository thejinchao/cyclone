/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cy_core.h>
#include <cy_event.h>

namespace cyclone
{

class TcpServerMasterThread : noncopyable
{
public:
	//accept thread command
	enum { 
		kBindSocketCmdID = 1, 
		kStopBindSocketCmdID,
		kShutdownCmdID,

		kCustomCmdID_Begin = TcpServer::kCustomMasterThreadCmdID_Begin,
	};

	struct BindSocketCmd
	{
		enum { ID = kBindSocketCmdID };
		socket_t sfd;
		Address address;
	};

	struct StopBindSocketCmd
	{
		enum { ID = kStopBindSocketCmdID };
		Address address;
	};

	struct ShutdownCmd
	{
		enum { ID = kShutdownCmdID };
	};

private:
	//call by TcpServer Only
	friend class TcpServer;

	// send message to this work thread (thread safe)
	void send_thread_message(uint16_t id, uint16_t size, const char* message);
	void send_thread_message(const Packet* message);
	void send_thread_message(const Packet** message, int32_t counts);

	// bind new address
	bool bind(const Address& addr, bool enable_reuse_port);
	// stop listen binded address(thread safe)
	bool stop_bind(const Address& addr);
	// remove binded address(NOT thread safe, must call in master thread or before master thread running)
	bool remove_bind_address(const Address& addr);

	// start master thread
	bool start(void);
	// join work thread(thread safe)
	void join(void);

private:
	TcpServer*	m_server;
	WorkThread	m_master_thread;

	typedef std::list< std::tuple<socket_t, Looper::event_id_t, Address>> SocketList;
	SocketList m_acceptor_sockets;
	sys_api::mutex_t m_acceptor_sockets_mutex;

private:
	/// master thread function start
	bool _on_thread_start(void);

	/// master thread message
	void _on_thread_message(Packet*);

	/// on accept callback function
	void _on_accept_event(Looper::event_id_t id, socket_t fd, Looper::event_t event);

public:
	TcpServerMasterThread(TcpServer* server);
	~TcpServerMasterThread();
};

}
