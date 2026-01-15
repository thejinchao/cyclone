#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <utility/cyu_simple_opt.h>

#include "chat_message.h"

#include <iostream>

using namespace cyclone;
using namespace std::placeholders;

enum { OPT_HOST, OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_HOST, "-h",     SO_REQ_SEP }, // "-h HOST_IP"
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
class ChatClient
{
public:
	void startClientThread(const std::string& server_ip, uint16_t server_port)
	{
		m_server_ip = server_ip;
		m_server_port = server_port;

		//start client thread
		sys_api::thread_create_detached(std::bind(&ChatClient::clientThread, this), nullptr, "client");
		CY_LOG(L_DEBUG, "connect to %s:%d...", server_ip.c_str(), server_port);

		//wait connect completed
		sys_api::signal_wait(m_connected_signal);
	}

public:
	//-------------------------------------------------------------------------------------
	void clientThread(void)
	{
		Looper* looper = Looper::create_looper();

        m_client = std::make_shared<TcpClient>(looper, nullptr);
		m_client->m_listener.on_connected = std::bind(&ChatClient::onConnected, this, _1, _3);
		m_client->m_listener.on_message = std::bind(&ChatClient::onMessage, this, _2);
		m_client->m_listener.on_close = std::bind(&ChatClient::onClose, this);

		m_client->connect(Address(m_server_ip.c_str(), m_server_port));
		looper->loop();

		m_client = nullptr;
		Looper::destroy_looper(looper);
	}

	//-------------------------------------------------------------------------------------
	virtual uint32_t onConnected(TcpClientPtr client, bool success)
	{
		CY_LOG(L_DEBUG, "connect to %s:%d %s.",
			client->get_server_address().get_ip(),
			client->get_server_address().get_port(),
			success ? "OK" : "FAILED");

		if (success){
			m_client = client;
			sys_api::signal_notify(m_connected_signal);
			return 0;
		}
		else
		{
			uint32_t retry_time = 1000 * 5;
			CY_LOG(L_INFO, "connect failed!, retry after %d milliseconds...\n", retry_time);
			return 1000 * 5;
		}
	}

	//-------------------------------------------------------------------------------------
	virtual void onMessage(TcpConnectionPtr conn)
	{
		RingBuf& buf = conn->get_input_buf();

		Packet packet;
		packet.build_from_ringbuf(PACKET_HEAD_SIZE, buf);

		char temp[1024] = { 0 };
		memcpy(temp, packet.get_packet_content(), packet.get_packet_size());

		CY_LOG(L_INFO, "%s", temp);
	}

	//-------------------------------------------------------------------------------------
	virtual void onClose(void)
	{
		CY_LOG(L_INFO, "socket close");
		exit(0);
	}
	
public:
	TcpClientPtr get_client(void) { return m_client; }

private:
	TcpClientPtr m_client;
	std::string m_server_ip;
	uint16_t m_server_port;
	sys_api::signal_t m_connected_signal;

public:
	ChatClient()
		: m_client(nullptr)
	{
		m_connected_signal = sys_api::signal_create();
	}

	~ChatClient()
	{
		sys_api::signal_destroy(m_connected_signal);
	}
};

//-------------------------------------------------------------------------------------
static void printUsage(const char* moduleName)
{
	printf("===== Chat Client(Powerd by Cyclone) =====\n");
	printf("Usage: %s [-h HOST_IP][-p HOST_PORT] [-?] [--help]\n", moduleName);
}

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	std::string server_ip = "127.0.0.1";
	uint16_t server_port = 1978;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_HOST) {
				server_ip = args.OptionArg();
			}
			else if (args.OptionId() == OPT_PORT) {
				server_port = (uint16_t)atoi(args.OptionArg());
			}
		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	ChatClient chatClient;
	chatClient.startClientThread(server_ip, server_port);

	//input
	char line[MAX_CHAT_LENGTH + 1];
	while (std::cin.getline(line, MAX_CHAT_LENGTH + 1))
	{
		if (line[0] == 0) continue;

		Packet packet;
		packet.build_from_memory(PACKET_HEAD_SIZE, 0, (uint16_t)strlen(line), line);

		chatClient.get_client()->send(packet.get_memory_buf(), packet.get_memory_size());
	}

	return 0;
}

