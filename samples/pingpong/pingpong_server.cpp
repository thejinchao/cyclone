﻿#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>
#include <iostream>

#include "pingpong_common.h"

using namespace cyclone;
using namespace std::placeholders;

#define MAX_DATA_SIZE (1024*4)

enum { OPT_SERVER_PORT, OPT_KCP, OPT_VERBOSE, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_SERVER_PORT, "-p",	SO_REQ_SEP }, // "-p SERVER_PORT"
	{ OPT_KCP,  "-k",	  SO_NONE },	// "-k"
	{ OPT_VERBOSE,  "-v",	  SO_NONE },	// "-v"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
template<typename ServerType, typename ConnectionType>
class PingPongServer
{
public:
	void start_and_join(void)
	{
		assert(m_status.load() == PS_Closed);

		if (!m_server.bind(Address((uint16_t)m_listen_port, false))) {
			CY_LOG(L_ERROR, "Bind port %d error!", m_listen_port);
			return;
		}

		m_server.m_listener.on_connected = std::bind(&PingPongServer::on_client_connected, this, _3);
		m_server.m_listener.on_message = std::bind(&PingPongServer::on_client_message, this, _3);
		m_server.m_listener.on_close = std::bind(&PingPongServer::on_client_close, this, _3);

		if (!(m_server.start(1))) {
			CY_LOG(L_ERROR, "start server error!");
			return;
		}
		m_status = PS_Listening;

		m_server.join();
	}
protected:
	//-------------------------------------------------------------------------------------
	virtual void on_client_connected(ConnectionType conn)
	{
		if (!atomic_compare_exchange(m_status, PS_Listening, PS_Waiting_Handshake)) {
			CY_LOG(L_ERROR, "Only support one client");
			conn->shutdown();
			return;
		}
		m_client_id = conn->get_id();
		CY_LOG(L_INFO, "Client connected!");
	}

	//-------------------------------------------------------------------------------------
	void on_client_message(ConnectionType conn)
	{
		if (m_client_id != conn->get_id()) return;
		assert(m_status.load() == PS_Waiting_Handshake || m_status.load() == PS_Working);

		RingBuf& rb = conn->get_input_buf();
		while (rb.size() >= sizeof(PingPong_Head))
		{
			PingPong_Head head;
			rb.peek(0, &head, sizeof(PingPong_Head));
			if ((int32_t)rb.size() < head.size) return;

			switch (head.id)
			{
			case PingPong_HandShake::ID:
				_on_recv_handshake_message(conn);
				break;

			case PingPong_PingData::ID:
				_on_recv_ping_data(conn);
				break;

			case PingPong_Close::ID:
				_on_recev_close_message(conn);
				break;

			default:
				{
				//error
				CY_LOG(L_ERROR, "Error! Receive unknown message: %d", head.id);
				conn->shutdown();
				return;
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	void _on_recv_handshake_message(ConnectionType conn)
	{
		if (m_status != PS_Waiting_Handshake) {
			CY_LOG(L_ERROR, "Error! Receive handshake message on status: %d", m_status.load());
			conn->shutdown();
			return;
		}
		CY_LOG(L_INFO, "Receive handshake message");

		RingBuf& rb = conn->get_input_buf();

		PingPong_HandShake handshake;
		rb.memcpy_out(&handshake, sizeof(PingPong_HandShake));

		// handshake
		m_work_mode = handshake.work_mode;
		m_data_crc = INITIAL_ADLER;
		m_pingpong_counts = handshake.counts;

		//send handshake back
		_send_handshake_message(conn);
	}

	//-------------------------------------------------------------------------------------
	void _send_handshake_message(ConnectionType conn)
	{
		CY_LOG(L_INFO, "Send handshake message");
		m_status = PS_Working;

		PingPong_HandShake msg;
		msg.id = PingPong_HandShake::ID;
		msg.size = sizeof(PingPong_HandShake);
		msg.work_mode = m_work_mode;
		msg.counts = m_pingpong_counts;
		conn->send((const char*)&msg, sizeof(msg));
	}

	//-------------------------------------------------------------------------------------
	void _on_recv_ping_data(ConnectionType conn)
	{
		if (m_status != PS_Working) {
			CY_LOG(L_ERROR, "Receive ping data on status: %d", m_status.load());
			conn->shutdown();
			return;
		}

		RingBuf& rb = conn->get_input_buf();

		PingPong_PingData ping;
		rb.memcpy_out(&ping, sizeof(PingPong_PingData));

		//check ping data
		if (ping.size != (int32_t)sizeof(PingPong_PongData) + ping.data_size)
		{
			CY_LOG(L_ERROR, "Receive ping data size error, ping data size=%d!", ping.data_size);
			conn->shutdown();
			return;
		}

		CY_LOG(L_TRACE, "Receive %d/%d ping message", ping.index+1, m_pingpong_counts);

		//receive ping data
		char temp_data[MAX_DATA_SIZE];
		rb.memcpy_out(temp_data, ping.data_size);

		//send pong data
		if (m_work_mode == 1) {
			_send_pong_data(conn, temp_data, ping.data_size);
		}
		else if (m_work_mode == 2) {
			m_data_crc = adler32(m_data_crc, (const uint8_t*)temp_data, ping.data_size);
		}
	}

	//-------------------------------------------------------------------------------------
	void _send_pong_data(ConnectionType conn, const char* data, int32_t size)
	{
		PingPong_PongData pong;
		pong.id = PingPong_PongData::ID;
		pong.size = (int32_t)sizeof(PingPong_PongData) + size;
		pong.data_size = size;

		//send...
		conn->send((const char*)&pong, sizeof(PingPong_PongData));
		conn->send((const char*)data, size);
	}

	//-------------------------------------------------------------------------------------
	void _on_recev_close_message(ConnectionType conn)
	{
		if (m_status != PS_Working) {
			CY_LOG(L_ERROR, "Receive close message on status: %d", m_status.load());
			conn->shutdown();
			return;
		}

		CY_LOG(L_INFO, "Receive close message");

		RingBuf& rb = conn->get_input_buf();

		PingPong_Close close_msg;
		rb.memcpy_out(&close_msg, sizeof(PingPong_Close));

		//send close cmd back
		_send_close_message(conn);
	}

	//-------------------------------------------------------------------------------------
	void _send_close_message(ConnectionType conn)
	{
		CY_LOG(L_INFO, "send close message to client(CRC=0x%08X), switch status to Closing and begin shutdown connection!", m_data_crc);

		m_status = PS_Closing;

		PingPong_Close close_msg;
		close_msg.id = PingPong_Close::ID;
		close_msg.size = sizeof(PingPong_Close);
		close_msg.data_crc = m_data_crc;
		conn->send((const char*)&close_msg, sizeof(PingPong_Close));

		//shutdown connection
		conn->shutdown();
	}

	//-------------------------------------------------------------------------------------
	void on_client_close(ConnectionType conn)
	{
		if (conn->get_id() != m_client_id) return;
		assert(m_status.load() == PS_Waiting_Handshake || m_status.load() == PS_Working || m_status.load() == PS_Closing);

		CY_LOG(L_INFO, "client connection closed!, switch status to Listening");
		m_status = PS_Listening;
		m_client_id = -1;
	}
protected:
	enum Status
	{
		PS_Closed,
		PS_Listening,
		PS_Waiting_Handshake,
		PS_Working,
		PS_Closing,
	};
	ServerType m_server;
	int32_t m_listen_port;
	std::atomic<Status> m_status;
	int32_t m_client_id;
	int32_t m_work_mode;
	uint32_t m_data_crc;
	int32_t m_pingpong_counts;

public:
	PingPongServer(int32_t listen_port)
		: m_listen_port(listen_port)
		, m_status(PS_Closed)
		, m_client_id(-1)
		, m_work_mode(1)
		, m_data_crc(INITIAL_ADLER)
		, m_pingpong_counts(0)
	{
	}
};

//-------------------------------------------------------------------------------------
class KcpPingPongServer : public PingPongServer<UdpServer, UdpConnectionPtr>
{
public:
	virtual void on_client_connected(UdpConnectionPtr conn)
	{
		PingPongServer<UdpServer, UdpConnectionPtr>::on_client_connected(conn);
		m_server.m_listener.on_close = nullptr;
		conn->set_on_closing(std::bind(&KcpPingPongServer::on_client_close, this, _1));
	}

public:
	KcpPingPongServer(int32_t listen_port) 
		: PingPongServer<UdpServer, UdpConnectionPtr>(listen_port)
	{

	}
};

//-------------------------------------------------------------------------------------
void printUsage(const char* moduleName)
{
	printf("===== TCP/KCP Pingpong Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -p  SERVER_PORT\tListen Port, Default 1978\n");
	printf("\t -k\t\t\tEnable KCP Protocol\n");
	printf("\t -v\t\t\tEnable Verbose Mode\n");
	printf("\t --help -?\t\tShow this help\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	uint16_t server_port = 1978;
	bool enable_kcp = false;
	bool verbose_mode = false;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_SERVER_PORT) {
				server_port = (uint16_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_KCP) {
				enable_kcp = true;
			}
			else if (args.OptionId() == OPT_VERBOSE) {
				verbose_mode = true;
			}
		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	set_log_threshold(verbose_mode ? L_TRACE : L_DEBUG);
	CY_LOG(L_DEBUG, "======== PingPongServer ========");
	CY_LOG(L_DEBUG, "ListenPort:%d", server_port);
	CY_LOG(L_DEBUG, "KCP : %s",  enable_kcp ? "enable" : "disable");
	CY_LOG(L_DEBUG, "Verbose: %s", verbose_mode ? "enable" : "disable");

	if (enable_kcp) {
		KcpPingPongServer server(server_port);
		server.start_and_join();
	}
	else {
		PingPongServer<TcpServer, ConnectionPtr> server(server_port);
		server.start_and_join();
	}
	return 0;
}