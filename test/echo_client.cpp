#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

using namespace cyclone;

int main(int argc, char* argv[])
{
	const char* server_ip = "127.0.0.1";
	if (argc > 1)
		server_ip = argv[1];

	uint16_t server_port = 1978;
	if (argc > 2)
		server_port = (uint16_t)atoi(argv[2]);

	Socket socket(socket_api::create_socket());
	Address address(server_ip, server_port);

	bool success = socket.connect(address);
	CY_LOG(L_DEBUG, "connect to %s:%d %s.", server_ip, server_port, success ? "OK" : "FAILED");
	if (!success) return 1;

	while (true)
	{
		printf("input:");
		char temp[1024] = { 0 };
		scanf("%s", temp);

		if (temp[0] == 0) break;

		socket_api::write(socket.get_fd(), temp, (int)strlen(temp));

		ssize_t len=0;
		do {
			len = socket_api::read(socket.get_fd(), temp, 1024);
			if (len <= 0) break;

			temp[len] = 0;
			printf("recv:%s", temp);

			//break;
			if (temp[len - 1] == '\n') break;
		} while (true);

		if (len <= 0) break;
	}

	return 0;
}

