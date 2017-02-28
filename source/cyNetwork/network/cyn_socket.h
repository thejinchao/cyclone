/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_NETWORK_SOCKET_H_
#define _CYCLONE_NETWORK_SOCKET_H_

#include <cyclone_config.h>
#include <network/cyn_address.h>

namespace cyclone
{

class Socket
{
public:
	//----------------------
	// operation api
	//----------------------

	/// get native handle
	socket_t get_fd(void) const { return m_sockfd; }
	bool bind(const Address& addr);
	void listen(void);
	bool connect(const Address& addr);

	//----------------------
	// socket option api
	//----------------------
	/// Enable/disable SO_REUSEADDR
	bool set_reuse_addr(bool on);
	/// Enable/disable SO_REUSEPORT
	bool set_reuse_port(bool on);
	/// Enable/disable SO_KEEPALIVE
	bool set_keep_alive(bool on);
	/// Set socket SO_LINGER
	bool set_linger(bool on, uint16_t linger_time);

public:
	Socket(socket_t fd);
	~Socket();

private:
	const socket_t m_sockfd;	//native socket fd

private:
	//not-copyable
	Socket & operator=(const Socket &) { return *this; }
};

}

#endif
