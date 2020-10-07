/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_UTILITY_STATISTICS_H_
#define _CYCLONE_UTILITY_STATISTICS_H_

#include <cyclone_config.h>
#include <cy_core.h>

#include <queue>

namespace cyclone
{
// MinMaxValue:
// Used to count the maximum and minimum values of a variable in the whole life cycle
// such as the size of a sending cache
template<typename T>
struct MinMaxValue 
{
	void update(const T& value) 
	{
		atomic_greater_exchange(m_max, value, value);
		atomic_smaller_exchange(m_min, value, value);
	}

	T min(void) const 
	{
		return m_min.load();
	}

	T max(void) const 
	{
		return m_max.load();
	}

public:
	MinMaxValue() 
	{
		m_min = std::numeric_limits<T>::max();
		m_max = std::numeric_limits<T>::min();
	}

	MinMaxValue(const T& init) 
	{
		m_min = init;
		m_max = init;
	}

private:
	typedef std::atomic<T> AtomicValue;

	AtomicValue m_min;
	AtomicValue m_max;
};

}

#endif

