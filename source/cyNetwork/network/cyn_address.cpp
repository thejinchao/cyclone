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

	socket_api::inet_ntop(m_address.sin_addr, m_ip_string, IP_ADDRESS_LEN);
}

//-------------------------------------------------------------------------------------
Address::Address(const char* ip, uint16_t port)
{
	memset(&m_address, 0, sizeof m_address);
	m_address.sin_family = AF_INET;
	m_address.sin_port = htons(port);
	socket_api::inet_pton(ip, m_address.sin_addr);

	socket_api::inet_ntop(m_address.sin_addr, m_ip_string, IP_ADDRESS_LEN);
}

//-------------------------------------------------------------------------------------
Address::Address(const struct sockaddr_in& addr)
	: m_address(addr)
{ 
	socket_api::inet_ntop(m_address.sin_addr, m_ip_string, IP_ADDRESS_LEN);
}

//-------------------------------------------------------------------------------------
Address::Address(const Address& other)
{
	memcpy(&m_address, &(other.m_address), sizeof(m_address));
	memcpy(&m_ip_string, other.m_ip_string, IP_ADDRESS_LEN);
}

//-------------------------------------------------------------------------------------
Address& Address::operator=(const Address& other)
{
	memcpy(&m_address, &(other.m_address), sizeof(m_address));
	memcpy(&m_ip_string, other.m_ip_string, IP_ADDRESS_LEN);
	return *this;
}

//-------------------------------------------------------------------------------------
Address::Address(bool peer, socket_t sfd)
{
	if (peer)
		socket_api::getpeername(sfd, m_address);
	else
		socket_api::getsockname(sfd, m_address);
	socket_api::inet_ntop(m_address.sin_addr, m_ip_string, IP_ADDRESS_LEN);
}

//-------------------------------------------------------------------------------------
Address::Address()
{
	memset(&m_address, 0, sizeof m_address);
	memset(m_ip_string, 0, IP_ADDRESS_LEN);
}

//-------------------------------------------------------------------------------------
const char* Address::get_ip(void) const
{
	return m_ip_string;
}

//-------------------------------------------------------------------------------------
uint16_t Address::get_port(void) const
{
	return socket_api::ntoh_16(m_address.sin_port);
}

//-------------------------------------------------------------------------------------
uint32_t Address::hash_value(const sockaddr_in& addr)
{
	const uint32_t FNV_offset_basis = 2166136261;
	const uint32_t FNV_prime = 16777619;

	uint32_t hash = FNV_offset_basis;

	//hash address
	const uint8_t* v = (const uint8_t*)&(addr.sin_addr);
	for (size_t i = 0; i < 4; i++) {
		hash = hash ^ v[0];
		hash = hash * FNV_prime;
	}
	//hash port
	v = (const uint8_t*)&(addr.sin_port);
	for (size_t i = 0; i < 2; i++) {
		hash = hash ^ v[0];
		hash = hash * FNV_prime;
	}
	return hash;
}

}
