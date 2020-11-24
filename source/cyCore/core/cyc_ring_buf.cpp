/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_crypt.h>

#include "cyc_ring_buf.h"
#ifdef CY_HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

namespace cyclone
{

//-------------------------------------------------------------------------------------
RingBuf::RingBuf(size_t _capacity)
{
	/* One byte is used for detecting the full condition. */
	m_buf = (uint8_t*)CY_MALLOC(_capacity + 1);
	m_end = _capacity + 1;
	reset();
}

//-------------------------------------------------------------------------------------
RingBuf::~RingBuf()
{
	CY_FREE(this->m_buf);
}

//-------------------------------------------------------------------------------------
void RingBuf::resize(size_t need_size)
{
	if (capacity() >= need_size) return;

	//auto inc size
	size_t new_size = 2;
	while (new_size < need_size) new_size *= 2;

	//copy old data
	size_t old_size = size();
	uint8_t* buf = (uint8_t*)CY_MALLOC(new_size);
	this->memcpy_out(buf, old_size);

	//free old buf
	CY_FREE(m_buf);

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
		resize(size() + count + 1);
	}

	char* csrc = (char*)src;

	//write data
	size_t nwritten = 0;
	while (nwritten != count) {
		size_t n = (size_t)std::min((size_t)(m_end - m_write), count - nwritten);
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
		size_t n = std::min((size_t)(m_end - m_read), count - nread);
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
size_t RingBuf::moveto(RingBuf& dst, size_t count)
{
	size_t bytes_used = size();
	if (count > bytes_used)
		count = bytes_used;

	size_t nread = 0;
	while (nread != count) {
		size_t n = std::min((size_t)(m_end - m_read), count - nread);
		dst.memcpy_into(m_buf + m_read, n);
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
ssize_t RingBuf::search(size_t off, uint8_t data) const
{
	size_t bytes_used = size();
	if (off > bytes_used || bytes_used<sizeof(data)) return -1;

	size_t search_count = bytes_used - off;
	size_t read_off = (m_read + off) % m_end;

	size_t nread = 0;
	while (nread != search_count) {
		size_t n = std::min((size_t)(m_end - read_off), search_count - nread);
		const uint8_t* p = (uint8_t*)memchr(m_buf + read_off, (int)data, n);
		if (p != nullptr) {
			size_t pos = (size_t)(std::ptrdiff_t)(p - m_buf);
			return (pos > m_read) ? (ssize_t)(pos - m_read) : (ssize_t)(pos + m_end - m_read);
		}

		read_off += n;
		nread += n;

		// wrap 
		if (read_off >= m_end) read_off = 0;
	}

	return -1;
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
		size_t n = std::min((size_t)(m_end - read_off), count - nread);
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
ssize_t RingBuf::read_socket(socket_t fd, bool extra_buf)
{
	const size_t STACK_BUF_SIZE = 0xFFFF;
	char stack_buf[STACK_BUF_SIZE];

#ifndef CY_HAVE_READWRITE_V
	//TODO: it is not correct to call read() more than once in on event call!
	size_t count = get_free_size();

	//in windows call read three times maxmium
	ssize_t nwritten = 0;
	while (nwritten != (ssize_t)count)	{
		ssize_t n = (ssize_t)std::min((ssize_t)(m_end - m_write), (ssize_t)(count - nwritten));
		ssize_t len = socket_api::read(fd, m_buf + m_write, (ssize_t)n);
		if (len == 0) return 0; //EOF
		if (len < 0) return socket_api::is_lasterror_WOULDBLOCK() ? nwritten : len;

		m_write += len;
		nwritten += len;

		// wrap?
		if (m_write >= m_end) m_write = 0;

		//no more data?
		if (len < n) return nwritten;
	}

	//need read more data
	if (extra_buf) {
		ssize_t len = socket_api::read(fd, stack_buf, STACK_BUF_SIZE);
		if (len == 0) return nwritten; //EOF
		if (len < 0) return socket_api::is_lasterror_WOULDBLOCK() ? nwritten : len;
		memcpy_into(stack_buf, len);
		return nwritten + len;
	}
	return nwritten;
#else
	//use vector read functon
	struct iovec vec[3];
	int32_t vec_counts = 0;
	size_t count = get_free_size();

	size_t nwritten = 0;
	size_t write_off = m_write;
	while (nwritten != count)	{
		size_t n = std::min((size_t)(m_end - write_off), count - nwritten);
		vec[vec_counts].iov_base = m_buf + write_off;
		vec[vec_counts].iov_len = n;
		vec_counts++;

		nwritten += n;
		write_off += n;

		// wrap?
		if (write_off >= m_end) write_off = 0;
	}

	//add extra buff
	if (extra_buf) {
		vec[vec_counts].iov_base = stack_buf;
		vec[vec_counts].iov_len = STACK_BUF_SIZE;
		vec_counts++;
	}

	//call sys function
	ssize_t read_counts = ::readv(fd, vec, vec_counts);
	if (read_counts <= 0) return read_counts;	//error

	//adjust point
	count = std::min(get_free_size(), (size_t)read_counts);
	nwritten = 0;
	while (nwritten != count)	{
		size_t n = std::min((size_t)(m_end - m_write), count - nwritten);

		nwritten += n;
		m_write += n;

		// wrap?
		if (m_write >= m_end) m_write = 0;
	}

	//append extra data
	if (nwritten < (size_t)read_counts) {
		assert(extra_buf);
		memcpy_into(stack_buf, (size_t)read_counts - nwritten);
	}

	return read_counts;
#endif
}

//-------------------------------------------------------------------------------------
ssize_t RingBuf::write_socket(socket_t fd)
{
	assert(!empty());

#ifndef CY_HAVE_READWRITE_V
	size_t count = size();

	size_t nsended = 0;
	while (nsended != count) {
		size_t n = std::min((size_t)(m_end - m_read), count - nsended);

		ssize_t len = socket_api::write(fd, (const char*)m_buf + m_read, (ssize_t)n);
		if (len == 0) break; //nothing was written
		if (len < 0) //error
		{
			if (nsended>0) break;
			else return len;
		}

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
		size_t n = std::min((size_t)(m_end - read_off), count - nsended);

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
		size_t n = std::min((size_t)(m_end - m_read), (size_t)write_counts - nsended);

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
	uint32_t adler = INITIAL_ADLER;

	size_t bytes_used = size();
	if (off > bytes_used) return adler;
	if (off + count > bytes_used) return adler;
	if (count == 0) return adler;

	size_t read_off = (m_read + off) % m_end;

	size_t nread = 0;
	while (nread != count) {
		size_t n = std::min((size_t)(m_end - read_off), count - nread);
		adler = adler32(adler, m_buf + read_off, n);
		read_off += n;
		nread += n;

		// wrap 
		if (read_off >= m_end) read_off = 0;
	}

	return adler;
}

//-------------------------------------------------------------------------------------
uint8_t* RingBuf::normalize(void)
{
	if (empty()) reset();
	if (m_write >= m_read) return m_buf + m_read;

	//need move memory
	char default_temp_block[kDefaultCapacity];

	size_t first_block = m_write;
	size_t second_block = m_end - m_read;

	//alloc a temp block memory
	size_t temp_block = std::min(first_block, second_block);
	char* p = (temp_block <= kDefaultCapacity) ? default_temp_block : (char*)CY_MALLOC(temp_block);

	//which block is the smaller block?
	if (first_block <= second_block) {
		memcpy(p, m_buf, first_block);
		memmove(m_buf, m_buf + m_read, second_block);
		memcpy(m_buf + second_block, p, first_block);
	}
	else {
		memcpy(p, m_buf + m_read, second_block);
		memmove(m_buf + second_block, m_buf, first_block);
		memcpy(m_buf, p, second_block);
	}
	m_read = 0;
	m_write = first_block + second_block;

	if (p != default_temp_block) {
		CY_FREE(p);
	}

	return m_buf;
}

}
