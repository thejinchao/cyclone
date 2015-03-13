/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_SOCKET_API_H_
#define _CYCLONE_CORE_SOCKET_API_H_

#include <cyclone_config.h>

namespace cyclone
{
namespace socket_api
{

/// Creates a non-blocking socket file descriptor,
socket_t create_non_blocking_socket(void);

/// Creates a blocking socket file descriptor,
socket_t create_blocking_socket(void);

/// Close socket
void close_socket(socket_t s);

bool bind(socket_t s, const struct sockaddr_in& addr);
bool listen(socket_t s);
socket_t accept(socket_t s, struct sockaddr_in& addr);
bool connect(socket_t s, const struct sockaddr_in& addr);
ssize_t write(socket_t s, const char* buf, size_t len);
ssize_t read(socket_t s, void *buf, size_t len);
bool shutdown(socket_t s);

/// convert ip address (like "123.123.123.123") to in_addr
bool inet_pton(const char* ip, struct in_addr& a);
/// convert in_addr to ip address
bool inet_ntop(const struct in_addr& a, char *dst, socklen_t size);
/// socket operation
bool setsockopt(socket_t s, int level, int optname, const void *optval, size_t optlen);

bool getsockname(socket_t s, struct sockaddr_in& addr);

/// big-endian/little-endian convert
uint16_t ntoh_16(uint16_t x);
uint32_t ntoh_32(uint32_t x);

}
}

#endif
