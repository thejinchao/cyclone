/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_packet.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
void* Packet::operator new(size_t size)
{
	void* p = CY_MALLOC(size);
	return p;
}

//-------------------------------------------------------------------------------------
void Packet::operator delete(void* p)
{
	CY_FREE(p);
}

//-------------------------------------------------------------------------------------
Packet* Packet::alloc_packet(const Packet* other)
{
	Packet* p = new Packet();

	if (other && other->m_memory_size > 0) {
		p->_resize(other->m_head_size, other->get_packet_size());
		memcpy(p->m_memory_buf, other->m_memory_buf, other->m_memory_size);
	}

	return p;
}

//-------------------------------------------------------------------------------------
void Packet::free_packet(Packet* p)
{
	delete p;
}

//-------------------------------------------------------------------------------------
Packet::Packet()
	: m_head_size(0)
	, m_memory_buf(nullptr)
	, m_memory_size(0)
	, m_packet_size(0)
	, m_packet_id(0)
	, m_content(nullptr)
{

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
		CY_FREE(m_memory_buf);
	}
	m_memory_buf = nullptr;
	m_memory_size = 0;
	m_packet_size = 0;
	m_packet_id = 0;
	m_content = nullptr;
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
void Packet::_resize(size_t head_size, size_t packet_size)
{
	assert(head_size >= 2 * sizeof(uint16_t));

	m_head_size = head_size;
	m_memory_size = head_size + packet_size;
	size_t need_memory_size = m_memory_size + MEMORY_SAFE_TAIL_SIZE;

	if (need_memory_size <= STATIC_MEMORY_LENGTH)
		m_memory_buf = m_static_buf;
	else
		m_memory_buf = (char*)CY_MALLOC(need_memory_size);
	memset(m_memory_buf, 0xCE, need_memory_size);	//fill memory with 0xCE (CyclonE)

	m_packet_size = (uint16_t*)m_memory_buf;
	m_packet_id = (uint16_t*)(m_memory_buf+sizeof(uint16_t));
	m_content = packet_size>0 ? (char*)(m_memory_buf + head_size) : nullptr;
}

//-------------------------------------------------------------------------------------
void Packet::build_from_memory(size_t head_size, uint16_t packet_id,
	uint16_t packet_size_part1, const char* packet_content_part1,
	uint16_t packet_size_part2, const char* packet_content_part2)
{
	clean();

	int32_t packet_size = (int32_t)packet_size_part1 + (int32_t)packet_size_part2;

	//check size
	assert(packet_size <= std::numeric_limits<uint16_t>::max());
	if (packet_size > std::numeric_limits<uint16_t>::max()) return;

	//prepare memory
	_resize(head_size, (size_t)packet_size);

	*m_packet_size = socket_api::ntoh_16((uint16_t)packet_size);
	*m_packet_id = socket_api::ntoh_16(packet_id);

	// no content?
	if (m_content==nullptr) return;

	if (packet_size_part1 > 0 && packet_content_part1) {
		memcpy(m_content, packet_content_part1, packet_size_part1);
	}

	if (packet_size_part2 > 0 && packet_content_part2) {
		memcpy(m_content+packet_size_part1, packet_content_part2, packet_size_part2);
	}
}

//-------------------------------------------------------------------------------------
bool Packet::build_from_pipe(size_t head_size, Pipe& pipe)
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
bool Packet::build_from_ringbuf(size_t head_size, RingBuf& ring_buf)
{
	clean();

	size_t buf_len = ring_buf.size();

	uint16_t packet_size;
	if (sizeof(packet_size) != ring_buf.peek(0, &packet_size, sizeof(packet_size))) return false;
	packet_size = socket_api::ntoh_16(packet_size);

	if (buf_len < head_size + (size_t)packet_size) return false;

	_resize(head_size, packet_size);

	return (m_memory_size==ring_buf.memcpy_out(m_memory_buf, m_memory_size));
}

}

