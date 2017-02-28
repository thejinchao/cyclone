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
bool Socket::bind(const Address& addr)
{
	return socket_api::bind(m_sockfd, addr.get_sockaddr_in());
}

//-------------------------------------------------------------------------------------
void Socket::listen()
{
	socket_api::listen(m_sockfd);
}

//-------------------------------------------------------------------------------------
bool Socket::connect(const Address& addr)
{
	return socket_api::connect(m_sockfd, addr.get_sockaddr_in());
}

//-------------------------------------------------------------------------------------
bool Socket::set_reuse_addr(bool on)
{
	return socket_api::set_reuse_addr(m_sockfd, on);
}

//-------------------------------------------------------------------------------------
bool Socket::set_reuse_port(bool on)
{
	return socket_api::set_reuse_port(m_sockfd, on);
}

//-------------------------------------------------------------------------------------
bool Socket::set_keep_alive(bool on)
{
	return socket_api::set_keep_alive(m_sockfd, on);
}

//-------------------------------------------------------------------------------------
bool Socket::set_linger(bool on, uint16_t linger_time)
{
	return socket_api::set_linger(m_sockfd, on, linger_time);
}

}

