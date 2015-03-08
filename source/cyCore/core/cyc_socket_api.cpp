/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_socket_api.h"

#ifdef CY_SYS_WINDOWS
#include <io.h>
#include <fcntl.h>
#endif

//
// Winsock Reference https://msdn.microsoft.com/en-us/library/ms741416(v=vs.85).aspx
//

namespace cyclone
{
namespace socket_api
{

//-------------------------------------------------------------------------------------
#ifdef CY_SYS_WINDOWS
class _auto_init_win_socke_s
{
public:
	_auto_init_win_socke_s() {
		WSADATA wsadata;
		WSAStartup(MAKEWORD(2, 1), &wsadata);
	}
};
static const _auto_init_win_socke_s& AUTO_INIT_WIN_SOCKET()
{
	static _auto_init_win_socke_s s;
	return s;
}
#endif

//-------------------------------------------------------------------------------------
socket_t create_non_blocking_socket(void)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();

	socket_t sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd==INVALID_SOCKET)
#elif defined(CY_SYS_LINUX)
	socket_t sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	if (sockfd < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::create_non_blocking_socket");
		return 0;
	}
#ifdef CY_SYS_WINDOWS
	//set socket to nonblock
	unsigned long nonblock = 1;
	if (::ioctlsocket(sockfd, FIONBIO, &nonblock) != 0)
	{
		CY_LOG(L_FATAL, "socket_api::create_non_blocking_socket");
		close_socket(sockfd);
		return 0;
	}
#endif
	return sockfd;
}

//-------------------------------------------------------------------------------------
socket_t create_socket_ex(int af, int type, int protocol)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();

	socket_t sockfd = ::socket(af, type, protocol);
	if (sockfd == INVALID_SOCKET)
#elif defined(CY_SYS_LINUX)
	socket_t sockfd = ::socket(af, type, protocol);
	if (sockfd < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::create_socket");
		return 0;
	}

	return sockfd;
}

//-------------------------------------------------------------------------------------
void close_socket(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if (SOCKET_ERROR == ::closesocket(s))
#else
	if (::close(s) < 0)
#endif
	{
		CY_LOG(L_ERROR, "socket_api::close_socket");
	}
}

//-------------------------------------------------------------------------------------
bool bind(socket_t s, const struct sockaddr_in& addr)
{
#ifdef CY_SYS_WINDOWS
	if (SOCKET_ERROR == ::bind(s, (const sockaddr*)(&addr), static_cast<int>(sizeof addr)))
#else
	if (::bind(s, (const sockaddr*)(&addr), static_cast<socklen_t>(sizeof addr))<0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::bind");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool listen(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if(::listen(s , SOMAXCONN) == SOCKET_ERROR) 
#else
	if(::listen(s, SOMAXCONN) < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::listen");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool connect(socket_t s, const struct sockaddr_in& addr)
{
#ifdef CY_SYS_WINDOWS
	if (::connect(s, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
#else
	if (::connect(s, (const sockaddr*)&addr, sizeof(addr)) <0 )
#endif
	{
		CY_LOG(L_FATAL, "socket_api::connect");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool inet_pton(const char* ip, struct in_addr& a)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
	if(::InetPton(AF_INET, ip, &a) != 1)
#else
	if (::inet_pton(AF_INET, ip, &a) != 1)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::inet_pton");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool inet_ntop(const struct in_addr& a, char *dst, socklen_t size)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
	in_addr t;
	memcpy(&t, &a, sizeof(t));
	if (::InetNtop(AF_INET, &t, dst, size) == 0)
#else
	if (::inet_ntop(AF_INET, &a, dst, size) == 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::inet_ntop");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
ssize_t write(socket_t s, const char* buf, ssize_t len)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = ::send((int)s, buf, len, MSG_DONTROUTE);
#else
	ssize_t _len = ::write(s, buf, len);
#endif
	return _len;
}

//-------------------------------------------------------------------------------------
ssize_t read(socket_t s, void *buf, ssize_t len)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = ::recv((int)s, (char*)buf, len, 0);
#else
	ssize_t _len = ::read(s, buf, len);
#endif

	return _len;
}

//-------------------------------------------------------------------------------------
bool shutdown(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if (::shutdown(s, SD_BOTH) == SOCKET_ERROR)
#else
	if (::shutdown(s, SHUT_RDWR) < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::shutdown");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool setsockopt(socket_t s, int level, int optname, const void *optval, ssize_t optlen)
{
#ifdef CY_SYS_WINDOWS
	if (::setsockopt(s, level, optname, (const char*)optval, optlen) == SOCKET_ERROR)
#else
	if (::setsockopt(s, level, optname, (const char*)optval, optlen) < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::setsockopt");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
socket_t accept(socket_t s, struct sockaddr_in& addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof addr);

#ifdef CY_SYS_WINDOWS
	socket_t connfd = ::accept(s, (struct sockaddr *)&addr, (int*)&addrlen);
	if (connfd == SOCKET_ERROR)
#else
	socket_t connfd = ::accept4(s, (struct sockaddr*)(&addr),
		&addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (connfd < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::accept");
		return -1;
	}
	return connfd;
}

//-------------------------------------------------------------------------------------
bool getsockname(socket_t s, struct sockaddr_in& addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof addr);
	memset(&addr, 0, sizeof addr);

#ifdef CY_SYS_WINDOWS
	if (::getsockname(s, (struct sockaddr*)(&addr), &addrlen) == SOCKET_ERROR)
#else
	if (::getsockname(s, (struct sockaddr*)(&addr), &addrlen) < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::getsockname");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
uint16_t ntoh_16(uint16_t x)
{
#ifdef CY_SYS_WINDOWS
	return ::ntohs(x);
#else
	return be16toh(x);
#endif
}

//-------------------------------------------------------------------------------------
uint32_t ntoh_32(uint32_t x)
{
#ifdef CY_SYS_WINDOWS
	return ::ntohl(x);
#else
	return be32toh(x);
#endif
}

}
}