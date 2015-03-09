/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_network.h>
#include "cyn_address.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Address::Address(uint16_t port, bool loopbackOnly)
{
	memset(&m_address, 0, sizeof m_address);
	m_address.sin_family = AF_INET;
	m_address.sin_addr.s_addr = loopbackOnly ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
	m_address.sin_port = htons(port);
}

//-------------------------------------------------------------------------------------
Address::Address(const char* ip, uint16_t port)
{
	memset(&m_address, 0, sizeof m_address);
	m_address.sin_family = AF_INET;
	m_address.sin_port = htons(port);
	socket_api::inet_pton(ip, m_address.sin_addr);
}

//-------------------------------------------------------------------------------------
Address::Address(const Address& other)
{
	memcpy(&m_address, &other, sizeof(m_address));
}

//-------------------------------------------------------------------------------------
Address::Address(socket_t sfd)
{
	socket_api::getsockname(sfd, m_address);
}

//-------------------------------------------------------------------------------------
const std::string Address::get_ip(void) const
{
	char buf[32] = { 0 };
	socket_api::inet_ntop(m_address.sin_addr, buf, 32);
	return std::string(buf);
}

//-------------------------------------------------------------------------------------
uint16_t Address::get_port(void) const
{
	return socket_api::ntoh_16(m_address.sin_port);
}

//-------------------------------------------------------------------------------------
std::string Address::get_ip_port(void) const
{
	const size_t size = 64;
	char buf[size] = { 0 };
	socket_api::inet_ntop(m_address.sin_addr, buf, size);

	size_t end = ::strlen(buf);
	uint16_t port = socket_api::ntoh_16(m_address.sin_port);
	snprintf(buf + end, size - end, ":%u", port);

	return std::string(buf);
}

}
