#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

#include <ctype.h>

using namespace cyclone;

//-------------------------------------------------------------------------------------
void on_connection_callback(TcpServer* server, Connection* conn)
{
	CY_LOG(L_INFO, "new connection accept, from %s:%d", 
		conn->get_peer_addr().get_ip(),
		conn->get_peer_addr().get_port());
}

//-------------------------------------------------------------------------------------
void on_message_callback(TcpServer* server, Connection* conn)
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
	for (size_t i = 0; i < len; i++) temp[i]=toupper(temp[i]);
	strcat(temp, "\n");

	conn->send(temp, strlen(temp));
}

//-------------------------------------------------------------------------------------
void on_close_callback(TcpServer* server, Connection* conn)
{
	CY_LOG(L_INFO, "connection %s:%d closed", 
		conn->get_peer_addr().get_ip(),
		conn->get_peer_addr().get_port());
}

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	uint16_t server_port = 1978;
	if (argc > 1)
		server_port = (uint16_t)atoi(argv[1]);

	TcpServer server(0);
	server.set_connection_callback(on_connection_callback);
	server.set_close_callback(on_close_callback);
	server.set_message_callback(on_message_callback);

	if (!server.start(Address(server_port, false), true, 2))
		return 1;

	server.join();
	return 0;
}