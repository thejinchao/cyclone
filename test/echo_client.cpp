#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

using namespace cyclone;

//-------------------------------------------------------------------------------------
struct client_s {
	const char* server_ip;
	uint16_t port;
	Pipe cmd_pipe;
	TcpClient* client;
};

#define MAX_MESSAGE_LEN (256)

//-------------------------------------------------------------------------------------
uint32_t on_connection_callback(TcpClient* client, bool success)
{
	CY_LOG(L_DEBUG, "connect to %s:%d %s.", 
		client->get_server_address().get_ip(), 
		client->get_server_address().get_port(), 
		success ? "OK" : "FAILED");

	if (success)
	{
		return 0;
	}
	else
	{
		uint32_t retry_time = 1000 * 5;
		printf("connect failed!, retry after %d milli seconds...\n", retry_time);
		return 1000 * 5;
	}
}

//-------------------------------------------------------------------------------------
void on_message_callback(TcpClient* client)
{
	RingBuf& buf = client->get_input_buf();

	char temp[1024] = { 0 };
	buf.memcpy_out(temp, 1024);

	CY_LOG(L_INFO, "receive:%s", temp);
}

//-------------------------------------------------------------------------------------
void on_close_callback(TcpClient* client)
{
	(void)client;

	CY_LOG(L_INFO, "socket close");
	exit(0);
}

//-------------------------------------------------------------------------------------
static bool _on_command(Looper::event_id_t, socket_t, Looper::event_t, void* param)
{
	client_s* data = (client_s*)param;

	size_t len;
	//read cmd
	if (sizeof(len) != data->cmd_pipe.read((char*)&len, sizeof(len)))
	{//error
		return false;
	}

	char message[MAX_MESSAGE_LEN];
	if ((ssize_t)len != data->cmd_pipe.read(message, len))
	{//error
		return false;
	}

	data->client->send(message, len);

	return false;
}

//-------------------------------------------------------------------------------------
void _client_thread(void* param)
{
	client_s* data = (client_s*)param;

	Looper* looper = Looper::create_looper();
	TcpClient client(looper);
	Address address(data->server_ip, data->port);

	data->client = &client;

	client.set_connection_callback(on_connection_callback);
	client.set_message_callback(on_message_callback);
	client.set_close_callback(on_close_callback);

	looper->register_event(data->cmd_pipe.get_read_port(), Looper::kRead, param, _on_command, 0);

	client.connect(address, 1000 * 3);

	looper->loop();
}

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	const char* server_ip = "127.0.0.1";
	if (argc > 1)
		server_ip = argv[1];

	uint16_t server_port = 1978;
	if (argc > 2)
		server_port = (uint16_t)atoi(argv[2]);

	client_s client;
	client.server_ip = server_ip;
	client.port = server_port;

	cyclone::thread_api::thread_create(_client_thread, &client);

	for (;;)
	{
		char temp[1024] = { 0 };
		char* str_pos = temp+sizeof(size_t);
		scanf("%s", str_pos);

		if (str_pos[0] == 0) break;

		size_t len = strlen(str_pos) + 1;
		if (len >= MAX_MESSAGE_LEN) continue;

		memcpy(temp, &len, sizeof(len));
		client.cmd_pipe.write(temp, len+sizeof(len));
	}
	
	return 0;
}

