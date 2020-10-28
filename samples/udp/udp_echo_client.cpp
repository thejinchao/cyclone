#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>
#include <iostream>

using namespace cyclone;
using namespace std::placeholders;

#define MAX_ECHO_LENGTH (255)

enum { OPT_HOST, OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_HOST, "-h",     SO_REQ_SEP }, // "-h HOST_IP"
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
void printUsage(const char* moduleName)
{
	printf("===== Echo Client(Powerd by Cyclone) =====\n");
	printf("Usage: %s [-h HOST_IP][-p HOST_PORT] [-?] [--help]\n", moduleName);
}

class UdpClient
{
public:
	void thread_function(void)
	{
		CY_LOG(L_TRACE, "Begin work thread...");

		m_looper = Looper::create_looper();

		//create connection
		m_connection = std::make_shared<UdpConnection>(m_looper);
		if (!m_connection->init(m_server_addr)) {
			return;
		}

		//set callback 
		m_connection->set_on_message(std::bind(&UdpClient::on_peer_message, this, std::placeholders::_1));

		m_looper->loop();
		Looper::destroy_looper(m_looper);
	}

	void send(const char* buf, int32_t len)
	{
		m_connection->send(buf, len);
	}

	void on_peer_message(UdpConnectionPtr conn)
	{
		char temp_buf[1024] = { 0 };
		
		RingBuf& input_buf = conn->get_input_buf();
		input_buf.memcpy_out(temp_buf, 1024);

		CY_LOG(L_TRACE, "Receive: %s", temp_buf);
	}
private:
	Address m_server_addr;
	Looper* m_looper;
	UdpConnectionPtr m_connection;

public:
	UdpClient(const char* server_ip, uint16_t server_port)
		: m_server_addr(server_ip, server_port)
		, m_looper(nullptr)
	{
	}
	~UdpClient()
	{
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
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
	CY_LOG(L_DEBUG, "ServerAddress: %s:%d", server_ip.c_str(), server_port);

	UdpClient theClient(server_ip.c_str(), server_port);
	sys_api::thread_create_detached(std::bind(&UdpClient::thread_function, &theClient), nullptr, "udp_send");

	//input
	char line[MAX_ECHO_LENGTH + 1] = { 0 };
	while (std::cin.getline(line, MAX_ECHO_LENGTH + 1))
	{
		if (line[0] == 0) continue;

		theClient.send(line, (int32_t)strlen(line) + 1);
	}

	return 0;
}
