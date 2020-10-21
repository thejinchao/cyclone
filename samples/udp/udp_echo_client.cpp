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
	socket_t s = socket_api::create_socket(true);
	Address server_addr(server_ip.c_str(), server_port);
	CY_LOG(L_DEBUG, "udp server %s:%d", server_ip.c_str(), server_port);

	//input
	char line[MAX_ECHO_LENGTH + 1] = { 0 };
	while (std::cin.getline(line, MAX_ECHO_LENGTH + 1))
	{
		if (line[0] == 0) continue;
		socket_api::sendto(s, line, strlen(line) + 1, server_addr.get_sockaddr_in());

		char tempBuf[MAX_ECHO_LENGTH+1] = { 0 };
		sockaddr_in peer_addr;
		socket_api::recvfrom(s, tempBuf, MAX_ECHO_LENGTH, peer_addr);

		Address remote_addr(peer_addr);
		printf("RecvFrom(%s:%d):%s\n", remote_addr.get_ip(), remote_addr.get_port(), tempBuf);
	}

	return 0;
}
