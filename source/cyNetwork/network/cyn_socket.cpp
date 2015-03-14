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
void Socket::set_reuse_addr(bool on)
{
	int optval = on ? 1 : 0;
	bool success = socket_api::setsockopt(m_sockfd, SOL_SOCKET, 
		SO_REUSEADDR,
		&optval, static_cast<socklen_t>(sizeof optval));

	if (!success && on)
	{
		//log error set SO_REUSEPORT failed
		CY_LOG(L_FATAL, "set SO_REUSEPORT failed[%s]", on ? "ON" : "OFF");
	}
}

//-------------------------------------------------------------------------------------
void Socket::set_reuse_port(bool on)
{
#ifdef CY_SYS_WINDOWS
	(void)on;
	//NOT SUPPORT 
#else
	int optval = on ? 1 : 0;
	bool success = socket_api::setsockopt(m_sockfd, SOL_SOCKET, 
		SO_REUSEPORT,
		&optval, static_cast<socklen_t>(sizeof optval));

	if (!success && on)
	{
		//log error set SO_REUSEPORT failed; 
		//this option avaliable in linux 3.9
		CY_LOG(L_FATAL, "set SO_REUSEPORT failed[%s]", on ? "ON" : "OFF");
	}
#endif
}

//-------------------------------------------------------------------------------------
void Socket::set_keep_alive(bool on)
{
	int optval = on ? 1 : 0;
	bool success = socket_api::setsockopt(m_sockfd, 
		SOL_SOCKET, 
		SO_KEEPALIVE,
		&optval, static_cast<socklen_t>(sizeof optval));

	if (!success && on)
	{
		//log error set SO_KEEPALIVE failed.;
		CY_LOG(L_FATAL, "set SO_KEEPALIVE failed[%s]", on ? "ON" : "OFF");
	}
}

//-------------------------------------------------------------------------------------
void Socket::set_linger(bool on, uint16_t linger_time)
{
	struct linger linger_;

	linger_.l_onoff = on; // && (linger_time > 0)) ? 1 : 0;
	linger_.l_linger = on ? linger_time : 0;

	if (!socket_api::setsockopt(m_sockfd, SOL_SOCKET, SO_LINGER, &linger_, sizeof(linger_)))
	{
		CY_LOG(L_ERROR, "set SO_LINGER failed[%s]", on ? "ON" : "OFF");
	}
}

}

