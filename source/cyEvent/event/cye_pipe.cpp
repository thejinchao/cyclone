/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_pipe.h"

#include <fcntl.h>

namespace cyclone
{

#ifdef CY_SYS_WINDOWS
//-------------------------------------------------------------------------------------
static bool _construct_pipe_windows(pipe_port_t handles[2])
{	
	//
	//https://trac.transmissionbt.com/browser/trunk/libtransmission/trevent.c
	//
	handles[0] = handles[1] = INVALID_SOCKET;

	socket_t s = socket_api::create_blocking_socket();
	if (s == 0)
	{
		//TODO: log L_FATAL message
		return false;
	}

	struct sockaddr_in serv_addr;
	int len = sizeof(serv_addr);
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(0);
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (!socket_api::bind(s, serv_addr))
	{
		//TODO: log L_FATAL message
		socket_api::close_socket(s);
		return false;
	}
	if(!socket_api::listen(s))
	{
		//TODO: log L_FATAL message
		socket_api::close_socket(s);
		return false;
	}

	//TODO: move getsockname to socket_api
	if (::getsockname(s, (SOCKADDR *)& serv_addr, &len) == SOCKET_ERROR) 
	{
		//TODO: log L_FATAL message
		socket_api::close_socket(s);
		return false;
	}

	if ((handles[1] = socket_api::create_blocking_socket()) == 0)
	{
		//TODO: log L_FATAL message
		socket_api::close_socket(s);
		return false;
	}

	//TODO: move connect to socket_api
	if (::connect(handles[1], (SOCKADDR *)& serv_addr, len) == SOCKET_ERROR)
	{
		//TODO: log L_FATAL message
		socket_api::close_socket(s);
		return false;
	}

	//TODO: move connect to socket_api
	if ((handles[0] = accept(s, (SOCKADDR *)& serv_addr, &len)) == INVALID_SOCKET)
	{
		//TODO: log L_FATAL message
		socket_api::close_socket(handles[1]);
		handles[1] = 0;
		socket_api::close_socket(s);
		return false;
	}
	socket_api::close_socket(s);
	return true;
}

//-------------------------------------------------------------------------------------
static void _destroy_pipe_windows(pipe_port_t handles[2])
{
	socket_api::close_socket(handles[0]);
	handles[0] = 0;

	socket_api::close_socket(handles[1]);
	handles[1] = 0;
}
#endif

//-------------------------------------------------------------------------------------
Pipe::Pipe()
{
#ifdef CY_SYS_WINDOWS
	_construct_pipe_windows(m_pipe_fd);
#else
	::pipe2(m_pipe_fd, O_NONBLOCK | O_CLOEXEC);
#endif
	//TODO: error...
}

//-------------------------------------------------------------------------------------
Pipe::~Pipe()
{
#ifdef CY_SYS_WINDOWS
	_destroy_pipe_windows(m_pipe_fd);
#else
	::close(m_pipe_fd[0]);
	::close(m_pipe_fd[1]);
#endif
}

//-------------------------------------------------------------------------------------
ssize_t Pipe::write(const char* buf, ssize_t len)
{
	return socket_api::write(m_pipe_fd[1], buf, len);
}

//-------------------------------------------------------------------------------------
ssize_t Pipe::read(char* buf, ssize_t len)
{
	return socket_api::read(m_pipe_fd[0], buf, len);
}

}
