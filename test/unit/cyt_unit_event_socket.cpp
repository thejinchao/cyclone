#include <cy_event.h>
#include <cy_crypt.h>
#include "cyt_event_fortest.h"

#include "cyt_unit_utils.h"

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
class SocketPair
{
public:
	socket_t m_fd[2];
	RingBuf rb;
	Looper::event_id_t event_id;
	void* param;
	atomic_uint64_t write_counts;

public:
	SocketPair(void* _param) : param(_param) , write_counts(0){
		Pipe::construct_socket_pipe(m_fd);
	}
	~SocketPair() {
		Pipe::destroy_socket_pipe(m_fd);
	}
};

typedef std::vector< SocketPair* > SocketPairVector;

//-------------------------------------------------------------------------------------
struct ReadThreadData
{
	EventLooper_ForTest* looper;
	SocketPairVector socketPairs;
	sys_api::signal_t ready_signal;
	uint32_t socket_counts;

	sys_api::signal_t write_to_full_signal;

	uint32_t active_counts;
	atomic_uint32_t actived_counts;
	sys_api::signal_t read_done_signal;

	uint32_t close_counts;
	atomic_uint32_t closed_counts;
	sys_api::signal_t close_done_signal;
};

//-------------------------------------------------------------------------------------
static void _onRead(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param)
{
	(void)event;

	SocketPair* socketPair = (SocketPair*)param;
	ReadThreadData* data = (ReadThreadData*)(socketPair->param);

	ssize_t read_size = socketPair->rb.read_socket(fd);
	if (read_size > 0) {
		data->actived_counts += (uint32_t)((uint32_t)read_size / sizeof(uint64_t));

		if (data->actived_counts.load() >= data->active_counts) {
			sys_api::signal_notify(data->read_done_signal);
		}
	}
	else {
		data->looper->disable_all(id);

		(data->closed_counts)++;

		if (data->closed_counts.load() >= data->close_counts) {
			sys_api::signal_notify(data->close_done_signal);
		}
	}
}

//-------------------------------------------------------------------------------------
static void _readThreadFunction(void* param)
{
	ReadThreadData* data = (ReadThreadData*)param;
	EventLooper_ForTest* looper = new EventLooper_ForTest();
	data->looper = looper;

	for (size_t i = 0; i < data->socketPairs.size(); i++) {
		SocketPair* sp = data->socketPairs[i];

		sp->event_id = looper->register_event(sp->m_fd[0], Looper::kRead, sp, _onRead, 0);
	}

	sys_api::signal_notify(data->ready_signal);
	looper->loop();

	delete looper;
	data->looper = nullptr;
}

//-------------------------------------------------------------------------------------
struct WriteThreadData
{
	EventLooper_ForTest* looper;
	SocketPairVector socketPairs;
	sys_api::signal_t ready_signal;
	uint32_t socket_counts;

	sys_api::signal_t loop_stoped_signal;
	sys_api::signal_t quit_signal;
};

//-------------------------------------------------------------------------------------
static void _onWrite(Looper::event_id_t id, socket_t fd, Looper::event_t event, void* param)
{
	(void)id;
	(void)fd;
	(void)event;

	SocketPair* socketPair = (SocketPair*)param;

	(socketPair->write_counts)++;
}

//-------------------------------------------------------------------------------------
static void _writeThreadFunction(void* param)
{
	WriteThreadData* data = (WriteThreadData*)param;
	EventLooper_ForTest* looper = new EventLooper_ForTest();
	data->looper = looper;

	for (size_t i = 0; i < data->socketPairs.size(); i++) {
		SocketPair* sp = data->socketPairs[i];
		sp->event_id = looper->register_event(sp->m_fd[0], Looper::kWrite, sp, 0, _onWrite);
	}

	sys_api::signal_notify(data->ready_signal);

	looper->loop();

	sys_api::signal_notify(data->loop_stoped_signal);
	sys_api::signal_wait(data->quit_signal);

	delete looper;
	data->looper = nullptr;
}

//-------------------------------------------------------------------------------------
TEST_CASE("ReadAndCloseSocket test for EventLooper", "[EventLooper]")
{
	ReadThreadData data;
	data.ready_signal = sys_api::signal_create();
	data.read_done_signal = sys_api::signal_create();
	data.close_done_signal = sys_api::signal_create();

	//read event
	{
		data.socket_counts = 100;
		data.active_counts = 10;
		data.actived_counts = 0;
		data.close_counts = 0;
		data.closed_counts = 0;

		XorShift128 rndSend, rndCheck;
		rndSend.make();
		rndCheck = rndSend;

		for (size_t i = 0; i < data.socket_counts; i++) {
			SocketPair* sp = new SocketPair(&data);
			data.socketPairs.push_back(sp);
		}

		thread_t thread = sys_api::thread_create(_readThreadFunction, &data, "looper_socket");
		sys_api::signal_wait(data.ready_signal);

		//send to some of socket
		std::vector< size_t > activeIDs;
		for (size_t i = 0; i < data.active_counts; i++) {
			size_t index = (size_t)rand() % data.socket_counts;

			uint64_t sndData = rndSend.next();
			REQUIRE_EQ(sizeof(sndData), (size_t)socket_api::write(data.socketPairs[index]->m_fd[1], (const char*)&sndData, sizeof(sndData)));
			activeIDs.push_back(index);
		}
		REQUIRE_EQ(data.active_counts, activeIDs.size());

		sys_api::signal_wait(data.read_done_signal);

		//begin check
		REQUIRE_GE(data.actived_counts.load(), data.active_counts);
		for (size_t i = 0; i < data.active_counts; i++) {
			size_t index = activeIDs[i];
			uint64_t rcvData = 0;
			REQUIRE_EQ(sizeof(rcvData), data.socketPairs[index]->rb.memcpy_out(&rcvData, sizeof(rcvData)));
			REQUIRE_EQ(rcvData, rndCheck.next());
		}
		activeIDs.clear();
		for (size_t i = 0; i < data.socket_counts; i++) {
			REQUIRE_TRUE(data.socketPairs[i]->rb.empty());
		}

		REQUIRE_GE(data.looper->get_loop_counts(), 1ull);
		REQUIRE_LE(data.looper->get_loop_counts(), data.active_counts + 1); //read event(s) + inner pipe register event

		//quit...
		data.looper->push_stop_request();
		sys_api::thread_join(thread);

		for (size_t i = 0; i < data.socketPairs.size(); i++) {
			delete data.socketPairs[i];
		}
		data.socketPairs.clear();
	}

	//close event 
	{
		data.socket_counts = 100;
		data.active_counts = 0;
		data.actived_counts = 0;
		data.close_counts = 1;
		data.closed_counts = 0;

		for (size_t i = 0; i < data.socket_counts; i++) {
			SocketPair* sp = new SocketPair(&data);
			data.socketPairs.push_back(sp);
		}

		thread_t thread = sys_api::thread_create(_readThreadFunction, &data, "looper_socket");
		sys_api::signal_wait(data.ready_signal);

		//remember all ids
		std::set < Looper::event_id_t > allIDs;
		for (size_t i = 0; i < data.socket_counts; i++) {
			allIDs.insert(data.socketPairs[i]->event_id);
		}

		//close some socket
		std::set < Looper::event_id_t > closeIDs;
		for (size_t i = 0; i < data.close_counts; i++) {
			size_t index = (size_t)rand() % data.socket_counts;

			while (closeIDs.find(data.socketPairs[index]->event_id) != closeIDs.end()) {
				index = (size_t)rand() % data.socket_counts;
			}

			socket_api::close_socket(data.socketPairs[index]->m_fd[1]);
			data.socketPairs[index]->m_fd[1] = INVALID_SOCKET;

			closeIDs.insert(data.socketPairs[index]->event_id);
		}
		REQUIRE_EQ(data.close_counts, closeIDs.size());
		sys_api::signal_wait(data.close_done_signal);

		//begin check
		const auto& channel_buf = data.looper->get_channel_buf();
		REQUIRE_EQ(data.closed_counts.load(), data.close_counts);
		REQUIRE_EQ(data.socket_counts - data.close_counts, (uint32_t)data.looper->get_active_channel_counts() - 1);
		for (size_t i = 0; i < channel_buf.size(); i++) {
			const auto& channel = channel_buf[i];

			if (closeIDs.end() != closeIDs.find(channel.id)) {
				REQUIRE_FALSE(channel.active);
			}
			else {
				if (allIDs.end() != allIDs.find(channel.id)) {
					REQUIRE_TRUE(channel.active);
				}
			}
		}

		REQUIRE_GE(data.looper->get_loop_counts(), 1ull);
		REQUIRE_LE(data.looper->get_loop_counts(), data.close_counts + 1); //close event(s) + inner pipe register event

		//quit...
		data.looper->push_stop_request();
		sys_api::thread_join(thread);

		for (size_t i = 0; i < data.socketPairs.size(); i++) {
			delete data.socketPairs[i];
		}
		data.socketPairs.clear();
	}

	sys_api::signal_destroy(data.ready_signal);
	sys_api::signal_destroy(data.read_done_signal);
	sys_api::signal_destroy(data.close_done_signal);
}

//-------------------------------------------------------------------------------------
TEST_CASE("WriteSocket test for EventLooper", "[EventLooper]")
{
	WriteThreadData data;
	data.ready_signal = sys_api::signal_create();
	data.loop_stoped_signal = sys_api::signal_create();
	data.quit_signal = sys_api::signal_create();

	//write test
	{
		data.socket_counts = 10;

		for (size_t i = 0; i < data.socket_counts; i++) {
			SocketPair* sp = new SocketPair(&data);
			data.socketPairs.push_back(sp);
		}

		thread_t thread = sys_api::thread_create(_writeThreadFunction, &data, "looper_socket");
		sys_api::signal_wait(data.ready_signal);

		sys_api::thread_sleep(100); //fly some time

		//quit...
		data.looper->push_stop_request();
		sys_api::signal_wait(data.loop_stoped_signal);
		
		uint64_t loop_counts = data.looper->get_loop_counts();

		sys_api::signal_notify(data.quit_signal);
		sys_api::thread_join(thread);

		for (size_t i = 0; i < data.socketPairs.size(); i++) {
			REQUIRE_GE(data.socketPairs[i]->write_counts.load(), loop_counts - 1);
			REQUIRE_LE(data.socketPairs[i]->write_counts.load(), loop_counts);
			delete data.socketPairs[i];
		}
		data.socketPairs.clear();
	}

	sys_api::signal_destroy(data.ready_signal);
	sys_api::signal_destroy(data.loop_stoped_signal);
	sys_api::signal_destroy(data.quit_signal);
}

}
