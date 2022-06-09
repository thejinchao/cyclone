/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>
#include <cy_core.h>
#include <utility/cyu_ring_queue.h>

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

// PeriodValue:
// More concerned about the performance of a variable in a certain period of time(in millisecond)
template<typename T, bool WithLock = true, T ZeroValue=0>
struct PeriodValue
{
public:
	struct AutoLock {
		AutoLock(sys_api::mutex_t l) : _lock(l){
			if CONSTEXPR(WithLock) {
				sys_api::mutex_lock(_lock);
			}
		}
		~AutoLock() {
			if CONSTEXPR(WithLock) {
				sys_api::mutex_unlock(_lock);
			}
		}
		sys_api::mutex_t _lock;
	};
public:
	void push(const T& value, int64_t cur_performance_time=0)
	{
		AutoLock lock(m_lock);

		if (cur_performance_time == 0) {
			cur_performance_time = sys_api::performance_time_now() / 1000ll;
		}

		m_valueQueue.push(std::make_pair(cur_performance_time, value));

		//is value queue full?
		if (m_valueQueue.get_free_size() == 0) {
			//pop the front value if expired
			if (cur_performance_time - m_valueQueue.front().first > m_time_period) {
				m_valueQueue.pop();
			}
		}
	}

	size_t total_counts(void) const
	{
		return m_valueQueue.size();
	}

	std::pair<T, int32_t> sum_and_counts(int64_t cur_performance_time = 0) const
	{
		AutoLock lock(m_lock);

		if (cur_performance_time == 0) {
			cur_performance_time = sys_api::performance_time_now() / 1000ll;
		}

		_update(cur_performance_time);
		if (m_valueQueue.empty()) return std::make_pair(ZeroValue, 0);

		T sum = ZeroValue;
		int32_t counts=0;
		m_valueQueue.walk([&sum, &counts](size_t index, const ValuePair& v) -> bool {
			sum += v.second;
			counts++;
			return true;
		});

		return std::make_pair(sum, counts);
	}

	int32_t get_time_period(void) const {
		return m_time_period;
	}

public:
	PeriodValue(int32_t time_period_ms=1000) 
		: m_time_period(time_period_ms)
		, m_valueQueue(0) //not fixed size
		, m_lock(nullptr)
	{
		if CONSTEXPR(WithLock) {
			m_lock = sys_api::mutex_create();
		}
	}

	~PeriodValue()
	{
		if CONSTEXPR(WithLock) {
			sys_api::mutex_destroy(m_lock);
		}
	}

private:
	void _update(int64_t cur_time) const
	{
		if (m_valueQueue.empty()) return;
		int64_t expire = cur_time - m_time_period;

		if (m_valueQueue.front().first >= expire) return; // not necessary
		size_t left_bounder = 0;

		if (m_valueQueue.back().first < expire) { //all element expired
			m_valueQueue.reset();
			return;
		}
		size_t right_bounder = m_valueQueue.size()-1;
		
		while (right_bounder - left_bounder > 1) {
			size_t mid = (left_bounder + right_bounder) / 2; //binary search

			if (m_valueQueue.get(mid).first >= expire) {
				right_bounder = mid;
			}
			else {
				left_bounder = mid;
			}
		}
		//discard all expired element
		m_valueQueue.pop(left_bounder + 1);
	}

public:
	typedef std::pair<int64_t, T> ValuePair;
	typedef RingQueue<ValuePair> ValueQueue;

private:
	int32_t m_time_period; //millisecond
	mutable ValueQueue m_valueQueue;
	sys_api::mutex_t m_lock;
};

}
