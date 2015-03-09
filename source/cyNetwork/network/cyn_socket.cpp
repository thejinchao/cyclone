/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_network.h>
#include "cyn_socket.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Socket::Socket(socket_t fd)
	: m_sockfd(fd)
{
}

//-------------------------------------------------------------------------------------
Socket::~Socket()
{
	socket_api::close_socket(m_sockfd);
}

//-------------------------------------------------------------------------------------
void Socket::bind(const Address& addr)
{
	socket_api::bind(m_sockfd, addr.get_sockaddr_in());
}

//-------------------------------------------------------------------------------------
void Socket::listen()
{
	socket_api::listen(m_sockfd);
}

//-------------------------------------------------------------------------------------
socket_t Socket::accept(Address& peer_addr)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	socket_t connfd = socket_api::accept(m_sockfd, addr);
	if (connfd > 0)
	{
		peer_addr = addr;
	}

	return connfd;
}

//-------------------------------------------------------------------------------------
bool Socket::connect(const Address& addr)
{
	return socket_api::connect(m_sockfd, addr.get_sockaddr_in());
}

//-------------------------------------------------------------------------------------
void Socket::set_reuse_addr(bool on)
{
	int optval = on ? 1 : 0;
	bool success = socket_api::setsockopt(m_sockfd, SOL_SOCKET, 
		SO_REUSEADDR,
		&optval, static_cast<socklen_t>(sizeof optval));

	if (!success && on)
	{
		//TODO: log error set SO_REUSEPORT failed.;
	}
}

//-------------------------------------------------------------------------------------
void Socket::set_reuse_port(bool on)
{
	int optval = on ? 1 : 0;
	bool success = socket_api::setsockopt(m_sockfd, SOL_SOCKET, 
#ifdef CY_SYS_WINDOWS
		SO_REUSEADDR,
#else
		SO_REUSEPORT,
#endif
		&optval, static_cast<socklen_t>(sizeof optval));

	if (!success && on)
	{
		//TODO: log error set SO_REUSEPORT failed.;
	}
}

//-------------------------------------------------------------------------------------
void Socket::set_keep_alive(bool on)
{
	int optval = on ? 1 : 0;
	bool success = socket_api::setsockopt(m_sockfd, 
		SOL_SOCKET, SO_KEEPALIVE,
		&optval, static_cast<socklen_t>(sizeof optval));

	if (!success && on)
	{
		//TODO: log error set SO_KEEPALIVE failed.;
	}
}

}

