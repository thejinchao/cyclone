#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

#include <ctype.h>

using namespace cyclone;

class ServerListener : public TcpServer::Listener
{
	//-------------------------------------------------------------------------------------
	virtual void on_connection_callback(TcpServer* server, int32_t thread_index, Connection* conn)
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
	virtual void on_message_callback(TcpServer* server, int32_t thread_index, Connection* conn)
	{
		RingBuf& buf = conn->get_input_buf();

		char temp[1024] = { 0 };
		buf.memcpy_out(temp, 1024);

		CY_LOG(L_INFO, "[T=%d]receive:%s", thread_index, temp);

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
	virtual void on_close_callback(TcpServer* server, int32_t thread_index, Connection* conn)
	{
		(void)server;

		CY_LOG(L_INFO, "[T=%d]connection %s:%d closed",
			thread_index,
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}
	
	//-------------------------------------------------------------------------------------
	void on_extra_workthread_msg(TcpServer* server, int32_t thread_index, Packet* msg)
	{
		(void)server;
		(void)thread_index;
		(void)msg;
	}
};

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	(void)argc; 
	(void)argv;

	sys_api::thread_set_current_name("main");

	ServerListener listener;

	TcpServer server(&listener, "echo");
	server.bind(Address(1978, false), true);

	if (!server.start(2))
		return 1;

	server.join();

	return 0;
}