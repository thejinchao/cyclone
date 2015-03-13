/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_ring_buf.h"

#ifndef MIN
#define MIN(x, y)	((x)<(y)?(x):(y))
#endif

namespace cyclone
{

//-------------------------------------------------------------------------------------
RingBuf::RingBuf(size_t _capacity)
{
	/* One byte is used for detecting the full condition. */
	m_buf = (char*)malloc(_capacity + 1);
	m_end = _capacity + 1;
	reset();
}

//-------------------------------------------------------------------------------------
RingBuf::~RingBuf()
{
	free(this->m_buf);
}

//-------------------------------------------------------------------------------------
void RingBuf::_auto_resize(size_t need_size)
{
	//auto inc size
	size_t new_size = 2;
	while (new_size < need_size) new_size *= 2;

	//copy old data
	size_t old_size = size();
	char* buf = (char*)malloc(new_size);
	this->memcpy_out(buf, old_size);

	//free old buf
	free(m_buf);

	//reset
	m_buf = buf;
	m_end = new_size;
	m_read = 0;
	m_write = old_size;
}

//-------------------------------------------------------------------------------------
void RingBuf::memcpy_into(const void *src, size_t count)
{
	if (get_free_size() < count) {
		_auto_resize(size() + count + 1);
	}

	char* csrc = (char*)src;

	//write data
	size_t nwritten = 0;
	while (nwritten != count) {
		size_t n = (size_t)MIN((size_t)(m_end - m_write), count - nwritten);
		memcpy(m_buf+m_write, csrc + nwritten, n);
		m_write += n;
		nwritten += n;

		// wrap?
		if (m_write >= m_end) m_write = 0;
	}
}

//-------------------------------------------------------------------------------------
size_t RingBuf::memcpy_out(void *dst, size_t count)
{
	size_t bytes_used = size();
	if (count > bytes_used)
		count = bytes_used;

	char* cdst = (char*)dst;
	size_t nread = 0;
	while (nread != count) {
		size_t n = MIN((size_t)(m_end - m_read), count - nread);
		memcpy(cdst + nread, m_buf + m_read, n);
		m_read += n;
		nread += n;

		// wrap 
		if (m_read >= m_end) m_read = 0;
	}

	//reset read and write index to zero
	if (empty()) reset();
	return count;
}

//-------------------------------------------------------------------------------------
size_t RingBuf::copyto(RingBuf* dst, size_t count)
{
	size_t bytes_used = size();
	if (count > bytes_used)
		count = bytes_used;

	size_t nread = 0;
	while (nread != count) {
		size_t n = MIN((size_t)(m_end - m_read), count - nread);
		dst->memcpy_into(m_buf + m_read, n);
		m_read += n;
		nread += n;

		// wrap 
		if (m_read >= m_end) m_read = 0;
	}

	//reset read and write index to zero
	if (empty()) reset();
	return count;
}

//-------------------------------------------------------------------------------------
size_t RingBuf::peek(size_t off, void* dst, size_t count) const
{
	size_t bytes_used = size();
	if (off > bytes_used) return 0;
	if (off + count > bytes_used) count = bytes_used - off;
	if (count == 0 || dst == 0) return 0;

	char* cdst = (char*)dst;
	size_t read_off = (m_read+off)%m_end;

	size_t nread = 0;
	while (nread != count) {
		size_t n = MIN((size_t)(m_end - read_off), count - nread);
		memcpy(cdst + nread, m_buf + read_off, n);
		read_off += n;
		nread += n;

		// wrap 
		if (read_off >= m_end) read_off = 0;
	}

	return count;
}

//-------------------------------------------------------------------------------------
size_t RingBuf::discard(size_t count)
{
	size_t bytes_used = size();
	if (count > bytes_used)
		count = bytes_used;

	m_read += count;
	m_read %= m_end;

	//reset read and write index to zero
	if (empty()) reset();

	return count;
}

//-------------------------------------------------------------------------------------
ssize_t RingBuf::read_socket(socket_t fd)
{
	const int32_t STACK_BUF_SIZE = 0xFFFF;
	char stack_buf[STACK_BUF_SIZE];

#ifdef CY_SYS_WINDOWS
	//TODO: it is not correct to call read() more than once in on event call!
	size_t count = get_free_size();

	//in windows call read three times maxmium
	ssize_t nwritten = 0;
	while (nwritten != (ssize_t)count)	{
		ssize_t n = (ssize_t)MIN((size_t)(m_end - m_write), count - nwritten);
		ssize_t len = socket_api::read(fd, m_buf + m_write, (ssize_t)n);
		if (len <= 0) return len;
		m_write += len;
		nwritten += len;

		// wrap?
		if (m_write >= m_end) m_write = 0;

		//no more data?
		if (len < n) return nwritten;
	}

	//need read more data
	ssize_t len = socket_api::read(fd, stack_buf, STACK_BUF_SIZE);
	if (len <= 0) return len;
	memcpy_into(stack_buf, len);
	return nwritten + len;
#else
	//use vector read functon
	struct iovec vec[3];
	int32_t vec_counts = 0;
	size_t count = get_free_size();

	ssize_t nwritten = 0;
	size_t write_off = m_write;
	while (nwritten != (ssize_t)count)	{
		ssize_t n = (ssize_t)MIN((size_t)(m_end - write_off), count - nwritten);
		vec[vec_counts].iov_base = m_buf + write_off;
		vec[vec_counts].iov_len = n;
		vec_counts++;

		nwritten += n;
		write_off += n;

		// wrap?
		if (write_off >= m_end) write_off = 0;
	}

	//add extra buff
	vec[vec_counts].iov_base = stack_buf;
	vec[vec_counts].iov_len = STACK_BUF_SIZE;
	vec_counts++;

	//call sys function
	ssize_t read_counts = ::readv(fd, vec, vec_counts);
	if (read_counts <= 0) return read_counts;	//error

	//adjust point
	count = MIN(get_free_size(), (size_t)read_counts);
	nwritten = 0;
	while (nwritten != (ssize_t)count)	{
		ssize_t n = (ssize_t)MIN((size_t)(m_end - m_write), count - nwritten);

		nwritten += n;
		m_write += n;

		// wrap?
		if (m_write >= m_end) m_write = 0;
	}

	//append extra data
	if (nwritten < read_counts) {
		memcpy_into(stack_buf, read_counts - nwritten);
	}

	return read_counts;
#endif
}

//-------------------------------------------------------------------------------------
ssize_t RingBuf::write_socket(socket_t fd)
{
#ifdef CY_SYS_WINDOWS
	size_t count = size();

	size_t nsended = 0;
	while (nsended != count) {
		size_t n = MIN((size_t)(m_end - m_read), count - nsended);

		ssize_t len = socket_api::write(fd, m_buf + m_read, (ssize_t)n);
		if (len <= 0) return len; //error

		m_read += len;
		nsended += len;

		// wrap 
		if (m_read >= m_end) m_read = 0;

		//socket buf busy, try next time
		if(len < (ssize_t)n) {
			break;
		}
	}

	//reset read and write index to zero
	if (empty()) reset();

	return (ssize_t)nsended;
#else
	struct iovec vec[2];
	int32_t vec_counts = 0;
	size_t count = size();

	size_t nsended = 0;
	size_t read_off = m_read;
	while (nsended != count) {
		size_t n = MIN((size_t)(m_end - read_off), count - nsended);

		vec[vec_counts].iov_base = m_buf + read_off;
		vec[vec_counts].iov_len = n;
		vec_counts++;

		read_off += n;
		nsended += n;

		// wrap 
		if (read_off >= m_end) read_off = 0;
	}

	//call sys function
	ssize_t write_counts = ::writev(fd, vec, vec_counts);
	if (write_counts <= 0) return write_counts;	//error

	//adjust point
	nsended = 0;
	while (nsended != (size_t)write_counts) {
		size_t n = MIN((size_t)(m_end - m_read), write_counts - nsended);

		m_read += n;
		nsended += n;

		// wrap 
		if (m_read >= m_end) m_read = 0;
	}

	//reset read and write index to zero
	if (empty()) reset();

	return (ssize_t)nsended;
#endif
}

//-------------------------------------------------------------------------------------
uint32_t RingBuf::checksum(size_t off, size_t count) const
{
	uint32_t adler = adler32(0, 0, 0);

	size_t bytes_used = size();
	if (off > bytes_used) return adler;
	if (off + count > bytes_used) return adler;
	if (count == 0) return adler;

	size_t read_off = (m_read + off) % m_end;

	size_t nread = 0;
	while (nread != count) {
		size_t n = MIN((size_t)(m_end - read_off), count - nread);
		adler = adler32(adler, m_buf + read_off, n);
		read_off += n;
		nread += n;

		// wrap 
		if (read_off >= m_end) read_off = 0;
	}

	return adler;
}

}
