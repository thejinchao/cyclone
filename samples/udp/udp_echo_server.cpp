#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>

using namespace cyclone;
using namespace std::placeholders;

#define MAX_ECHO_LENGTH (255)
enum { OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
void onPeerMessage(UdpServer* server, int32_t thread_index, int32_t socket_index, RingBuf& ring_buf, const Address& peer_address)
{
	char temp[MAX_ECHO_LENGTH + 1] = { 0 };
	ring_buf.memcpy_out(temp, MAX_ECHO_LENGTH);

	CY_LOG(L_INFO, "[T=%d]receive(from '%s:%d'):'%s'", thread_index, peer_address.get_ip(), peer_address.get_port(), temp);

	if (strcmp(temp, "shutdown") == 0) {
		sys_api::thread_create_detached([server](void*) {
			server->stop();
		}, 0, nullptr);
		return;
	}

	size_t len = strlen(temp);
	for (size_t i = 0; i < len; i++) temp[i] = (char)toupper(temp[i]);

	server->sendto(thread_index, socket_index, temp, len + 1, peer_address);
}

//-------------------------------------------------------------------------------------
void printUsage(const char* moduleName)
{
	printf("===== UDP Echo Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [-p LISTEN_PORT] [-?] [--help]\n", moduleName);
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);
	uint16_t server_port = 1978;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
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
	CY_LOG(L_DEBUG, "listen port %d", server_port);

	UdpServer server(sys_api::get_cpu_counts());

	if (server.bind(Address(server_port, false))<0 ) {
			CY_LOG(L_ERROR, "Can't bind port");
		return 1;
	}

	server.m_listener.on_message = onPeerMessage;
	if (!server.start()) {
		CY_LOG(L_ERROR, "Start udp server failed!");
		return 1;
	}
	server.join();
	return 0;
}
