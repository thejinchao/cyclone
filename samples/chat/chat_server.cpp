#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <SimpleOpt.h>

#include "chat_message.h"

#include <map>

using namespace cyclone;
using namespace std::placeholders;

enum { OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

//-------------------------------------------------------------------------------------
class ChatServer
{
public:
	void startAndJoin(uint16_t server_port)
	{
		m_clients_lock = sys_api::mutex_create();

		TcpServer server("chat_server", nullptr);
		server.m_listener.on_connected = std::bind(&ChatServer::onClientConnected, this, _3);
		server.m_listener.on_message = std::bind(&ChatServer::onClientMessage, this, _3);
		server.m_listener.on_close = std::bind(&ChatServer::onClientClose, this, _3);

		if (!server.bind(Address(server_port, false), false)) return;

		if (!(server.start(sys_api::get_cpu_counts()))) return;

		server.join();

		sys_api::mutex_destroy(m_clients_lock);
	}
private:
	//-------------------------------------------------------------------------------------
	void onClientConnected(ConnectionPtr conn)
	{
		//new connection
		sys_api::auto_mutex lock(m_clients_lock);
		m_clients.insert({ conn->get_id(),{ conn->get_id(), conn } });

		CY_LOG(L_DEBUG, "new connection accept, from %s:%d to %s:%d",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port(),
			conn->get_local_addr().get_ip(),
			conn->get_local_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	void onClientMessage(ConnectionPtr conn)
	{
		RingBuf& buf = conn->get_input_buf();

		for (;;)
		{
			Packet packet;
			if (!packet.build(PACKET_HEAD_SIZE, buf)) return;

			{
				sys_api::auto_mutex lock(m_clients_lock);

				for (auto client : m_clients) {
					client.second.connection->send(packet.get_memory_buf(), packet.get_memory_size());
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	void onClientClose(ConnectionPtr conn)
	{
		sys_api::auto_mutex lock(m_clients_lock);

		m_clients.erase(conn->get_id());
		CY_LOG(L_DEBUG, "connection %s:%d closed",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}

private:
	struct Client
	{
		int32_t agent_id;
		ConnectionPtr connection;
	};
	std::map< int32_t, Client > m_clients;
	sys_api::mutex_t m_clients_lock;
};

//-------------------------------------------------------------------------------------
static void printUsage(const char* moduleName)
{
	printf("===== Chat Server(Powerd by Cyclone) =====\n");
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

	ChatServer server;
	server.startAndJoin(server_port);
	return 0;
}
