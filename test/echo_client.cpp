#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

using namespace cyclone;
using namespace cyclone::network;

int main(int argc, char* argv[])
{
	Socket socket(socket_api::create_socket_ex(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	Address address("127.0.0.1", 1978);

	socket.connect(address);

	while (true)
	{
		printf("input:");
		char temp[1024] = { 0 };
		scanf("%s", temp);

		if (temp[0] == 0) break;

		socket_api::write(socket.get_fd(), temp, (int)strlen(temp));

		int len=0;
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

