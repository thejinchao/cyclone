#include "s5_protocol.h"

//-------------------------------------------------------------------------------------
void s5_build_handshake_act(RingBuf& outputBuf)
{
	uint8_t socks5_version_ack[2];

	socks5_version_ack[0] = S5_VERSION;
	socks5_version_ack[1] = S5_METHOD_NO_AUTH;
	outputBuf.memcpy_into(socks5_version_ack, sizeof(socks5_version_ack));
}

//-------------------------------------------------------------------------------------
void s5_build_connect_act(RingBuf& outputBuf, uint8_t reply, const Address& address)
{
	//head
	uint8_t temp[4] = { S5_VERSION, reply, 0, 0x01 };
	outputBuf.memcpy_into(temp, 4);

	//address
	const sockaddr_in& addr = address.get_sockaddr_in();
	outputBuf.memcpy_into(&addr.sin_addr.s_addr, sizeof(addr.sin_addr.s_addr));
	outputBuf.memcpy_into(&addr.sin_port, sizeof(addr.sin_port));
}

//-------------------------------------------------------------------------------------
int32_t s5_get_handshake(RingBuf& inputBuf)
{
	const size_t max_peek_size = 260;

	uint8_t temp[max_peek_size];
	size_t peek_size = inputBuf.peek(0, temp, max_peek_size);
	if (peek_size < 3) return S5ERR_NEED_MORE_DATA;

	//check version
	if (temp[0] != S5_VERSION) return S5ERR_WRONG_VERSION;

	//check methods
	uint8_t nmethods = temp[1];
	if (peek_size < (size_t)(nmethods + 2)) return S5ERR_NEED_MORE_DATA;
	if (peek_size > (size_t)(nmethods + 2)) return S5ERR_PROTOCOL_ERR;

	for (uint8_t i = 0; i < nmethods; i++) {
		if (temp[i + 2] == S5_METHOD_NO_AUTH) {
			//handshake ok
			inputBuf.discard(peek_size);
			return S5ERR_SUCCESS;
		}
	}
	return S5ERR_WRONG_METHOD;
}

//-------------------------------------------------------------------------------------
int32_t s5_get_connect_request(RingBuf& inputBuf, Address& address, std::string& domain)
{
	char head[4] = { 0 };
	size_t peek_size = inputBuf.peek(0, head, 4);
	if (peek_size < 4) return S5ERR_NEED_MORE_DATA;

	//check version
	if (head[0] != S5_VERSION) return S5ERR_WRONG_VERSION;
	//check cmd
	if (head[1] != S5_CMD_CONNECT) return S5ERR_NOTSUPPORT;

	//switch ATYP 
	switch (head[3]) {
	case 0x01:	//IP V4 address: X'01'
	{
		if (inputBuf.size() < 10) return S5ERR_NEED_MORE_DATA;
		if (inputBuf.size() > 10) return S5ERR_PROTOCOL_ERR;

		inputBuf.discard(4);

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));

		addr.sin_family = AF_INET;
		inputBuf.memcpy_out(&(addr.sin_addr.s_addr), 4);
		inputBuf.memcpy_out(&(addr.sin_port), 2);

		address = Address(addr);
		domain = address.get_ip();
	}
	break;

	case 0x03:	//DOMAINNAME: X'03'
	{
		uint8_t domain_length;
		if (sizeof(uint8_t) != inputBuf.peek(4, &domain_length, sizeof(uint8_t))) return S5ERR_NEED_MORE_DATA;
		if (inputBuf.size() < (size_t)(7 + domain_length)) return S5ERR_NEED_MORE_DATA;
		if (inputBuf.size() > (size_t)(7 + domain_length)) return S5ERR_PROTOCOL_ERR;

		inputBuf.discard(5);

		char domain_name[260] = { 0 };
		inputBuf.memcpy_out(domain_name, domain_length);
		
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));

		addr.sin_family = AF_INET;
		inputBuf.memcpy_out(&(addr.sin_port), 2);

		//resolve
		if (!socket_api::resolve_hostname(domain_name, addr)) return S5ERR_DNS_FAILED;
		address = Address(addr);
		domain = domain_name;
	}
	break;

	default:	//IP V6 address: X'04'(not supported)
		return S5ERR_NOTSUPPORT;
		break;
	}

	return S5ERR_SUCCESS;
}
