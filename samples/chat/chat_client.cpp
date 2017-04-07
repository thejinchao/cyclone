#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

#include "chat_message.h"

#include <iostream>

using namespace cyclone;

//-------------------------------------------------------------------------------------
class ClientListener : public TcpClient::Listener
{
public:
	//-------------------------------------------------------------------------------------
	virtual uint32_t on_connected(TcpClient* client, ConnectionPtr conn, bool success)
	{
		(void)conn;

		CY_LOG(L_DEBUG, "connect to %s:%d %s.",
			client->get_server_address().get_ip(),
			client->get_server_address().get_port(),
			success ? "OK" : "FAILED");

		if (success){
			m_client = client;
			sys_api::signal_notify(m_connected_signal);
			return 0;
		}
		else
		{
			uint32_t retry_time = 1000 * 5;
			CY_LOG(L_INFO, "connect failed!, retry after %d milliseconds...\n", retry_time);
			return 1000 * 5;
		}
	}

	//-------------------------------------------------------------------------------------
	virtual void on_message(TcpClient* client, ConnectionPtr conn)
	{
		(void)client;

		RingBuf& buf = conn->get_input_buf();

		Packet packet;
		packet.build(PACKET_HEAD_SIZE, buf);

		char temp[1024] = { 0 };
		memcpy(temp, packet.get_packet_content(), packet.get_packet_size());

		CY_LOG(L_INFO, "%s", temp);
	}

	//-------------------------------------------------------------------------------------
	virtual void on_close(TcpClient* client)
	{
		(void)client;

		CY_LOG(L_INFO, "socket close");
		exit(0);
	}
	
public:
	void wait_connected(void) { sys_api::signal_wait(m_connected_signal); }
	TcpClient* get_client(void) { return m_client; }

private:
	TcpClient* m_client;
	sys_api::signal_t m_connected_signal;

public:
	ClientListener() 
		: m_client(nullptr)
		, m_connected_signal(sys_api::signal_create())
	{
	}

	~ClientListener()
	{
		sys_api::signal_destroy(m_connected_signal);
	}
};

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	const char* server_ip = "127.0.0.1";
	if (argc > 1)
		server_ip = argv[1];

	uint16_t server_port = 1978;
	if (argc > 2)
		server_port = (uint16_t)atoi(argv[2]);

	ClientListener client_listener;

	//client thread
	sys_api::thread_create_detached([&] (void*){
		Looper* looper = Looper::create_looper();

		TcpClient client(looper, &client_listener, 0);
		client.connect(Address(server_ip, server_port));

		looper->loop();

		Looper::destroy_looper(looper);
	}, 0, "client");

	CY_LOG(L_DEBUG, "connect to %s:%d...", server_ip, server_port);

	//wait connect completed
	client_listener.wait_connected();

	//input
	char line[MAX_CHAT_LENGTH + 1];
	while (std::cin.getline(line, MAX_CHAT_LENGTH + 1))
	{
		if (line[0] == 0) continue;

		Packet packet;
		packet.build(PACKET_HEAD_SIZE, 0, (uint16_t)strlen(line), line);

		client_listener.get_client()->send(packet.get_memory_buf(), packet.get_memory_size());
	}

	return 0;
}

