/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_socket_api.h"

#ifdef CY_SYS_WINDOWS
#include <io.h>
#else
#include <netdb.h>
#include <netinet/tcp.h>
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
socket_t create_socket(bool udp)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
#endif

	socket_t sockfd = ::socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, udp ? IPPROTO_UDP : IPPROTO_TCP);
	if (sockfd == INVALID_SOCKET)
	{
		CY_LOG(L_FATAL, "socket_api::create_socket, err=%d", get_lasterror());
	}

	return sockfd;
}

//-------------------------------------------------------------------------------------
void close_socket(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if (SOCKET_ERROR == ::closesocket(s))
#else
	if (SOCKET_ERROR == ::close(s))
#endif
	{
		CY_LOG(L_ERROR, "socket_api::close_socket, err=%d", get_lasterror());
	}
}

//-------------------------------------------------------------------------------------
bool set_nonblock(socket_t s, bool enable)
{
#ifdef CY_SYS_WINDOWS
	//set socket to nonblock
	unsigned long nonblock = enable ? 1 : 0;
	if (::ioctlsocket(s, FIONBIO, &nonblock) != 0)
#else
	int32_t flags = ::fcntl(s, F_GETFL, 0);
	if (SOCKET_ERROR == flags ||
		SOCKET_ERROR == ::fcntl(s, F_SETFL, enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)))
#endif
	{
		CY_LOG(L_FATAL, "socket_api::set_nonblock, err=%d", get_lasterror());
		return false;
	}

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
	if (SOCKET_ERROR == flags ||
		SOCKET_ERROR == ::fcntl(s, F_SETFD, enable ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC)))
	{
		CY_LOG(L_FATAL, "socket_api::set_close_onexec, err=%d", get_lasterror());
		return false;
	}
#endif
	return true;
}

//-------------------------------------------------------------------------------------
bool bind(socket_t s, const struct sockaddr_in& addr)
{
	if (SOCKET_ERROR == ::bind(s, (const sockaddr*)(&addr), static_cast<socklen_t>(sizeof addr)))
	{
		CY_LOG(L_FATAL, "socket_api::bind, err=%d", get_lasterror());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool listen(socket_t s)
{
	if(SOCKET_ERROR == ::listen(s, SOMAXCONN))
	{
		CY_LOG(L_FATAL, "socket_api::listen, err=%d", get_lasterror());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool connect(socket_t s, const struct sockaddr_in& addr)
{
	if (::connect(s, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		int lasterr = get_lasterror();
#ifdef CY_SYS_WINDOWS
		//non-block socket
		if(lasterr == WSAEWOULDBLOCK) return true; 
#else
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
		CY_LOG(L_FATAL, "socket_api::inet_pton, err=%d", get_lasterror());
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
		CY_LOG(L_FATAL, "socket_api::inet_ntop, err=%d", get_lasterror());
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
ssize_t sendto(socket_t s, const char* buf, size_t len, const struct sockaddr_in& peer_addr)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = ::sendto(s, buf, (int32_t)len, 0, (struct sockaddr*)&peer_addr, (int32_t)sizeof(peer_addr));
#else
	ssize_t _len = ::sendto(s, buf, len, 0, (struct sockaddr*)&peer_addr, (socklen_t)sizeof(peer_addr));
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
ssize_t recvfrom(socket_t s, void* buf, size_t len, struct sockaddr_in& peer_addr)
{
	socklen_t addr_len = sizeof(peer_addr);

#ifdef CY_SYS_WINDOWS
	ssize_t _len = ::recvfrom(s, (char *)buf, (int32_t)len, 0, (struct sockaddr*)&peer_addr, &addr_len);
#else
	ssize_t _len = ::recvfrom(s, (char *)buf, len, 0, (struct sockaddr*)&peer_addr, &addr_len);
#endif
	return _len;
}

//-------------------------------------------------------------------------------------
bool shutdown(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if (SOCKET_ERROR == ::shutdown(s, SD_BOTH))
#else
	if (SOCKET_ERROR == ::shutdown(s, SHUT_RDWR))
#endif
	{
		CY_LOG(L_FATAL, "socket_api::shutdown, err=%d", get_lasterror());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool setsockopt(socket_t s, int level, int optname, const void *optval, size_t optlen)
{
	if (0 != ::setsockopt(s, level, optname, (const char*)optval, (socklen_t)optlen))
	{
		CY_LOG(L_ERROR, "socket_api::setsockopt, level=%d, optname=%d, err=%d", get_lasterror());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool set_reuse_addr(socket_t s, bool on)
{
	int optval = on ? 1 : 0;
	return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

//-------------------------------------------------------------------------------------
bool set_reuse_port(socket_t s, bool on)
{
#ifndef SO_REUSEPORT
	(void)s;
	(void)on;
	//NOT SUPPORT 
	CY_LOG(L_WARN, "socket_api::set_reuse_port, System do NOT support REUSEPORT issue!");
	return false;
#else
	int optval = on ? 1 : 0;
	return setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
#endif
}

//-------------------------------------------------------------------------------------
bool set_keep_alive(socket_t s, bool on)
{
	int optval = on ? 1 : 0;
	return setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

//-------------------------------------------------------------------------------------
bool set_nodelay(socket_t s, bool on)
{
	int optval = on ? 1 : 0;
	return setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

//-------------------------------------------------------------------------------------
bool set_linger(socket_t s, bool on, uint16_t linger_time)
{
	struct linger linger_;

	linger_.l_onoff = on; // && (linger_time > 0)) ? 1 : 0;
	linger_.l_linger = on ? linger_time : 0;

	return setsockopt(s, SOL_SOCKET, SO_LINGER, &linger_, sizeof(linger_));
}

//-------------------------------------------------------------------------------------
int get_socket_error(socket_t sockfd)
{
	int optval;
	socklen_t optlen = sizeof(optval);

	if (SOCKET_ERROR == ::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen))
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
		CY_LOG(L_FATAL, "socket_api::accept, err=%d", get_lasterror());
	}
	return connfd;
}

//-------------------------------------------------------------------------------------
bool getsockname(socket_t s, struct sockaddr_in& addr)
{
	/*
		https://docs.microsoft.com/zh-cn/windows/win32/api/mswsock/nf-mswsock-acceptex?redirectedfrom=MSDN
		The buffer size for the local and remote address must be 16 bytes more than the size of the sockaddr structure for
		the transport protocol in use because the addresses are written in an internal format.
		For example, the size of a sockaddr_in (the address structure for TCP/IP) is 16 bytes.
		Therefore, a buffer size of at least 32 bytes must be specified for the local and remote addresses.
	*/
	const socklen_t ADDR_BUF_SIZE_NEEDED = static_cast<socklen_t>(sizeof(sockaddr_in) + 64);
	char addr_buf[ADDR_BUF_SIZE_NEEDED] = { 0 };
	socklen_t addr_buf_size = ADDR_BUF_SIZE_NEEDED;

	if (SOCKET_ERROR == ::getsockname(s, (struct sockaddr*)addr_buf, &addr_buf_size))
	{
		CY_LOG(L_FATAL, "socket_api::getsockname, err=%d", get_lasterror());
		return false;
	}
	memcpy(&addr, addr_buf, sizeof(sockaddr_in));
	return true;
}

//-------------------------------------------------------------------------------------
bool getpeername(socket_t s, struct sockaddr_in& addr)
{
	const socklen_t ADDR_BUF_SIZE_NEEDED = static_cast<socklen_t>(sizeof(sockaddr_in) + 64);
	char addr_buf[ADDR_BUF_SIZE_NEEDED] = { 0 };
	socklen_t addr_buf_size = ADDR_BUF_SIZE_NEEDED;

	if (SOCKET_ERROR == ::getpeername(s, (struct sockaddr*)addr_buf, &addr_buf_size) )
	{
		CY_LOG(L_FATAL, "socket_api::getpeername, err=%d", get_lasterror());
		return false;
	}

	memcpy(&addr, addr_buf, sizeof(sockaddr_in));
	return true;
}

//-------------------------------------------------------------------------------------
bool resolve_hostname(const char* hostname, struct sockaddr_in& addr)
{
	socket_api::global_init();

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *result = nullptr;
	if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
		CY_LOG(L_ERROR, "socket_api::resolve %s failed, errno=%d", hostname, socket_api::get_lasterror());
		return false;
	}

	bool find_ipv4_address = false;
	for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		if (ptr->ai_family != AF_INET) continue;

		//AF_INET (IPv4)
		addr.sin_addr = ((struct sockaddr_in*) ptr->ai_addr)->sin_addr;
		find_ipv4_address = true;
		break;
	}

	if (!find_ipv4_address) {
		CY_LOG(L_ERROR, "socket_api::resolve %s failed, IPV4 address not found", hostname);
	}

	freeaddrinfo(result);
	return find_ipv4_address;
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

//-------------------------------------------------------------------------------------
bool check_connect(const struct sockaddr_in& addr, int timeout_ms)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
#endif

	// create socket
	socket_t s = create_socket();
	if (s == INVALID_SOCKET) return false;

	// set to non block mode
	if (!set_nonblock(s, true)) return false;

	// begin connect...
	if (!connect(s, addr)) {
		//socket error
		close_socket(s); 
		return false;
	}

	// wait connect result
	fd_set set_write, set_err;

	FD_ZERO(&set_write);
	FD_SET(s, &set_write);

	FD_ZERO(&set_err);
	FD_SET(s, &set_err);

	// check if the socket is ready
	timeval time_out = { timeout_ms/1000, (timeout_ms%1000) * 1000};
	int ret = ::select(
#ifdef CY_SYS_WINDOWS
		0, 
#else
		s+1,
#endif
		nullptr, &set_write, &set_err, timeout_ms<0 ? nullptr : &time_out);

	if (ret <= 0) {
		//time out or socket error
		close_socket(s);
		return false;
	}

	if (!FD_ISSET(s, &set_write)) {
		//socket not ready
		close_socket(s);
		return false;
	}

	int err = get_socket_error(s);
	if (err != 0) {
		//socket error
		close_socket(s);
		return false;
	}

	//OK!
	close_socket(s);
	return true;
}

}
}