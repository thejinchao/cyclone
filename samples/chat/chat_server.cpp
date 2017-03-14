#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

#include "chat_message.h"

#include <map>

using namespace cyclone;

class ServerListener : public TcpServer::Listener
{
	//-------------------------------------------------------------------------------------
	virtual void on_connection_callback(TcpServer*, int32_t thread_index, Connection* conn)
	{
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
	virtual void on_message_callback(TcpServer* server, int32_t thread_index, Connection* conn)
	{
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
	virtual void on_close_callback(TcpServer*, int32_t thread_index, Connection* conn)
	{
		sys_api::auto_mutex lock(this->clients_lock);

		this->clients.erase(conn->get_id());
		CY_LOG(L_DEBUG, "connection %s:%d closed",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	void on_extra_workthread_msg(TcpServer* server, int32_t thread_index, Packet* msg)
	{

	}


private:
	struct Client
	{
		int32_t agent_id;
		Connection* connection;
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
int main(int argc, char* argv[])
{
	uint16_t server_port = 1978;

	if (argc > 1)
		server_port = (uint16_t)atoi(argv[1]);

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
