/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

///
/// A byte-addressable ring buffer FIFO implementation.
///
///    +-------------------+------------------+------------------+
///    |  writable bytes   |  readable bytes  |  writable bytes  |
///    |                   |     (CONTENT)    |                  |
///    +-------------------+------------------+------------------+
///    |                   |                  |                  |
/// m_beginPoint <=     m_readIndex   <=   m_writeIndex    <=  m_endIndex
///
///  wrap
///    +-------------------+------------------+------------------+
///    |  readable bytes   |  writable bytes  |  readable bytes  |
///    |  (CONTENT PART2)  |                  | (CONTENT PART1)  |
///    +-------------------+------------------+------------------+
///    |                   |                  |                  |
/// m_beginPoint <=     m_writeIndex   <=   m_readIndex   <=  m_endIndex
///

namespace cyclone
{

class RingBuf
{
public:
	enum { kDefaultCapacity = 1024 - 1 };

	/// return the size of the internal buffer, in bytes.
	size_t size(void) const {
		return (m_write >= m_read) ? (m_write - m_read) : (m_end - m_read + m_write);
	}

	/// reset a ring buffer to its initial state (empty).
	void reset(void) {
		m_write = m_read = 0;
	}

	/// return the usable capacity of the ring buffer, in bytes.
	size_t capacity(void) const {
		return m_end-1;
	}

	//// return the number of free/available bytes in the ring buffer.
	size_t get_free_size(void) const {
		return (m_write >= m_read) ? (m_end - m_write + m_read - 1) : (m_read - m_write - 1);
	}

	//// return is empty
	bool empty(void) const {
		return (m_write == m_read);
	}

	/// return is full
	bool full(void) const {
		return get_free_size() == 0;
	}

	//// re-alloca memory, make sure capacity greater need_size
	void resize(size_t need_size);

	////  copy n bytes from a contiguous memory area into the ring buffer
	void memcpy_into(const void *src, size_t count);

	//// copy n bytes from the ring buffer into a contiguous memory area dst
	size_t memcpy_out(void *dst, size_t count);

	//// move data to another ring buf dst
	size_t moveto(RingBuf& dst, size_t count);

	//// search data(1 byte) and return the first position, return -1 means not find
	ssize_t search(size_t off, uint8_t data) const;

	//// copy n bytes from the ring buffer into a contiguous memory, but 
	//// do not change current buf
	size_t peek(size_t off, void* dst, size_t count) const;

	//// just discard at least n bytes data, return size that abandon actually
	size_t discard(size_t count);

	//// call read on the socket descriptor(fd), using the ring buffer rb as the 
	//// destination buffer for the read, and read as more data as impossible data.
	//// set extra_read to false if you don't want expand this ring buf
	ssize_t read_socket(socket_t fd, bool extra_read=true);

	//// call write on the socket descriptor(fd), using the ring buffer rb as the 
	//// source buffer for writing, In Linux platform, it will only call writev
	//// once, and may return a short count.
	ssize_t write_socket(socket_t fd);

	//// calculate the checksum(adler32) of data from off to off+len
	//// if off greater than size() or off+count greater than size() 
	//// or len equ 0 return initial adler value (1)
	uint32_t checksum(size_t off, size_t count) const;

	//// move all data to a flat memory block and return point
	uint8_t* normalize(void);

public:
	RingBuf(size_t capacity = kDefaultCapacity);
	~RingBuf();

private:
	uint8_t* m_buf;
	size_t m_end;
	size_t m_read;
	size_t m_write;
};

}
