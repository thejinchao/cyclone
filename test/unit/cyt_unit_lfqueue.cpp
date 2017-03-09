#include <cy_core.h>
#include <gtest/gtest.h>

using namespace cyclone;

namespace {
//-------------------------------------------------------------------------------------
TEST(LockFreeQueue, Basic)
{
	const int32_t QUEUE_SIZE = 32;
	typedef LockFreeQueue<int32_t, QUEUE_SIZE> IntQueue;
	IntQueue queue;

	EXPECT_EQ(0ll, queue.size());

	int32_t pop_num;
	bool have_data = queue.pop(pop_num);
	EXPECT_FALSE(have_data);
	EXPECT_EQ(0ll, queue.size());

	int32_t push_num = (int32_t)rand();
	bool push_ret = queue.push(push_num);
	EXPECT_EQ(1ll, queue.size());
	EXPECT_TRUE(push_ret);

	have_data = queue.pop(pop_num);
	EXPECT_EQ(pop_num, push_num);
	EXPECT_TRUE(have_data);
	EXPECT_EQ(0ll, queue.size());

	for (int32_t i = 0; i < QUEUE_SIZE-1; i++) {
		push_ret = queue.push(i);
		EXPECT_TRUE(push_ret);
		EXPECT_EQ(size_t(i+1), queue.size());
	}

	EXPECT_EQ(size_t(QUEUE_SIZE-1), queue.size());
	push_ret = queue.push(QUEUE_SIZE);
	EXPECT_FALSE(push_ret);

	for (int32_t i = 0; i < QUEUE_SIZE-1; i++) {
		have_data = queue.pop(pop_num);
		EXPECT_TRUE(have_data);
		EXPECT_EQ(i, pop_num);
		EXPECT_EQ(size_t(QUEUE_SIZE-i-2), queue.size());
	}
	EXPECT_EQ(0ll, queue.size());
}

//-------------------------------------------------------------------------------------
class MultiThreadPushPop
{
public:
	typedef LockFreeQueue<uint32_t, 1024> UIntQueue;

	struct ThreadData
	{
		MultiThreadPushPop* globalData;
		sys_api::signal_t	complete;
		char				name[32];
		size_t				workCounts;
	};

	void pushAndPop(void) {
		m_queue = new UIntQueue();
		m_result = new std::atomic_flag[m_topValue + 1];
		for (uint32_t i = 0; i <= m_topValue; i++) {
			m_result[i].clear();
		}

		ThreadData** pushThreadData = new ThreadData*[m_pushThreads];
		ThreadData** popThreadData = new ThreadData*[m_popThreads];

		for (int32_t i = 0; i < m_pushThreads; i++) {
			pushThreadData[i] = new ThreadData();
			pushThreadData[i]->globalData = this;
			pushThreadData[i]->workCounts = 0;
			pushThreadData[i]->complete = sys_api::signal_create();
			snprintf(pushThreadData[i]->name, 32, "push%02d", i);
		}
		for (int32_t i = 0; i < m_popThreads; i++) {
			popThreadData[i] = new ThreadData();
			popThreadData[i]->globalData = this;
			popThreadData[i]->workCounts = 0;
			popThreadData[i]->complete = sys_api::signal_create();
			snprintf(popThreadData[i]->name, 32, "pop%02d", i);
		}

		if (m_pushFirst) {
			for (int32_t i = 0; i < m_pushThreads; i++) {
				sys_api::thread_create_detached(MultiThreadPushPop::pushFunction, pushThreadData[i], pushThreadData[i]->name);
			}

			for (int32_t i = 0; i < m_popThreads; i++) {
				sys_api::thread_create_detached(MultiThreadPushPop::popFunction, popThreadData[i], popThreadData[i]->name);
			}
		}
		else {
			for (int32_t i = 0; i < m_popThreads; i++) {
				sys_api::thread_create_detached(MultiThreadPushPop::popFunction, popThreadData[i], popThreadData[i]->name);
			}

			for (int32_t i = 0; i < m_pushThreads; i++) {
				sys_api::thread_create_detached(MultiThreadPushPop::pushFunction, pushThreadData[i], pushThreadData[i]->name);
			}
		}

		//wait all push thread
		double averageCounts = 0.0;
		for (int32_t i = 0; i < m_pushThreads; i++) {
			sys_api::signal_wait(pushThreadData[i]->complete);
			averageCounts += (double)pushThreadData[i]->workCounts;
		}	
		m_noMoreData = true;

		averageCounts /= m_pushThreads;

#if 0
		std::string pushWorkReport = "PushReport: ";
		for (int32_t i = 0; i < m_pushThreads; i++) {
			char temp[32] = { 0 };
			snprintf(temp, 32, "%.3f%%,", ((double)pushThreadData[i]->workCounts - averageCounts)*100.0 / averageCounts);

			pushWorkReport += temp;
		}
		printf("%s\n", pushWorkReport.c_str());
#endif

		//wait all pop thread
		averageCounts = 0.0;
		for (int32_t i = 0; i < m_popThreads; i++) {
			sys_api::signal_wait(popThreadData[i]->complete);
			averageCounts += (double)popThreadData[i]->workCounts;
		}
		averageCounts /= m_popThreads;
#if 0
		std::string popWorkReport = "PopReport: ";
		for (int32_t i = 0; i < m_popThreads; i++) {
			char temp[32] = { 0 };
			snprintf(temp, 32, "%.3f%%,", ((double)popThreadData[i]->workCounts - averageCounts)*100.0 / averageCounts);

			popWorkReport += temp;
		}
		printf("%s\n", popWorkReport.c_str());
#endif
		//check result
		for (uint32_t i = 1; i <= m_topValue; i++) {
			EXPECT_TRUE(m_result[i].test_and_set());
		}

		//free memory
		for (int32_t i = 0; i < m_pushThreads; i++) {
			delete pushThreadData[i];
		}
		for (int32_t i = 0; i < m_popThreads; i++) {
			delete popThreadData[i];
		}
		delete[] pushThreadData;
		delete[] popThreadData;
		delete[] m_result;
		delete m_queue;
	}

	static void pushFunction(void* param)
	{
		ThreadData* threadData = (ThreadData*)param;
		MultiThreadPushPop* pThis = threadData->globalData;
		pThis->_pushFunction(threadData);
	}

	static void popFunction(void* param)
	{
		ThreadData* threadData = (ThreadData*)param;
		MultiThreadPushPop* pThis = threadData->globalData;
		pThis->_popFunction(threadData);
	}

private:
	void _pushFunction(ThreadData* threadData) {
		uint32_t nextValue = m_currentValue++;
		while (nextValue <= m_topValue) {
			EXPECT_GE(nextValue, 1u);
			EXPECT_LE(nextValue, m_topValue);
			while (!(m_queue->push(nextValue))) {
				sys_api::thread_yield();
			}

			threadData->workCounts++;
			nextValue = m_currentValue++;
		}

		//Complete
		sys_api::signal_notify(threadData->complete);
	}

	void _popFunction(ThreadData* threadData) {
		for (;;) {
			uint32_t pop_value;
			bool pop_result = m_queue->pop(pop_value);

			if (pop_result) {
				EXPECT_GE(pop_value, 1u);
				EXPECT_LE(pop_value, m_topValue);
				EXPECT_FALSE(m_result[pop_value].test_and_set());
				threadData->workCounts++;
			}
			else {
				if (m_noMoreData) break;

				//minor wait when queue is empty
				sys_api::thread_yield();
			}
		}
		//Complete
		sys_api::signal_notify(threadData->complete);
	}

private:
	UIntQueue*	m_queue;
	std::atomic_flag* m_result;

	const uint32_t m_topValue;
	atomic_uint32_t m_currentValue;

	int32_t m_pushThreads;
	int32_t m_popThreads;

	const bool m_pushFirst;

	atomic_bool_t m_noMoreData;

public:
	MultiThreadPushPop(uint32_t topValue, int32_t pushThreads, int32_t popThreads, bool pushFirst)
		: m_queue(0)
		, m_result(0)
		, m_topValue(topValue)
		, m_currentValue(1)
		, m_pushThreads(pushThreads)
		, m_popThreads(popThreads)
		, m_pushFirst(pushFirst)
		, m_noMoreData(false)
	{
	}

	~MultiThreadPushPop()
	{
	}
};

//-------------------------------------------------------------------------------------
TEST(LockFreeQueue, MultiThread)
{
	MultiThreadPushPop test1(100u, 1, 1, true);
	test1.pushAndPop();
	
	MultiThreadPushPop test2(100, 1, 1, false);
	test2.pushAndPop();
	
	MultiThreadPushPop test3(100000u, 3, 5, true);
	test3.pushAndPop();

	MultiThreadPushPop test4(100000u, 3, 5, false);
	test4.pushAndPop();

	MultiThreadPushPop test5(100000u, 5, 2, false);
	test5.pushAndPop();

	MultiThreadPushPop test6(100000u, 5, 2, false);
	test6.pushAndPop();
}

}
