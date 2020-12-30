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
		kShutdownCmdID = 1, kStopListenCmdID, 

		kCustomCmdID_Begin = TcpServer::kCustomMasterThreadCmdID_Begin,
	};

	struct ShutdownCmd
	{
		enum { ID = kShutdownCmdID };
	};

	struct StopListenCmd
	{
		enum { ID = kStopListenCmdID };
		size_t index;
	};

public: //call by TcpServer Only
	//// send message to this work thread (thread safe)
	void send_thread_message(uint16_t id, uint16_t size, const char* message);
	void send_thread_message(const Packet* message);
	void send_thread_message(const Packet** message, int32_t counts);

	// add a binded socket(called by TcpServer only!)
	bool bind_socket(const Address& bind_addr, bool enable_reuse_port);
	// start master thread
	bool start(void);
	/// get bind address(called by TcpServer only!)
	Address get_bind_address(size_t index);
	/// get bind socket size
	size_t get_bind_socket_size(void) const {
		return m_acceptor_sockets.size();
	}

	//// join work thread(thread safe)
	void join(void);

private:
	TcpServer*	m_server;
	WorkThread	m_master_thread;

	typedef std::vector< std::tuple<socket_t, Looper::event_id_t> > SocketVector;
	SocketVector m_acceptor_sockets;

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
