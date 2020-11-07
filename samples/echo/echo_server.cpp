#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <utility/cyu_simple_opt.h>

#include <ctype.h>

using namespace cyclone;

#define MAX_ECHO_LENGTH (255)

enum { OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
void onPeerConnected(TcpServer* server, int32_t thread_index, TcpConnectionPtr conn)
{
	(void)server;

	CY_LOG(L_INFO, "[T=%d]new connection accept, from %s:%d to %s:%d",
		thread_index,
		conn->get_peer_addr().get_ip(),
		conn->get_peer_addr().get_port(),
		conn->get_local_addr().get_ip(),
		conn->get_local_addr().get_port());
}

//-------------------------------------------------------------------------------------
void onPeerMessage(TcpServer* server, int32_t thread_index, TcpConnectionPtr conn)
{
	RingBuf& buf = conn->get_input_buf();

	char temp[MAX_ECHO_LENGTH + 1] = { 0 };
	buf.memcpy_out(temp, MAX_ECHO_LENGTH);

	CY_LOG(L_INFO, "[T=%d]receive:%s", thread_index, temp);

	if (strcmp(temp, "exit") == 0) {
		server->shutdown_connection(conn);
		return;
	}

	if (strcmp(temp, "shutdown") == 0) {
		sys_api::thread_create_detached([server](void*) {
			server->stop();
		}, 0, nullptr);
		return;
	}

	size_t len = strlen(temp);
	for (size_t i = 0; i < len; i++) temp[i] = (char)toupper(temp[i]);

	conn->send(temp, strlen(temp));
}

//-------------------------------------------------------------------------------------
void onPeerClose(TcpServer* server, int32_t thread_index, TcpConnectionPtr conn)
{
	(void)server;

	CY_LOG(L_INFO, "[T=%d]connection %s:%d closed",
		thread_index,
		conn->get_peer_addr().get_ip(),
		conn->get_peer_addr().get_port());
}

//-------------------------------------------------------------------------------------
static void printUsage(const char* moduleName)
{
	printf("===== Echo Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [-p LISTEN_PORT] [-?] [--help]\n", moduleName);
}

//-------------------------------------------------------------------------------------
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

	TcpServer server;

	server.m_listener.on_connected = onPeerConnected;
	server.m_listener.on_close = onPeerClose;
	server.m_listener.on_message = onPeerMessage;

	if (!server.bind(Address(server_port, false), true))
		return 1;

	if (!server.start(sys_api::get_cpu_counts()))
		return 1;

	server.join();

	return 0;
}