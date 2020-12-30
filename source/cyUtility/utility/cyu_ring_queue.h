/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{

template<typename T>
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
			_auto_size((size_t)1);
		}
		m_vector[m_write] = value;
		m_write = _next(m_write);
	}

	const T& front(void) const {
		assert(!empty());

		return m_vector[m_read];
	}

	const T& back(void) const {
		assert(!empty());

		return m_vector[_prev(m_write)];
	}

	const T& get(size_t index) const {
		assert(index>=0 && index<size());

		return m_vector[_next(m_read, index)];
	}

	void pop(size_t counts = 1) {
		if (counts >= size()) {
			reset();
			return;
		}
		m_read = _next(m_read, counts);
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
	size_t _next(size_t pos, size_t step=1) const {
		return (pos + step) % m_end;
	}
	
	size_t _prev(size_t pos, size_t step=1) const {
		return (pos >= step) ? (pos - step) : (pos + m_end - step);
	}

	void _auto_size(size_t more_size) 
	{
		size_t free_size = get_free_size();
		if (free_size >= more_size) return;

		if (m_fixed) {
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
				std::move(m_vector.begin(), m_vector.begin()+ (ValueVectorDifferenceType)m_write, m_vector.begin()+ (ValueVectorDifferenceType)m_end);
				m_write += m_end;
			}
			m_end = new_size;
		}
	}
public:
	RingQueue(size_t fixed_capacity = 0) : m_fixed(fixed_capacity>0)
	{
		size_t cap = kDefaultCapacity + 1;
		m_end = cap;

		if (m_fixed) {
			cap = ((size_t)(fixed_capacity /8) + 1) * 8; //align 8
			m_end = fixed_capacity + 1;
		}

		m_vector.resize(cap);
		reset();
	}
private:
	typedef std::vector<T> ValueVector;
	using ValueVectorDifferenceType = typename ValueVector::difference_type;

	bool m_fixed;
	ValueVector m_vector;
	size_t m_read;
	size_t m_write;
	size_t m_end;
};

}
