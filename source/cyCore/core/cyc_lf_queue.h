/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>
#include "cyc_atomic.h"

namespace cyclone
{

//
// Yet another implementation of a lock-free circular array queue
// http://www.codeproject.com/Articles/153898/Yet-another-implementation-of-a-lock-free-circular
//

template <typename ELEM_T, uint32_t Q_SIZE = 65536>  //default 2^16, it must be a power of 2 value
class LockFreeQueue
{
public:
	//returns the current number of items in the queue
	//It tries to take a snapshot of the size of the queue, but in busy environments
	//this function might return bogus values. 
	uint32_t size(void) const { return m_count.load();  }

	//push an element at the tail of the queue, returns true if the element was inserted in the queue. False if the queue was full
	bool push(const ELEM_T& data);

	//pop the element at the head of the queue, returns true if the element was successfully extracted from the queue. False if the queue was empty
	bool pop(ELEM_T &data);

private:
	//array to keep the elements
	ELEM_T m_queue[Q_SIZE];
	//number of elements in the queue
	mutable atomic_uint32_t m_count;
	//where a new element will be inserted
	atomic_uint32_t m_writeIndex;
	//where the next element where be extracted from
	atomic_uint32_t m_readIndex;
	//maximum read index for multiple producer queues
	atomic_uint32_t m_maximumReadIndex;

private:
	/// calculate the index in the circular array that corresponds to a particular "count" value
	inline uint32_t _countToIndex(uint32_t count) { return (count & (Q_SIZE - 1)); }

public:
	LockFreeQueue() : m_count(0), m_writeIndex(0), m_readIndex(0), m_maximumReadIndex(0) {}
	virtual ~LockFreeQueue() { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Impl
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ELEM_T, uint32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::push(const ELEM_T &data)
{
	uint32_t currentReadIndex;
	uint32_t currentWriteIndex;

	do
	{
		currentWriteIndex = m_writeIndex;
		currentReadIndex = m_readIndex;
		if (_countToIndex(currentWriteIndex + 1) ==
			_countToIndex(currentReadIndex))
		{
			if (m_count > (Q_SIZE >> 1))
				// the queue is full
				return false;
			else
				//maybe this thread was blocked between m_write_index.get() and m_read_index.get(), cause other threads write and read
				continue;
		}
	} while (!atomic_compare_exchange(m_writeIndex, currentWriteIndex, (currentWriteIndex + 1)));

	// We know now that this index is reserved for us. Use it to save the data
	m_queue[_countToIndex(currentWriteIndex)] = data;

	// update the maximum read index after saving the data. It wouldn't fail if there is only one thread 
	// inserting in the queue. It might fail if there are more than 1 producer threads because this
	// operation has to be done in the same order as the previous CAS
	while (!atomic_compare_exchange(m_maximumReadIndex, currentWriteIndex, (currentWriteIndex + 1)))
	{
		// this is a good place to yield the thread in case there are more
		// software threads than hardware processors and you have more
		// than 1 producer thread
		sys_api::thread_yield();
	}

	// The value was successfully inserted into the queue
	m_count++;
	return true;
}


//-------------------------------------------------------------------------------------
template <typename ELEM_T, uint32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::pop(ELEM_T &a_data)
{
	uint32_t currentMaximumReadIndex;
	uint32_t currentReadIndex;

	for (;;) // keep looping to try again!
	{
		// to ensure thread-safety when there is more than 1 producer thread
		// a second index is defined (m_maximumReadIndex)
		currentReadIndex = m_readIndex;
		currentMaximumReadIndex = m_maximumReadIndex;

		if (_countToIndex(currentReadIndex) ==
			_countToIndex(currentMaximumReadIndex))
		{
			// the queue is empty or
			// a producer thread has allocate space in the queue but is 
			// waiting to commit the data into it
			return false;
		}

		// retrieve the data from the queue
		a_data = m_queue[_countToIndex(currentReadIndex)];

		// try to perfrom now the CAS operation on the read index. If we succeed
		// a_data already contains what m_readIndex pointed to before we 
		// increased it
		if (atomic_compare_exchange(m_readIndex, currentReadIndex, (currentReadIndex + 1)))
		{
			// got here. The value was retrieved from the queue. Note that the
			// data inside the m_queue array is not deleted nor reseted
			m_count--;
			return true;
		}

		// it failed retrieving the element off the queue. Someone else must
		// have read the element stored at countToIndex(currentReadIndex)
		// before we could perform the CAS operation        

	}

	// Something went wrong. it shouldn't be possible to reach here
}

}
