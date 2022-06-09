/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{

typedef socket_t pipe_port_t;

class Pipe : noncopyable
{
public:
	pipe_port_t get_read_port(void) { return m_pipe_fd[0]; }
	pipe_port_t get_write_port(void) { return m_pipe_fd[1]; }

	ssize_t write(const char* buf, size_t len);
	ssize_t read(char* buf, size_t len);

	static bool construct_socket_pipe(pipe_port_t handles[2]);
	static void destroy_socket_pipe(pipe_port_t handles[2]);

private:
	pipe_port_t m_pipe_fd[2];

public:
	Pipe();		//build pipe
	~Pipe();
};

}
