/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>
#include "cyc_atomic.h"

namespace cyclone
{


// LockFreeQueue
// ----------------
// A bounded, lock-free FIFO queue built on top of a fixed-size node pool.
// The implementation uses a singly-linked queue (Michael-Scott style) where
// nodes are taken from and returned to an internal free list (m_poolHead).
//
// Key properties and constraints:
// - The template element type ELEM_T must be trivial, copyable/assignable and
//   nothrow-movable or nothrow-copyable/assignable (compile-time enforced).
// - Q_SIZE is the fixed number of nodes available in the pool. The queue is
//   capacity-limited by this pool: when the pool is exhausted push() returns
//   false (non-blocking try semantics).
// - The queue supports multiple producers and multiple consumers.
// - To mitigate ABA, list heads (pool head, queue head, queue tail) are
//   represented by TaggedIndex (64-bit) where the high 32 bits are a tag
//   (version) and the low 32 bits are the node index. Tags are incremented
//   on updates to reduce ABA risk.
// - The implementation favors performance: consumers optimistically read the
//   next node value before performing the CAS to advance the head. If CAS
//   fails the read is discarded. Because ELEM_T is required to be cheap to
//   copy/move this tradeoff is acceptable.
//


template <typename ELEM_T, int32_t Q_SIZE = 65536>  
class LockFreeQueue
{
public:
	// Element type constraints
	static_assert(std::is_trivial<ELEM_T>::value, "The type ELEM_T must be trivial");
	static_assert(std::is_copy_constructible<ELEM_T>::value, "The type ELEM_T must be copy constructible");
	static_assert(std::is_trivially_destructible<ELEM_T>::value, "The type ELEM_T must be trivially destructible types");
	static_assert(std::is_copy_assignable<ELEM_T>::value, "The type ELEM_T must be copy assignable");
	// Require that element construction/assignment do not throw to avoid
	// leaking nodes from the internal pool if copy/move throws during push.
	static_assert(
		std::is_nothrow_move_constructible<ELEM_T>::value || std::is_nothrow_copy_constructible<ELEM_T>::value,
		"ELEM_T must be nothrow move-constructible or nothrow copy-constructible"
	);
	static_assert(
		std::is_nothrow_move_assignable<ELEM_T>::value || std::is_nothrow_copy_assignable<ELEM_T>::value,
		"ELEM_T must be nothrow move-assignable or nothrow copy-assignable"
	);
	static_assert(Q_SIZE > 2, "Q_SIZE must be greater than 2");

	// Enqueue an element. Returns true on success, false if the queue is full
	// (no free nodes available).
	bool push(const ELEM_T& data);

	// Dequeue an element. Returns true and fills `data` on success, false if
	// the queue is empty.
	bool pop(ELEM_T &data);

	// Approximate size (may be slightly stale under concurrency).
	size_t size() const {
		return m_size.load(std::memory_order_acquire);
	}

private:
	static constexpr int32_t kEmptyIndex = -1;
	typedef uint64_t TaggedIndex; // high 32 bits: tag, low 32 bits: index

	// Node stored in the fixed pool. `next` holds the index of the next node
	// in a list (either the queue or the free pool). `value` holds element data.
	struct Node
	{
		std::atomic<int32_t> next;
		ELEM_T value;
	};

private:
	// Head and tail of the queue (both TaggedIndex to mitigate ABA on updates).
	std::atomic<TaggedIndex> m_head;
	std::atomic<TaggedIndex> m_tail;
	// Approximate number of elements in the queue.
	std::atomic<size_t> m_size;

private:
	// Helpers to pack/unpack a TaggedIndex composed of (tag, index).
	static TaggedIndex pack(int32_t idx, uint32_t tag) {
		return (static_cast<uint64_t>(tag) << 32) | static_cast<uint32_t>(idx);
	}

	static int32_t unpack_index(TaggedIndex packed) {
		return static_cast<int32_t>(packed & 0xFFFFFFFFu);
	}

	static uint32_t unpack_tag(TaggedIndex packed) {
		return static_cast<uint32_t>(packed >> 32);
	}

	// Allocate a free node index from the internal pool. Returns kEmptyIndex
	// if the pool is exhausted. Uses a lock-free pop from m_poolHead.
	int32_t alloc();
	// Return a node index to the pool using a lock-free push to m_poolHead.
	void free(int32_t idx);

	// Pool head uses TaggedIndex (tag + index) to mitigate ABA on the free list.
	std::atomic<TaggedIndex> m_poolHead;
	// Fixed-size node pool storage.
	Node m_nodePool[size_t(Q_SIZE)];

public:
	// Construct the queue and initialize the pool and the queue sentinel node.
	LockFreeQueue();
	virtual ~LockFreeQueue();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Impl
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
// - Initializes the internal free pool as a singly-linked list of indices
//   (m_nodePool[Q_SIZE-1] -> ... -> m_nodePool[0]). Then alloc() is called
//   to remove one node to use as the dummy sentinel for the MS queue.
template <typename ELEM_T, int32_t Q_SIZE>
LockFreeQueue<ELEM_T, Q_SIZE>::LockFreeQueue()
{
	//init memory pool
	for (int32_t i = 0; i < Q_SIZE; ++i)
	{
		int32_t nextIdx = (i == Q_SIZE-1) ? kEmptyIndex : (i + 1);
		m_nodePool[i].next.store(nextIdx, std::memory_order_relaxed);
	}
	m_poolHead.store(pack(0, 0), std::memory_order_relaxed);

	//init queue
	int32_t dummyIndex = alloc();
	Node& dummyNode = m_nodePool[dummyIndex];

	dummyNode.next.store(kEmptyIndex, std::memory_order_relaxed);

	m_head.store(pack(dummyIndex, 0), std::memory_order_relaxed);
	m_tail.store(pack(dummyIndex, 0), std::memory_order_relaxed);
	m_size.store(0, std::memory_order_relaxed);
}

// Destructor
// - Does not attempt to destroy elements or drain the queue. ELEM_T must be
//   trivially destructible per class constraints.
template <typename ELEM_T, int32_t Q_SIZE>
LockFreeQueue<ELEM_T, Q_SIZE>::~LockFreeQueue()
{

}

// push
// - Allocate a node from the pool. If the pool is empty return false.
// - Construct/assign the value into the node, set next = kEmptyIndex, then
//   attempt to link the node at the current tail using CAS on tail->next.
// - On success, try to swing m_tail to the new node (best-effort) and update
//   the approximate size.
// Note: element assignment must not throw (enforced by static_assert) to
// avoid leaking the allocated node.
template <typename ELEM_T, int32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::push(const ELEM_T& data)
{
	int32_t newIdx = alloc();
	if(newIdx < 0 || newIdx >= Q_SIZE) return false; //queue full	

	Node& newNode = m_nodePool[newIdx];
	
	newNode.value = data;
	std::atomic_thread_fence(std::memory_order_release); // Ensure value is visible before next update.

	newNode.next.store(kEmptyIndex, std::memory_order_relaxed);

	for (;;) 
	{
		TaggedIndex tailSnap = m_tail.load(std::memory_order_acquire);
		Node& tailNode = m_nodePool[unpack_index(tailSnap)];
		int32_t nextIdx = tailNode.next.load(std::memory_order_acquire);

		if (tailSnap != m_tail.load(std::memory_order_acquire))
		{
			continue; // Tail moved; retry.
		}

		if (nextIdx == kEmptyIndex)
		{
			// Attempt to link the new node at the current tail.
			if (tailNode.next.compare_exchange_weak(nextIdx, newIdx,
				std::memory_order_acq_rel, std::memory_order_acquire)) 
			{
				// Swing tail to the new node; failure is acceptable.
				TaggedIndex newTail = pack(newIdx, unpack_tag(tailSnap) + 1);
				m_tail.compare_exchange_strong(tailSnap, newTail, 
					std::memory_order_acq_rel,std::memory_order_acquire);
				m_size.fetch_add(1, std::memory_order_release);
				return true;
			}
		}
		else 
		{
			// Tail was not pointing to the last node, try to advance it.
			TaggedIndex newTail = pack(nextIdx, unpack_tag(tailSnap) + 1);
			m_tail.compare_exchange_weak(tailSnap, newTail,
				std::memory_order_acq_rel, std::memory_order_acquire);
		}
	}
}

// pop
// - Snapshot head/tail and inspect head->next. If head->next == empty, the
//   queue is empty. Otherwise read the value from head->next and attempt to
//   advance the head via CAS. On success, put the old head node back to the
//   pool and return the read value.
// - The implementation reads the node value before advancing head (optimistic
//   read). If CAS fails the read is discarded and the operation is retried.
template <typename ELEM_T, int32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::pop(ELEM_T& data)
{
	for (;;) 
	{
		TaggedIndex headSnap = m_head.load(std::memory_order_acquire);
		TaggedIndex tailSnap = m_tail.load(std::memory_order_acquire);

		int32_t headIdx = unpack_index(headSnap);
		Node& headNode = m_nodePool[headIdx];
		int32_t nextIdx = headNode.next.load(std::memory_order_acquire);

		if (headSnap != m_head.load(std::memory_order_acquire))
		{
			continue; // Head moved; retry.
		}

		if (nextIdx == kEmptyIndex)
		{
			return false; // Queue empty.
		}

		if (headIdx == unpack_index(tailSnap))
		{
			// Tail is falling behind, try to advance it.
			TaggedIndex newTail = pack(nextIdx, unpack_tag(tailSnap) + 1);
			m_tail.compare_exchange_weak(tailSnap, newTail,
				std::memory_order_acq_rel, std::memory_order_acquire);
			continue;
		}

		Node& nextNode = m_nodePool[nextIdx];

		std::atomic_thread_fence(std::memory_order_acquire); // Ensure we see the value written before next was linked.
		ELEM_T value = nextNode.value;

		TaggedIndex newHead = pack(nextIdx, unpack_tag(headSnap) + 1);
		if (m_head.compare_exchange_weak(headSnap, newHead,
			std::memory_order_acq_rel, std::memory_order_acquire)) 
		{
			m_size.fetch_sub(1, std::memory_order_release);
			data = std::move(value);

			free(headIdx);
			return true;
		}
	}
}

// alloc
// - Pop an index from the internal free pool (m_poolHead). Returns
//   kEmptyIndex if the pool is empty.
template <typename ELEM_T, int32_t Q_SIZE>
int32_t LockFreeQueue<ELEM_T, Q_SIZE>::alloc() 
{
	for (;;) 
	{
		TaggedIndex head = m_poolHead.load(std::memory_order_acquire);
		int32_t idx = unpack_index(head);
		if (idx == kEmptyIndex) return kEmptyIndex;

		uint32_t tag = unpack_tag(head);
		int32_t nextIdx = m_nodePool[idx].next.load(std::memory_order_relaxed);
		TaggedIndex desired = pack(nextIdx, tag + 1);
		if (m_poolHead.compare_exchange_weak(head, desired,
			std::memory_order_acq_rel, std::memory_order_acquire)) 
		{
			return idx;
		}
	}
}

// free
// - Push an index back to the internal free pool (m_poolHead) using a
//   lock-free CAS loop. The node's next field is set to the current head
//   index before the CAS.
template <typename ELEM_T, int32_t Q_SIZE>
void LockFreeQueue<ELEM_T, Q_SIZE>::free(int32_t idx)
{
	if (idx < 0 || idx >= Q_SIZE) return;

	Node& node = m_nodePool[idx];
	for (;;) 
	{
		TaggedIndex head = m_poolHead.load(std::memory_order_acquire);
		int32_t headIdx = unpack_index(head);
		node.next.store(headIdx, std::memory_order_release);

		TaggedIndex newHead = pack(idx, unpack_tag(head) + 1);
		if (m_poolHead.compare_exchange_weak(head, newHead,
			std::memory_order_acq_rel, 	std::memory_order_acquire)) 
		{
			return;
		}
	}
}

}
