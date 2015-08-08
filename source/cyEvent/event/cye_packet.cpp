/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_packet.h"

namespace cyclone
{
//-------------------------------------------------------------------------------------
Packet::Packet()
	: m_head_size(0)
	, m_memory_buf(0)
	, m_memory_size(0)
	, m_packet_size(0)
	, m_packet_id(0)
	, m_content(0)
{

}

//-------------------------------------------------------------------------------------
Packet::Packet(const Packet& other)
	: m_head_size(0)
	, m_memory_buf(0)
	, m_memory_size(0)
	, m_packet_size(0)
	, m_packet_id(0)
	, m_content(0)
{
	if (other.m_memory_size == 0) return;

	_resize(other.m_head_size, other.get_packet_size());
	memcpy(m_memory_buf, other.m_memory_buf, other.m_memory_size);
}

//-------------------------------------------------------------------------------------
Packet::~Packet()
{
	clean();
}

//-------------------------------------------------------------------------------------
void Packet::clean(void)
{
	m_head_size = 0;

	if (m_memory_buf && m_memory_buf != m_static_buf)
	{
		delete[] m_memory_buf;
	}
	m_memory_buf = 0;
	m_memory_size = 0;
	m_packet_size = 0;
	m_packet_id = 0;
	m_content = 0;
}

//-------------------------------------------------------------------------------------
uint16_t Packet::get_packet_size(void) const
{
	return m_packet_size ? socket_api::ntoh_16(*m_packet_size) : (uint16_t)0;
}

//-------------------------------------------------------------------------------------
uint16_t Packet::get_packet_id(void) const
{
	return m_packet_id ? socket_api::ntoh_16(*m_packet_id) : (uint16_t)0;
}

//-------------------------------------------------------------------------------------
const char* Packet::get_packet_content(void) const
{
	return m_content;
}

//-------------------------------------------------------------------------------------
void Packet::_resize(size_t head_size, size_t packet_size)
{
	assert(head_size >= 2 * sizeof(uint16_t));

	m_head_size = head_size;
	m_memory_size = head_size + packet_size;

	if (m_memory_size <= STATIC_MEMORY_LENGTH)
		m_memory_buf = m_static_buf;
	else
		m_memory_buf = new char[m_memory_size];

	m_packet_size = (uint16_t*)m_memory_buf;
	m_packet_id = (uint16_t*)(m_memory_buf+sizeof(uint16_t));
	m_content = packet_size>0 ? (char*)(m_memory_buf + head_size) : 0;
}

//-------------------------------------------------------------------------------------
bool Packet::build(size_t head_size, uint16_t packet_id, uint16_t packet_size, const char* packet_content)
{
	clean();

	//prepare memory
	_resize(head_size, packet_size);

	*m_packet_size = socket_api::ntoh_16(packet_size);
	*m_packet_id = socket_api::ntoh_16(packet_id);

	if (packet_content)
		memcpy(m_content, packet_content, packet_size);
	return true;
}

//-------------------------------------------------------------------------------------
bool Packet::build(size_t head_size, Pipe& pipe)
{
	clean();

	//read size
	uint16_t packet_size;
	if (sizeof(packet_size) != pipe.read((char*)&packet_size, sizeof(packet_size))){
		return false;
	}

	//prepare memory
	_resize(head_size, (size_t)socket_api::ntoh_16(packet_size));
	*m_packet_size = packet_size;

	//read other
	size_t remain = head_size + get_packet_size() - sizeof(uint16_t);
	if ((ssize_t)remain != pipe.read((char*)m_packet_id, remain)){
		clean();
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool Packet::build(size_t head_size, RingBuf& ring_buf)
{
	clean();

	size_t buf_len = ring_buf.size();

	uint16_t packet_size;
	if (sizeof(packet_size) != ring_buf.peek(0, &packet_size, sizeof(packet_size))) return false;
	packet_size = socket_api::ntoh_16(packet_size);

	if (buf_len < head_size + (size_t)packet_size) return false;

	_resize(head_size, packet_size);

	ring_buf.memcpy_out(m_memory_buf, m_memory_size);

	return true;
}

//-------------------------------------------------------------------------------------
bool Packet::build(size_t head_size, uint16_t packet_id,
	uint16_t packet1_size, const char* packet1_content,
	uint16_t packet2_size, const char* packet2_content)
{
	clean();

	uint16_t total_size = (uint16_t)(packet1_size + packet2_size);
	_resize(head_size, packet1_size + packet2_size);

	*m_packet_size = socket_api::ntoh_16((uint16_t)(packet1_size + packet2_size));
	*m_packet_id = socket_api::ntoh_16(packet_id);

	if (total_size>0)
	{
		if (packet1_size>0)
			memcpy(m_content, packet1_content, packet1_size);

		if (packet2_size>0)
			memcpy(m_content + packet1_size, packet2_content, packet2_size);
	}
	return true;
}

}

