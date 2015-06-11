#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

#include <ctype.h>

using namespace cyclone;

class ServerListener : public TcpServer::Listener
{
	//-------------------------------------------------------------------------------------
	virtual void on_connection_callback(TcpServer* server, Connection* conn)
	{
		(void)server;

		CY_LOG(L_INFO, "new connection accept, from %s:%d to %s:%d",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port(),
			conn->get_local_addr().get_ip(),
			conn->get_local_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	virtual void on_message_callback(TcpServer* server, Connection* conn)
	{
		RingBuf& buf = conn->get_input_buf();

		char temp[1024] = { 0 };
		buf.memcpy_out(temp, 1024);

		CY_LOG(L_INFO, "receive:%s", temp);

		if (strcmp(temp, "exit") == 0)
		{
			server->shutdown_connection(conn);
			return;
		}

		size_t len = strlen(temp);
		for (size_t i = 0; i < len; i++) temp[i] = (char)toupper(temp[i]);
		strcat(temp, "\n");

		conn->send(temp, strlen(temp));
	}

	//-------------------------------------------------------------------------------------
	virtual void on_close_callback(TcpServer* server, Connection* conn)
	{
		(void)server;

		CY_LOG(L_INFO, "connection %s:%d closed",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}
};

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	(void)argc; 
	(void)argv;

	thread_api::thread_set_current_name("main");

	ServerListener listener;

	TcpServer server(&listener);
	server.bind(Address(1978, false), true);

	if (!server.start(2))
		return 1;

	server.join();
	return 0;
}