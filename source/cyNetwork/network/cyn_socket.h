/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_SOCKET_H_
#define _CYCLONE_NETWORK_SOCKET_H_

#include <cyclone_config.h>
#include <network/cyn_address.h>

namespace cyclone
{
namespace network
{

class Socket
{
public:
	//----------------------
	// operation api
	//----------------------

	/// get native handle
	socket_t get_fd(void) const { return m_sockfd; }
	void bind(const Address& addr);
	void listen(void);
	bool connect(const Address& addr);
	socket_t accept(Address& peer_addr);

	//----------------------
	// socket option api
	//----------------------
	/// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
	void set_tcp_no_delay(bool on);
	/// Enable/disable SO_REUSEADDR
	void set_reuse_addr(bool on);
	/// Enable/disable SO_REUSEPORT
	void set_reuse_port(bool on);
	/// Enable/disable SO_KEEPALIVE
	void set_keep_alive(bool on);

public:
	Socket(socket_t fd);
	~Socket();

private:
	const socket_t m_sockfd;	//native socket fd
};

}
}
#endif
