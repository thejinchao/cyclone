#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>

using namespace cyclone;
using namespace std::placeholders;

#define MAX_ECHO_LENGTH (255)
enum { OPT_PORT, OPT_KCP, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_KCP,  "-k",	  SO_NONE },	// "-k"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
void onPeerConnected(UdpServer* server, int32_t thread_index, UdpConnectionPtr conn)
{
	CY_LOG(L_INFO, "[T=%d]peer connected(from '%s:%d')", thread_index, conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());
}

//-------------------------------------------------------------------------------------
void onPeerMessage(UdpServer* server, int32_t thread_index, UdpConnectionPtr conn)
{
	char temp[MAX_ECHO_LENGTH + 1] = { 0 };
	conn->get_input_buf().memcpy_out(temp, MAX_ECHO_LENGTH);

	CY_LOG(L_INFO, "[T=%d]receive(from '%s:%d'):'%s'", thread_index, conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port(), temp);

	if (strcmp(temp, "shutdown") == 0) {
		sys_api::thread_create_detached([server](void*) {
			server->stop();
		}, 0, nullptr);
		return;
	}

	size_t len = strlen(temp);
	for (size_t i = 0; i < len; i++) temp[i] = (char)toupper(temp[i]);
	conn->send(temp, (int32_t)len + 1);
}

//-------------------------------------------------------------------------------------
void onPeerClose(UdpServer* server, int32_t thread_index, UdpConnectionPtr conn)
{
	CY_LOG(L_INFO, "[T=%d]peer closed(from '%s:%d')", thread_index, conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());
}

//-------------------------------------------------------------------------------------
void printUsage(const char* moduleName)
{
	printf("===== UDP/KCP Echo Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [-p LISTEN_PORT] [-k] [-?] [--help]\n", moduleName);
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);
	uint16_t server_port = 1978;
	bool enable_kcp = false;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_PORT) {
				server_port = (uint16_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_KCP) {
				enable_kcp = true;
			}
		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}
	CY_LOG(L_INFO, "Listen port %d, KCP %s", server_port, enable_kcp?"enable":"disable");

	UdpServer server(enable_kcp);
	if (!server.bind(Address(server_port, false))) {
		CY_LOG(L_ERROR, "Can't bind port");
		return 1;
	}

	server.m_listener.on_connected = onPeerConnected;
	server.m_listener.on_message = onPeerMessage;
	server.m_listener.on_close = onPeerClose;

	if (!server.start(sys_api::get_cpu_counts())) {
		CY_LOG(L_ERROR, "Start udp server failed!");
		return 1;
	}
	server.join();
	return 0;
}
