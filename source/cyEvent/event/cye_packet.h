/*
Copyright(C) thecodeway.com
*/

#ifndef _CYCLONE_EVENT_PACKET_H_
#define _CYCLONE_EVENT_PACKET_H_

namespace cyclone
{

class Packet
{
public:
	void clean(void);

	bool build(size_t head_size, uint16_t packet_id, uint16_t packet_size, const char* packet_content);
	bool build(size_t head_size, Pipe& pipe);
	bool build(size_t head_size, RingBuf& ring_buf);

public:
	char* get_memory_buf(void) { return m_memory_buf; }
	const char* get_memory_buf(void) const { return m_memory_buf; }
	size_t get_memory_size(void) const { return m_memory_size; }

	uint16_t get_packet_size(void) const;
	uint16_t get_packet_id(void) const;
	const char* get_packet_content(void) const;

private:
	void _resize(size_t head_size, size_t packet_size);

private:
	size_t m_head_size;

	enum { STATIC_MEMORY_LENGTH = 1024 };
	char m_static_buf[STATIC_MEMORY_LENGTH];
	char*	m_memory_buf;
	size_t	m_memory_size;

	uint16_t *m_packet_size;
	uint16_t *m_packet_id;
	char	 *m_content;

public:
	Packet();
	Packet(const Packet& other);
	~Packet();
};

}

#endif
