#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <SimpleOpt.h>

#include "chat_message.h"

#include <map>

using namespace cyclone;

enum { OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

class ServerListener : public TcpServer::Listener
{
	//-------------------------------------------------------------------------------------
	virtual void on_workthread_start(TcpServer* server, int32_t thread_index, Looper* looper)
	{
		(void)server;
		(void)thread_index;
		(void)looper;
	};

	//-------------------------------------------------------------------------------------
	virtual void on_connected(TcpServer* server, int32_t thread_index, ConnectionPtr conn)
	{
		(void)server;
		(void)thread_index;

		//new connection
		sys_api::auto_mutex lock(this->clients_lock);
		this->clients.insert({ conn->get_id(), { conn->get_id(), conn} });

		CY_LOG(L_DEBUG, "new connection accept, from %s:%d to %s:%d",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port(),
			conn->get_local_addr().get_ip(),
			conn->get_local_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	virtual void on_message(TcpServer* server, int32_t thread_index, ConnectionPtr conn)
	{
		(void)server;
		(void)thread_index;

		RingBuf& buf = conn->get_input_buf();

		for (;;)
		{
			Packet packet;
			if (!packet.build(PACKET_HEAD_SIZE, buf)) return;

			{
				sys_api::auto_mutex lock(this->clients_lock);

				for (auto client : clients) {
					client.second.connection->send(packet.get_memory_buf(), packet.get_memory_size());
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	virtual void on_close(TcpServer* server, int32_t thread_index, ConnectionPtr conn)
	{
		(void)server;
		(void)thread_index;

		sys_api::auto_mutex lock(this->clients_lock);

		this->clients.erase(conn->get_id());
		CY_LOG(L_DEBUG, "connection %s:%d closed",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	void on_workthread_cmd(TcpServer* server, int32_t thread_index, Packet* msg)
	{
		(void)server;
		(void)thread_index;
		(void)msg;
	}


private:
	struct Client
	{
		int32_t agent_id;
		ConnectionPtr connection;
	};
	std::map< int32_t, Client > clients;
	sys_api::mutex_t clients_lock;

public:
	ServerListener()
	{
		this->clients_lock = sys_api::mutex_create();
	}
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

	ServerListener listener;
	TcpServer server(&listener, "chat_server", 0);
	if (!server.bind(Address(server_port, false), false))
		return 1;

	if (!(server.start(sys_api::get_cpu_counts())))
		return 1;

	server.join();
	return 0;
}
