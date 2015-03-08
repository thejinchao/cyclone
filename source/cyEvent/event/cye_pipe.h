/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_PIPE_H_
#define _CYCLONE_EVENT_PIPE_H_

#include <cyclone_config.h>

namespace cyclone
{
namespace event
{

typedef socket_t pipe_port_t;

class Pipe
{
public:
	pipe_port_t get_read_port(void) { return m_pipe_fd[0]; }
	ssize_t write(const char* buf, ssize_t len);
	ssize_t read(char* buf, ssize_t len);

private:
	pipe_port_t m_pipe_fd[2];

public:
	Pipe();		//build pipe
	~Pipe();
};

}
}

#endif
