/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_PIPE_H_
#define _CYCLONE_EVENT_PIPE_H_

#include <cyclone_config.h>

namespace cyclone
{

typedef socket_t pipe_port_t;

class Pipe
{
public:
	pipe_port_t get_read_port(void) { return m_pipe_fd[0]; }
	pipe_port_t get_write_port(void){ return m_pipe_fd[1]; }

	ssize_t write(const char* buf, size_t len);
	ssize_t read(char* buf, size_t len);

private:
	pipe_port_t m_pipe_fd[2];

public:
	Pipe();		//build pipe
	~Pipe();
};

}

#endif
