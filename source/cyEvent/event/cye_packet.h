/*
Copyright(C) thecodeway.com
*/
#pragma once

/*
                       Low                        High                          
                        +-------------+------------+  <---+ get_memory_buf()    
                     /  |  PacketID   | PacketSize |                            
             HeadSize   +-------------+------------+                            
                     \  |    (User Define Head)    |                            
                        +--------------------------+  <---+ get_packet_content()
                      / |     (Packet Content)     |                            
                     /  |                          |                            
           PacketSize   |                          |                            
                     \  |                          |                            
                      \ |                          |                            
                        +--------------------------+                            
MEMORY_SAFE_TAIL_SIZE   |         Safe Tail        |                            
                        +--------------------------+    

*MemorySize = HeadSize+PacketSize
*PacketID and PacketSize is big endain 16bit ingeter

*/
namespace cyclone
{

class Packet : noncopyable
{
public:
	void clean(void);

	//build from memory
	// | packet_size |   packet_id   |  user_define_head |      packet_content1      |   packet_content2     |
	// |    uint16   |     uint16    |        ...        |        packet_size1       |     packet_size2      |
	// |  <---------------      head_size      --------->|<--------------      packet_size    -------------->|
	//
	void build_from_memory(size_t head_size, uint16_t packet_id,
		uint16_t packet_size_part1, const char* packet_content_part1, 
		uint16_t packet_size_part2=0, const char* packet_content_part2=nullptr);

	//build from Pipe and RingBuf
	// | packet_size |   packet_id   |  user_define_head |      packet_content   |
	// |    uint16   |     uint16    |        ...        |           ...         |
	// |  <---------------      head_size      --------->|<-----  packet_size -->|
	//
	bool build_from_pipe(size_t head_size, Pipe& pipe);
	bool build_from_ringbuf(size_t head_size, RingBuf& ring_buf);

public:
	char* get_memory_buf(void) { return m_memory_buf; }
	const char* get_memory_buf(void) const { return m_memory_buf; }
	size_t get_memory_size(void) const { return m_memory_size; }

	uint16_t get_packet_size(void) const;
	uint16_t get_packet_id(void) const;
	char* get_packet_content(void) { return m_content; }
	const char* get_packet_content(void) const { return m_content; }

private:
	void _resize(size_t head_size, size_t packet_size);

private:
	size_t m_head_size;

	enum { STATIC_MEMORY_LENGTH = 1024 };
	enum { MEMORY_SAFE_TAIL_SIZE = 8 };

	char m_static_buf[STATIC_MEMORY_LENGTH];
	char*	m_memory_buf;
	size_t	m_memory_size;

	uint16_t *m_packet_size;
	uint16_t *m_packet_id;
	char	 *m_content;

public:
	Packet();
	~Packet();

	void* operator new(size_t);
	void operator delete(void*);

	static Packet* alloc_packet(const Packet* other = 0);
	static void free_packet(Packet*);
};

}

