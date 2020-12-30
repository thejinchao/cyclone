/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{
namespace socket_api
{

//// init global socket data( call WSAStartup at windows system)
void global_init(void);

/// Creates a blocking socket file descriptor, return INVALID_SOCKET if failed
socket_t create_socket(bool udp=false);

/// Close socket
void close_socket(socket_t s);

/// enable/disable socket non-block mode
bool set_nonblock(socket_t s, bool enable);

/// enable/disable socket close-on-exec mode
bool set_close_onexec(socket_t s, bool enable);

/// bind a local name to a socket
bool bind(socket_t s, const struct sockaddr_in& addr);

/// listen for connection
bool listen(socket_t s);

/// accept a connection on a socket, return INVALID_SOCKET if failed
socket_t accept(socket_t s, struct sockaddr_in* addr);

/// initiate a connection on a socket
bool connect(socket_t s, const struct sockaddr_in& addr);

/// write to socket file desc
ssize_t write(socket_t s, const char* buf, size_t len);

/// send data to socket file desc
ssize_t sendto(socket_t s, const char* buf, size_t len, const struct sockaddr_in& peer_addr);

/// read from a socket file desc
ssize_t read(socket_t s, void *buf, size_t len);

/// receive from socket
ssize_t recvfrom(socket_t s, void* buf, size_t len, struct sockaddr_in& peer_addr);

/// shutdown read and write part of a socket connection
bool shutdown(socket_t s);

/// convert ip address (like "123.123.123.123") to in_addr
bool inet_pton(const char* ip, struct in_addr& a);

/// convert in_addr to ip address
bool inet_ntop(const struct in_addr& a, char *dst, socklen_t size);

/// socket operation
bool setsockopt(socket_t s, int level, int optname, const void *optval, size_t optlen);

/// Enable/disable SO_REUSEADDR
bool set_reuse_addr(socket_t s, bool on);

/// Enable/disable SO_REUSEPORT
bool set_reuse_port(socket_t s, bool on);

/// Enable/disable SO_KEEPALIVE
bool set_keep_alive(socket_t s, bool on);

/// Enable/disable TCP_NODELAY
bool set_nodelay(socket_t s, bool on);

/// Set socket SO_LINGER
bool set_linger(socket_t s, bool on, uint16_t linger_time);

/// get socket error
int get_socket_error(socket_t sockfd);

/// get local name of socket
bool getsockname(socket_t s, struct sockaddr_in& addr);

/// get peer name of socket
bool getpeername(socket_t s, struct sockaddr_in& addr);

///resolve hostname to IP address, not changing port or sin_family
bool resolve_hostname(const char* hostname, struct sockaddr_in& addr);

/// big-endian/little-endian convert
uint16_t ntoh_16(uint16_t x);
uint32_t ntoh_32(uint32_t x);

//// get last socket error
int get_lasterror(void);

//// is lasterror  WOULDBLOCK
bool is_lasterror_WOULDBLOCK(void);

//// tests if the remote address is connectable(timeout : max time for wait, milli second, -1 means block mode)
bool check_connect(const struct sockaddr_in& addr, int timeout_ms);

}
}
