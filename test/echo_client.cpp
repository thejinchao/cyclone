#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

using namespace cyclone;

//-------------------------------------------------------------------------------------
struct client_s {
	const char* server_ip;
	uint16_t port;
	TcpClient* client;
};

#define MAX_MESSAGE_LEN (256)

class ClientListener : public TcpClient::Listener
{
public:
	//-------------------------------------------------------------------------------------
	virtual uint32_t on_connection_callback(TcpClient* client, bool success)
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
	virtual void on_message_callback(TcpClient* /*client*/, Connection* conn)
	{
		RingBuf& buf = conn->get_input_buf();

		char temp[1024] = { 0 };
		buf.memcpy_out(temp, 1024);

		CY_LOG(L_INFO, "receive:%s", temp);
	}

	//-------------------------------------------------------------------------------------
	virtual void on_close_callback(TcpClient* client)
	{
		(void)client;

		CY_LOG(L_INFO, "socket close");
		exit(0);
	}
};

//-------------------------------------------------------------------------------------
void _client_thread(void* param)
{
	client_s* data = (client_s*)param;
	ClientListener listener;
	Looper* looper = Looper::create_looper();
	TcpClient client(looper, &listener, data);
	Address address(data->server_ip, data->port);

	data->client = &client;

	client.connect(address, 1000 * 10);

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

	cyclone::sys_api::thread_create(_client_thread, &client, "client");

	for (;;)
	{
		char temp[1024] = { 0 };
		char* str_pos = temp+sizeof(size_t);
		scanf("%s", str_pos);

		if (str_pos[0] == 0) break;

		size_t len = strlen(str_pos) + 1;
		if (len >= MAX_MESSAGE_LEN) continue;

		client.client->send(str_pos, len);
	}
	
	return 0;
}

