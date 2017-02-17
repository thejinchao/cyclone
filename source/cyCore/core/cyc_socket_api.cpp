/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_socket_api.h"

#ifdef CY_SYS_WINDOWS
#include <io.h>
#endif
#include <fcntl.h>

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
void global_init(void)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
#endif
}

//-------------------------------------------------------------------------------------
socket_t create_socket(void)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
#endif

	socket_t sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == INVALID_SOCKET)
	{
		CY_LOG(L_FATAL, "socket_api::create_socket");
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
bool set_nonblock(socket_t s, bool enable)
{
#ifdef CY_SYS_WINDOWS
	//set socket to nonblock
	unsigned long nonblock = enable ? 1 : 0;
	if (::ioctlsocket(s, FIONBIO, &nonblock) != 0)
	{
		CY_LOG(L_FATAL, "socket_api::set_nonblock");
		return false;
	}
#else
	int32_t flags = ::fcntl(s, F_GETFL, 0);
	if(flags==-1 ||
		::fcntl(s, F_SETFL, enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK))==-1)
	{
		CY_LOG(L_FATAL, "socket_api::set_nonblock");
		return false;
	}
#endif

	return true;
}

//-------------------------------------------------------------------------------------
bool set_close_onexec(socket_t s, bool enable)
{
#ifdef CY_SYS_WINDOWS
	(void)s;
	(void)enable;
	//NOT SUPPORT (I'm not sure...)
#else
	// close-on-exec
	int32_t flags = ::fcntl(s, F_GETFD, 0);
	if(flags==-1 ||
		::fcntl(s, F_SETFD, enable ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC))==-1)
	{
		CY_LOG(L_FATAL, "socket_api::set_close_onexec");
		return false;
	}
#endif
	return true;
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
	{
		int lasterr = get_lasterror();
		//non-block socket
		if(lasterr == WSAEWOULDBLOCK) return true; 
#else
	if (::connect(s, (const sockaddr*)&addr, sizeof(addr)) <0 )
	{
		int lasterr = get_lasterror();
		//non-block socket
		if (lasterr == EINPROGRESS ||
			lasterr == EINTR ||
			lasterr == EISCONN) return true; 
#endif

		CY_LOG(L_FATAL, "socket_api::connect, err=%d", get_lasterror());
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
ssize_t write(socket_t s, const char* buf, size_t len)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = (ssize_t)::send(s, buf, (int32_t)len, MSG_DONTROUTE);
#else
	ssize_t _len = (ssize_t)::write(s, buf, len);
#endif
	return _len;
}

//-------------------------------------------------------------------------------------
ssize_t read(socket_t s, void *buf, size_t len)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = (ssize_t)::recv(s, (char*)buf, (int32_t)len, 0);
#else
	ssize_t _len = (ssize_t)::read(s, buf, len);
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
bool setsockopt(socket_t s, int level, int optname, const void *optval, size_t optlen)
{
#ifdef CY_SYS_WINDOWS
	if (::setsockopt(s, level, optname, (const char*)optval, (int32_t)optlen) == SOCKET_ERROR)
#else
	if (::setsockopt(s, level, optname, (const char*)optval, (socklen_t)optlen) < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::setsockopt");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
int get_socket_error(socket_t sockfd)
{
	int optval;
	socklen_t optlen = sizeof(optval);

	if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) < 0)
	{
		return errno;
	}
	else
	{
		return optval;
	}
}

//-------------------------------------------------------------------------------------
socket_t accept(socket_t s, struct sockaddr_in* addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof(sockaddr_in));
	socket_t connfd = ::accept(s, (struct sockaddr *)addr, addr ? (&addrlen) : 0);

	if (connfd == INVALID_SOCKET)
	{
		CY_LOG(L_FATAL, "socket_api::accept");
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
bool getpeername(socket_t s, struct sockaddr_in& addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof addr);
	memset(&addr, 0, sizeof addr);

#ifdef CY_SYS_WINDOWS
	if (::getpeername(s, (struct sockaddr*)(&addr), &addrlen) == SOCKET_ERROR)
#else
	if (::getpeername(s, (struct sockaddr*)(&addr), &addrlen) < 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::getpeername");
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
uint16_t ntoh_16(uint16_t x)
{
#ifdef CY_SYS_LINUX
	return be16toh(x);
#else
	return ntohs(x);
#endif
}

//-------------------------------------------------------------------------------------
uint32_t ntoh_32(uint32_t x)
{
#ifdef CY_SYS_LINUX
	return be32toh(x);
#else
	return ntohl(x);
#endif
}

//-------------------------------------------------------------------------------------
int get_lasterror(void)
{
#ifdef CY_SYS_WINDOWS
	return ::WSAGetLastError();
#else
	return errno;
#endif
}

//-------------------------------------------------------------------------------------
bool is_lasterror_WOULDBLOCK(void)
{
	//This error is returned from operations on nonblocking sockets that cannot be completed immediately
#ifdef CY_SYS_WINDOWS
	return get_lasterror() == WSAEWOULDBLOCK;
#else
	return errno==EWOULDBLOCK;
#endif

}

}
}