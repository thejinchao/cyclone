/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>
#include "cyc_atomic.h"

namespace cyclone
{

//
// LockFreeQueue - a lock-free circular array queue implementation
//
// Overview
// - This is a bounded, array-based, lock-free queue optimized for high
//   concurrency with trivial element types (POD). The template parameter
//   `ELEM_T` must be trivial; Q_SIZE must be a power of two and > 2.
//
// Concurrency model
// - Multiple producers and multiple consumers are supported.
// - Producers reserve a slot by atomically incrementing `m_writeIndex`,
//   write the element into the reserved slot, publish the slot by advancing
//   `m_publishedWriteIndex` in order, then increment an approximate `m_count`.
// - Consumers read `m_publishedWriteIndex` to discover available slots, read
//   the element from the array, and attempt to advance `m_readIndex` using
//   CAS. If CAS fails, another consumer claimed the slot and the consumer
//   retries.
//
// Memory ordering
// - The implementation uses explicit acquire/release semantics and fences to
//   ensure correct publication of written data before making the slot visible
//   to consumers. Specifically:
//     * Loads of indices use `memory_order_acquire`.
//     * CAS operations use `memory_order_acq_rel` for success and
//       `memory_order_acquire` for failure.
//     * A release fence is used after storing the element to guarantee its
//       visibility to a consumer that observes the advanced published write index.
// - These choices are conservative and portable across mainstream platforms.
//
// Semantics and guarantees
// - push returns true if an element was successfully enqueued, false if the
//   queue is full (non-blocking API). pop returns true if an element was
//   dequeued, false if the queue was empty (non-blocking API).
// - The `size()` method returns an approximate value and is only intended
//   for informational/monitoring use. It may be stale under heavy concurrency.
// - The queue stores elements by value in a pre-allocated array. Because
//   elements are not constructed/destructed in-place, `ELEM_T` is required to
//   be trivial.
//
// Implementation notes / warnings
// - The algorithm depends on monotonically increasing indices (`m_writeIndex`,
//   `m_readIndex`) whose difference is used to detect full/empty conditions.
//   Indices are 64-bit to make wrap-around effectively impossible in practice,
//   but the algorithm assumes that the distance between write and read indices
//   never exceed the buffer capacity. If extremely long-running systems cause
//   wrap-around, correctness can be violated.
// - Producers must publish `m_publishedWriteIndex` in the same order as they
//   reserved slots. To preserve order, producers may spin (yield) waiting for
//   earlier reservations to publish; this can introduce latency under certain
//   contention patterns. Consider backoff strategies if required.
// - `m_count` is updated with relaxed atomics and only provides a snapshot of
//   queue occupancy; it must not be used for correctness decisions.
// - The implementation favors low-latency optimistic reads: consumers read
//   the element before CAS. If CAS fails the read is discarded. This is
//   acceptable because `ELEM_T` is trivial and cheap to copy.
//
// Testing
// - Exercise the queue under multi-producer/multi-consumer stress tests,
//   tests near full/empty boundaries, and long-running tests to validate
//   behavior under contention.
//

template <typename ELEM_T, uint32_t Q_SIZE = 65536>  //default 2^16, it must be a power of 2 value
class LockFreeQueue
{
public:
	static_assert(std::is_trivial<ELEM_T>::value, "The type ELEM_T must be trivial");
	static_assert(std::is_copy_constructible<ELEM_T>::value, "The type ELEM_T must be copy constructible");
	static_assert(std::is_copy_assignable<ELEM_T>::value, "The type ELEM_T must be copy assignable");
	static_assert((Q_SIZE& (Q_SIZE - 1)) == 0, "Q_SIZE must be a power of 2");
	static_assert(Q_SIZE > 2, "Q_SIZE must be greater than 2");

	//It tries to take a snapshot of the size of the queue, but in busy environments
	//this function might return bogus values. 
	uint32_t size(void) const { return m_count.load(std::memory_order_relaxed);  }

	//push an element at the tail of the queue, returns true if the element was inserted in the queue. False if the queue was full
	bool push(const ELEM_T& data);

	//pop the element at the head of the queue, returns true if the element was successfully extracted from the queue. False if the queue was empty
	bool pop(ELEM_T &data);

private:
	//array to keep the elements
	ELEM_T m_queue[Q_SIZE];
	//where a new element will be inserted
	atomic_uint64_t m_writeIndex;
	//where the next element where be extracted from
	atomic_uint64_t m_readIndex;
	//published write index: the maximum write index that is fully written and safe for consumers to read
	atomic_uint64_t m_publishedWriteIndex;
	//number of elements in the queue
	mutable atomic_uint32_t m_count;

private:
	/// calculate the index in the circular array that corresponds to a particular "count" value
	inline static uint32_t _countToIndex(uint64_t count) { 
		return static_cast<uint32_t>(count & (Q_SIZE - 1)); 
	}

public:
	LockFreeQueue() :m_writeIndex(0), m_readIndex(0), m_publishedWriteIndex(0), m_count(0){}
	virtual ~LockFreeQueue() { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Impl
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ELEM_T, uint32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::push(const ELEM_T &data)
{
	uint64_t currentWriteIndex;

	do
	{
		currentWriteIndex = m_writeIndex.load(std::memory_order_acquire);
		const uint64_t currentReadIndex = m_readIndex.load(std::memory_order_acquire);
		if (currentWriteIndex - currentReadIndex >= Q_SIZE-1)
		{
			// the queue is full
			return false;
		}
	} while (!m_writeIndex.compare_exchange_weak(currentWriteIndex, (currentWriteIndex + 1), 
		std::memory_order_acq_rel, std::memory_order_acquire));

	// We know now that this index is reserved for us. Use it to save the data
	m_queue[_countToIndex(currentWriteIndex)] = data;
	// Ensure that the data is saved before updating the maximum read index
	std::atomic_thread_fence(std::memory_order_release);

	// update the published write index after saving the data. It wouldn't fail if there is only one thread 
	// inserting in the queue. It might fail if there are more than 1 producer threads because this
	// operation has to be done in the same order as the previous CAS
	for (;;)
	{
		uint64_t expectWriteIndex = currentWriteIndex;
		if (m_publishedWriteIndex.compare_exchange_weak(expectWriteIndex, (currentWriteIndex + 1), 
			std::memory_order_acq_rel, std::memory_order_acquire))
		{
			// successfully updated the published write index
			break;
		}
		// it failed. other producer thread have updated the published write index before we could.
		// We have to try again
		sys_api::thread_yield();
	}

	// The value was successfully inserted into the queue
	m_count.fetch_add(1, std::memory_order_relaxed);
	return true;
}

//-------------------------------------------------------------------------------------
template <typename ELEM_T, uint32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::pop(ELEM_T &a_data)
{
	for (;;) // keep looping to try again!
	{
		// to ensure thread-safety when there is more than 1 producer thread
		// a second index is defined (m_publishedWriteIndex)
		uint64_t currentReadIndex = m_readIndex.load(std::memory_order_acquire);
		const uint64_t currentPublishedWriteIndex = m_publishedWriteIndex.load(std::memory_order_acquire);

		if (currentReadIndex == currentPublishedWriteIndex)
		{
			// the queue is empty or
			// a producer thread has allocate space in the queue but is 
			// waiting to commit the data into it
			return false;
		}

		// Ensure that we read the data after reading the indices
		std::atomic_thread_fence(std::memory_order_acquire);
		// retrieve the data from the queue
		a_data = m_queue[_countToIndex(currentReadIndex)];

		// try to perfrom now the CAS operation on the read index. If we succeed
		// a_data already contains what m_readIndex pointed to before we 
		// increased it
		if (m_readIndex.compare_exchange_weak(currentReadIndex, (currentReadIndex + 1), 
			std::memory_order_acq_rel, std::memory_order_acquire))
		{
			// got here. The value was retrieved from the queue. Note that the
			// data inside the m_queue array is not deleted nor reseted
			m_count.fetch_sub(1, std::memory_order_relaxed);
			return true;
		}

		// it failed retrieving the element off the queue. Someone else must
		// have read the element stored at countToIndex(currentReadIndex)
		// before we could perform the CAS operation        

	}

	// Something went wrong. it shouldn't be possible to reach here
}

}
