#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>
#include <utility/cyu_string_util.h>
#include <iostream>
#include "pingpong_common.h"

using namespace cyclone;
using namespace std::placeholders;

#define MAX_DATA_SIZE		(1024*4)
#define DEF_DATA_SIZE		(512)
#define MAX_DATA_COUNTS		(0xFFFF)

enum { OPT_SERVER_ADDR, OPT_SERVER_PORT, OPT_KCP, OPT_DATA_SIZE, OPT_DATA_COUNTS, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_SERVER_ADDR, "-h",	SO_REQ_SEP }, // "-h SERVER_IP"
	{ OPT_SERVER_PORT, "-p",	SO_REQ_SEP }, // "-p SERVER_PORT"
	{ OPT_KCP,  "-k",			SO_NONE },	// "-k"
	{ OPT_DATA_SIZE, "-s",		SO_REQ_SEP }, // "-s DATA_SIZE"
	{ OPT_DATA_COUNTS, "-c",		SO_REQ_SEP }, // "-c DATA_COUNTS"
	{ OPT_HELP, "-?",		SO_NONE },	// "-?"
	{ OPT_HELP, "--help",	SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};
//-------------------------------------------------------------------------------------
template<typename ConnectionType>
class PingPongClient
{
protected:
	//-------------------------------------------------------------------------------------
	void on_server_message(ConnectionType conn)
	{
		RingBuf& rb = conn->get_input_buf();
		if (rb.size() < sizeof(PingPong_Head)) return;

		PingPong_Head head;
		rb.peek(0, &head, sizeof(PingPong_Head));
		if ((int32_t)rb.size() < head.size) return;

		switch (head.id)
		{
		case PingPong_HandShake::ID:
			_on_recv_handshake_message(conn);
			break;

		case PingPong_PongData::ID:
			_on_recv_pong_data(conn);
			break;

		case PingPong_Close::ID:
			_on_recv_close_message(conn);
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

	//-------------------------------------------------------------------------------------
	void on_server_close(void)
	{
		int32_t time_span = (int32_t)((m_end_time - m_begin_time) / 1000);
		float speed = (float)(m_data_size*m_index * 1000) / (float)time_span;
		CY_LOG(L_INFO, "DataSize=%d, TotalCounts=%d, Time=%d(ms), Speed=%s/s",
			m_data_size, m_index, time_span, string_util::size_to_string(speed).c_str());

		m_looper->push_stop_request();
	}

	//-------------------------------------------------------------------------------------
	void _send_handshake_message(void)
	{
		CY_LOG(L_INFO, "Send handshake message");
		m_status = PS_Handshaking;

		PingPong_HandShake msg;
		msg.id = PingPong_HandShake::ID;
		msg.size = sizeof(PingPong_HandShake);
		m_connection->send((const char*)&msg, sizeof(msg));
	}

	//-------------------------------------------------------------------------------------
	void _on_recv_handshake_message(ConnectionType conn)
	{
		if (m_status != PS_Handshaking) {
			CY_LOG(L_ERROR, "Error! Receive handshake message on status: %d", m_status.load());
			conn->shutdown();
			return;
		}
		RingBuf& rb = conn->get_input_buf();

		PingPong_HandShake handshake;
		rb.memcpy_out(&handshake, sizeof(PingPong_HandShake));

		CY_LOG(L_INFO, "Receive handshake message.");
		//
		//TODO: handshake
		//

		//begin send ping data
		m_status = PS_Working;
		m_index = 0;
		m_send_data_crc = INITIAL_ADLER;
		m_begin_time = sys_api::utc_time_now();
		_send_ping_message();
	}

	//-------------------------------------------------------------------------------------
	void _send_ping_message()
	{
		assert(m_status == PS_Working);

		//fill random data
		_fill_random_data();

		//send ping data
		PingPong_PingData data;
		data.id = PingPong_PingData::ID;
		data.size = (int32_t)sizeof(PingPong_PingData) + m_data_size;
		data.data_size = m_data_size;

		//send...
		m_connection->send((const char*)&data, sizeof(PingPong_PingData));
		m_connection->send((const char*)m_send_data, m_data_size);
	}

	//-------------------------------------------------------------------------------------
	void _on_recv_pong_data(ConnectionType conn)
	{
		if (m_status != PS_Working) {
			CY_LOG(L_ERROR, "Error! Receive pong message on status: %d", m_status.load());
			conn->shutdown();
			return;
		}
		RingBuf& rb = conn->get_input_buf();

		PingPong_PongData pong;
		rb.memcpy_out(&pong, sizeof(PingPong_PongData));

		//check pong data
		if (pong.size != (int32_t)sizeof(PingPong_PongData)+pong.data_size ||
			pong.data_size != m_data_size || 
			memcmp(rb.normalize(), m_send_data, m_data_size) != 0) 
		{
			CY_LOG(L_ERROR, "Receive %d data error!", m_index);
			conn->shutdown();
			return;
		}
		rb.discard(m_data_size);

		if (m_index % 10 == 0) {
			CY_LOG(L_DEBUG, "Receive %d/%d pong data", m_index, m_pingpong_counts);
		}

		//send next ping data
		if (++m_index < m_pingpong_counts) {
			_send_ping_message();
			return;
		}

		//last pong data
		m_end_time = sys_api::utc_time_now();
		CY_LOG(L_INFO, "Receive Last pong, send close message");
		_send_close_message();
	}

	//-------------------------------------------------------------------------------------
	void _send_close_message(void)
	{
		m_status = PS_Closing;

		PingPong_Close close_msg;
		close_msg.id = PingPong_Close::ID;
		close_msg.size = sizeof(PingPong_Close);
		m_connection->send((const char*)&close_msg, sizeof(PingPong_Close));
	}

	//-------------------------------------------------------------------------------------
	void _on_recv_close_message(ConnectionType conn)
	{
		if (m_status != PS_Closing) {
			CY_LOG(L_ERROR, "Error! Receive close message on status: %d", m_status.load());
			conn->shutdown();
			return;
		}
		CY_LOG(L_DEBUG, "Receive close message");

		RingBuf& rb = conn->get_input_buf();

		PingPong_Close close_msg;
		rb.memcpy_out(&close_msg, sizeof(PingPong_Close));

		//shutdown connection
		conn->shutdown();
	}

	//-------------------------------------------------------------------------------------
	void _fill_random_data(void) {
		XorShift128 xorShift128;
		xorShift128.make();

		for (size_t i = 0; i < m_data_size / sizeof(uint64_t) + 1; i++) {
			((uint64_t*)m_send_data)[i] = xorShift128.next();
		}
		m_send_data_crc = adler32(m_send_data_crc, (const uint8_t*)m_send_data, m_data_size);
	}

protected:
	enum Status {
		PS_Connecting,
		PS_Handshaking,
		PS_Working,
		PS_Closing
	};
	Address m_server_address;
	ConnectionType m_connection;
	std::atomic<Status> m_status;
	Looper* m_looper;
	int32_t m_index;
	int32_t m_pingpong_counts;
	uint8_t m_send_data[MAX_DATA_SIZE + 8];
	uint32_t m_send_data_crc;
	int32_t m_data_size;
	RingBuf m_receive_data;
	int64_t m_begin_time, m_end_time;

public:
	PingPongClient(const char* server_ip, int32_t server_port, int32_t data_size, int32_t pingpong_counts)
		: m_server_address(server_ip, (uint16_t)server_port)
		, m_status(PS_Connecting)
		, m_index(0)
		, m_pingpong_counts(pingpong_counts)
		, m_send_data_crc(INITIAL_ADLER)
		, m_data_size(data_size)
		, m_begin_time(-1)
		, m_end_time(-1)
	{
		assert(m_data_size <= MAX_DATA_SIZE);

		//fill random data
		srand((unsigned int)::time(0));
		_fill_random_data();
	}
};

//-------------------------------------------------------------------------------------
class TcpPingPongClient : public PingPongClient<ConnectionPtr>
{
public:
	void start_and_join(void)
	{
		m_looper = Looper::create_looper();
		m_client = std::make_shared<TcpClient>(m_looper, nullptr);

		m_client->m_listener.on_connected = std::bind(&TcpPingPongClient::on_server_connected, this, _2, _3);
		m_client->m_listener.on_message = std::bind(&TcpPingPongClient::on_server_message, this, _2);
		m_client->m_listener.on_close = std::bind(&TcpPingPongClient::on_server_close, this);

		m_client->connect(m_server_address);

		m_looper->loop();
		m_client = nullptr;
		Looper::destroy_looper(m_looper);
		m_looper = nullptr;
	}

protected:
	//-------------------------------------------------------------------------------------
	uint32_t on_server_connected(ConnectionPtr conn, bool success)
	{
		assert(m_status == PS_Connecting);

		if (!success) {
			uint32_t retry_time = 1000 * 5;
			CY_LOG(L_INFO, "connect failed!, retry after %d milliseconds...\n", retry_time);
			return 1000 * 5;
		}
		m_connection = conn;

		//send handshake message after connected
		_send_handshake_message();
		return 0;
	}

private:
	TcpClientPtr m_client;

public:
	TcpPingPongClient(const char* server_ip, int32_t server_port, int32_t data_size, int32_t pingpong_counts)
		: PingPongClient<ConnectionPtr>(server_ip, server_port, data_size, pingpong_counts)
	{

	}
};

//-------------------------------------------------------------------------------------
class KcpPingPongClient : public PingPongClient<UdpConnectionPtr>
{
public:
	void start_and_join(void)
	{
		m_looper = Looper::create_looper();

		m_connection = std::make_shared<UdpConnection>(m_looper, true);
		if (!m_connection->init(m_server_address)) {
			CY_LOG(L_ERROR, "Init udp socket failed...");
			return;
		}
		m_connection->set_on_message(std::bind(&KcpPingPongClient::on_server_message, this, _1));
		m_connection->set_on_close(std::bind(&KcpPingPongClient::on_server_close, this));

		//send handshake message
		_send_handshake_message();

		//enter loop
		m_looper->loop();

		// quit
		m_connection = nullptr;
		Looper::destroy_looper(m_looper);
		m_looper = nullptr;
	}

public:
	KcpPingPongClient(const char* server_ip, int32_t server_port, int32_t data_size, int32_t pingpong_counts)
		: PingPongClient<UdpConnectionPtr>(server_ip, server_port, data_size, pingpong_counts)
	{

	}
};

//-------------------------------------------------------------------------------------
void printUsage(const char* moduleName)
{
	printf("===== TCP/KCP Pingpong Client(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -h  SERVER_IP\t\tTarget Server IP, Default 127.0.0.1\n");
	printf("\t -p  SERVER_PORT\tListen Port(server mode) or Server Port(client mode), Default 1978\n");
	printf("\t -k\t\t\tEnable KCP Protocol, Default not\n");
	printf("\t -s  DATA_SIZE\t\tData Size(bytes), max size %d, Default %d\n", MAX_DATA_SIZE, DEF_DATA_SIZE);
	printf("\t -c  DATA_COUNTS\tData counts, max counts %d, Default 500\n", MAX_DATA_COUNTS);
	printf("\t --help -?\t\tShow this help\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	std::string server_ip = "127.0.0.1";
	uint16_t server_port = 1978;
	bool enable_kcp = false;
	int32_t data_size = DEF_DATA_SIZE;
	int32_t data_counts = 500;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_SERVER_ADDR) {
				server_ip = args.OptionArg();
			}
			else if (args.OptionId() == OPT_SERVER_PORT) {
				server_port = (uint16_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_KCP) {
				enable_kcp = true;
			}
			else if (args.OptionId() == OPT_DATA_SIZE) {
				data_size = (int32_t)atoi(args.OptionArg());
				if (data_size <= 0 || data_size > MAX_DATA_SIZE) {
					printUsage(argv[0]);
					return 0;
				}
			}
			else if (args.OptionId() == OPT_DATA_COUNTS) {
				data_counts = (int32_t)atoi(args.OptionArg());
				if (data_counts <= 0 || data_counts > MAX_DATA_COUNTS) {
					printUsage(argv[0]);
					return 0;
				}
			}
		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	CY_LOG(L_DEBUG, "ServerAddress:%s:%d, KCP:%s, DataSize:%d, DataCounts:%d", 
		server_ip.c_str(), server_port, enable_kcp ? "enable" : "disable", data_size, data_counts);
	//set_log_threshold(L_TRACE);

	if (enable_kcp) {
		KcpPingPongClient client(server_ip.c_str(), server_port, data_size, data_counts);
		client.start_and_join();
	}
	else {
		TcpPingPongClient client(server_ip.c_str(), server_port, data_size, data_counts);
		client.start_and_join();
	}

	return 0;
}
