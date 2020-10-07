/*
Copyright(C) thecodeway.com
*/

#ifndef _CYCLONE_CORE_RING_QUEUE_H_
#define _CYCLONE_CORE_RING_QUEUE_H_

#include <cyclone_config.h>

namespace cyclone
{

template<typename T, size_t FixedCapacity=0>
class RingQueue
{
public:
	enum { kDefaultCapacity = 32 - 1 };
	typedef std::function<bool(size_t, const T&)> WalkFunc;

	size_t size(void) const {
		return (m_write >= m_read) ? (m_write - m_read) : (m_end - m_read + m_write);
	}

	void reset(void) {
		m_write = m_read = 0;
	}
	
	size_t capacity(void) const {
		return m_end - 1;
	}

	size_t get_free_size(void) const {
		return (m_write >= m_read) ? (m_end - m_write + m_read - 1) : (m_read - m_write - 1);
	}

	bool empty(void) const {
		return (m_write == m_read);
	}

	void push(const T& value) {
		if (get_free_size() == 0) {
			_auto_size(1);
		}
		m_vector[m_write] = value;
		m_write = _next(m_write);
	}

	T front(void) {
		assert(!empty());

		return m_vector[m_read];
	}

	void pop(size_t counts = 1) {
		if (counts >= size()) {
			reset();
			return;
		}
		m_read = (m_read + counts) % m_end;
	}

	void walk(WalkFunc walk_func) 
	{
		if (walk_func == nullptr || empty()) return;

		size_t index = 0;
		size_t pos = m_read;
		while (pos != m_write) {
			if (!walk_func(index++, m_vector[pos])) return;
			pos = _next(pos);
		}
	}

	void walk_reserve(WalkFunc walk_func) 
	{
		if (walk_func == nullptr || empty()) return;
		size_t index = size() - 1;
		size_t pos = m_write;
		while (pos != m_read) {
			pos = _prev(pos);
			if (!walk_func(index--, m_vector[pos])) return;
		};
	}

private:
	size_t _next(size_t pos, size_t step=1) {
		return (pos + step) % m_end;
	}
	
	size_t _prev(size_t pos, size_t step=1) {
		return (pos >= step) ? (pos - step) : (pos + m_end - step);
	}

	void _auto_size(size_t more_size) 
	{
		size_t free_size = get_free_size();
		if (free_size >= more_size) return;

		if CONSTEXPR(FixedCapacity > 0) {
			pop(more_size - free_size);
		}
		else {
			size_t need_size = more_size + size() + 1;

			//auto inc size
			size_t new_size = 2;
			while (new_size < need_size) new_size *= 2;
			m_vector.resize(new_size);

			//move data if wrap condition
			if (m_read > m_write) {
				std::move(m_vector.begin(), m_vector.begin() + m_write, m_vector.begin() + m_end);
				m_write += m_end;
			}
			m_end = new_size;
		}
	}
public:
	RingQueue() 
	{
		size_t cap = kDefaultCapacity + 1;
		m_end = cap;

		if CONSTEXPR (FixedCapacity > 0) {
			cap = ((size_t)(FixedCapacity/8) + 1) * 8; //align 8
			m_end = FixedCapacity + 1;
		}

		m_vector.resize(cap);
		reset();
	}
private:
	typedef std::vector<T> ValueVector;

	ValueVector m_vector;
	size_t m_read;
	size_t m_write;
	size_t m_end;
};

}

#endif
