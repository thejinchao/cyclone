#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>
#include <utility/cyu_statistics.h>
#include <utility/cyu_string_util.h>

#include <fstream>

#include "ft_common.h"

using namespace cyclone;
using namespace std::placeholders;

enum { OPT_PORT, OPT_FILE_PATH, OPT_THREADS, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_FILE_PATH, "-f",  SO_REQ_SEP }, // "-f FILE_PATH"
	{ OPT_THREADS, "-t",  SO_REQ_SEP }, // "-t THREAD_COUNTS"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

////////////////////////////////////////////////////////////////////////////////////////////
static void printUsage(const char* moduleName)
{
	printf("===== FileTransfer Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -p  LISTEN_PORT\t Local Listen Port, Default 3000\n");
	printf("\t -f FILE_PATH\tFile path name to be transmitted\n");
	printf("\t -t THREAD_COUNTS\tWork thread counts(default is cpu core counts)\n");
	printf("\t --help -?\tShow this help\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
class FileTransferServer
{
private:
	enum ThreadStatus
	{
		TS_Idle,
		TS_Connected,
		TS_Sending,
		TS_Completed,
	};
	struct ThreadContext
	{
		ThreadStatus status;
		std::ifstream fileHandle;
		size_t offsetBegin;
		size_t offsetNow;
		size_t offsetEnd;
		uint32_t fragmentCRC;
		enum { BUFFER_SIZE = 64*1024 };
		char* buffer;
		ConnectionPtr conn;
		std::atomic<float> sendSpeed;

		ThreadContext() {
			status = TS_Idle;
			offsetBegin = offsetNow = offsetEnd = 0;
			fragmentCRC = INITIAL_ADLER;
			buffer = nullptr;
			sendSpeed = 0.f;
		}
	};

	std::string m_strPathName;
	std::string m_strFileName;
	size_t m_fileSize;
	TcpServer m_server;
	atomic_int32_t m_workingCounts;
	std::vector<ThreadContext*> m_threadContext;

private:
	void _onMasterThreadStart(Looper* looper)
	{
#if CY_ENABLE_DEBUG
		looper->register_timer_event(1000, nullptr, std::bind(&FileTransferServer::_onMasterThreadTimer, this));
#endif
	}

	void _onMasterThreadTimer()
	{
#if CY_ENABLE_DEBUG
		if (m_workingCounts.load()==0) return;

		std::string speedStatus;
		for(int32_t i=0; i<(int32_t)m_threadContext.size(); i++) {
			ThreadContext* ctx = (ThreadContext*)m_threadContext[i];

			float speed = ctx->sendSpeed.load();

			char temp[256] = { 0 };
			std::snprintf(temp, 256, "[%d]%s/s ", i, string_util::size_to_string(speed).c_str());
			speedStatus += temp;
		}

		CY_LOG(L_INFO, "SendSpeed: %s", speedStatus.c_str());
#endif
	}

	void _onWorkThreadStart(int32_t index, Looper* looper)
	{
		assert(index >= 0 && index < (int32_t)m_threadContext.size());

		ThreadContext& ctx = *(m_threadContext[index]);
		ctx.status = TS_Idle;
		ctx.buffer = (char*)CY_MALLOC(ThreadContext::BUFFER_SIZE);
		ctx.offsetBegin = ctx.offsetEnd = ctx.offsetNow = 0;
		ctx.fragmentCRC = INITIAL_ADLER;
		ctx.sendSpeed = 0.f;

#if CY_ENABLE_DEBUG
		looper->register_timer_event(1000, nullptr, std::bind(&FileTransferServer::_onWorkThreadTimer, this, index));
#endif
	}

	void _onWorkThreadTimer(int32_t index)
	{
#if CY_ENABLE_DEBUG
		assert(index >= 0 && index < (int32_t)m_threadContext.size());
		ThreadContext& ctx = *(m_threadContext[index]);

		if (ctx.status == TS_Sending && ctx.conn) {
			ctx.sendSpeed = ctx.conn->get_write_speed();
		}
#endif
	}

	void _onClientConnected(int32_t index, ConnectionPtr conn)
	{
		assert(index >= 0 && index < (int32_t)m_threadContext.size());
		ThreadContext& ctx = *(m_threadContext[index]);

		if (ctx.status != TS_Idle) {
			conn->shutdown();
			return;
		}
		
		ctx.status = TS_Connected;
		ctx.conn = conn;

		this->m_workingCounts += 1;
	}

	void _onMessage_QueryFileInfo(int32_t , const FT_Head&head, ConnectionPtr conn)
	{
		RingBuf& ringBuf = conn->get_input_buf();
		ringBuf.discard(head.size);

		//send reply
		int32_t totalSize = (int32_t)(sizeof(FT_ReplyFileInfo) + m_strFileName.length() + 1);

		FT_ReplyFileInfo* reply = (FT_ReplyFileInfo*)CY_MALLOC(totalSize);
		reply->id = FT_ReplyFileInfo::ID;
		reply->size = totalSize;
		reply->threadCounts = (int)m_threadContext.size();
		reply->fileSize = m_fileSize;
		reply->nameLength = (int32_t)m_strFileName.length()+1;
		memcpy(((char*)reply) + sizeof(FT_ReplyFileInfo), m_strFileName.c_str(), m_strFileName.length() + 1);
		
		conn->send((const char*)reply, totalSize);

		CY_FREE(reply);
	}

	void _onSendReady(int32_t index, ConnectionPtr conn)
	{
		ThreadContext& ctx = *(m_threadContext[index]);
		assert(ctx.status == TS_Sending);

		ctx.fileHandle.seekg(ctx.offsetNow);
		size_t readSize = ctx.fileHandle.read(ctx.buffer, ThreadContext::BUFFER_SIZE).gcount();
		if (ctx.offsetNow + readSize > ctx.offsetEnd) {
			readSize = ctx.offsetEnd - ctx.offsetNow;
		}
		ctx.offsetNow += readSize;
		ctx.fragmentCRC = cyclone::adler32(ctx.fragmentCRC, (const uint8_t*)ctx.buffer, readSize);

		conn->send(ctx.buffer, readSize);

		//end?
		if (ctx.offsetNow >= ctx.offsetEnd) {
			ctx.fileHandle.close();
			ctx.status = TS_Completed;
			conn->set_on_send_complete(nullptr);

			CY_LOG(L_INFO, "Send fragment completed, crc=0x%08x, sendBufMaxSize=%s", ctx.fragmentCRC, 
#if CY_ENABLE_DEBUG
				string_util::size_to_string(conn->get_writebuf_max_size()).c_str()
#else
				"na"
#endif
			);

			//send last message
			FT_ReplyFileFragment_End fileEnd;
			fileEnd.id = FT_ReplyFileFragment_End::ID;
			fileEnd.size = sizeof(FT_ReplyFileFragment_End);
			fileEnd.fragmentCRC = ctx.fragmentCRC;

			conn->send((const char*)&fileEnd, sizeof(fileEnd));
		}
	}

	void _onMessage_RequireFileFragment(int32_t index, const FT_Head& , ConnectionPtr conn)
	{
		RingBuf& ringBuf = conn->get_input_buf();
		FT_RequireFileFragment* require = (FT_RequireFileFragment*)ringBuf.normalize();
		ThreadContext& ctx = *(m_threadContext[index]);

		if (require->fileOffset + require->fragmentSize > m_fileSize) {
			CY_LOG(L_INFO, "Receive invalid fragment download request, offset=%zd, size=%d", require->fileOffset, require->size);
			conn->shutdown();
			return;
		}

		assert(ctx.status == TS_Connected);
		ctx.fileHandle.open(m_strPathName.c_str(), std::ios::in | std::ios::binary);
		if (ctx.fileHandle.fail()) {
			CY_LOG(L_INFO, "Can't open file %s", m_strPathName.c_str());
			conn->shutdown();
			return;
		}

		ctx.status = TS_Sending;
		ctx.offsetNow = ctx.offsetBegin = require->fileOffset;
		ctx.offsetEnd = ctx.offsetBegin + require->fragmentSize;
		ctx.fragmentCRC = INITIAL_ADLER;
		ctx.sendSpeed = 0.f;

		//send fragment begin 
		FT_ReplyFileFragment_Begin fileBegin;
		fileBegin.id = FT_ReplyFileFragment_Begin::ID;
		fileBegin.size = sizeof(FT_ReplyFileFragment_Begin);
		fileBegin.fileOffset = require->fileOffset;
		fileBegin.fragmentSize = require->fragmentSize;

		conn->set_on_send_complete(std::bind(&FileTransferServer::_onSendReady, this, index, _1));
		conn->send((const char*)&fileBegin, sizeof(fileBegin));
	}

	void _onClientMessage(int32_t index, ConnectionPtr conn)
	{
		assert(index >= 0 && index < (int32_t)m_threadContext.size());

		if (m_threadContext[index]->status != TS_Connected) {
			conn->shutdown();
			return;
		}

		//read head
		RingBuf& ringBuf = conn->get_input_buf();
		if (ringBuf.size() < sizeof(FT_Head)) return;

		FT_Head head;
		ringBuf.peek(0, &head, sizeof(head));
		if ((int32_t)ringBuf.size() < head.size) return;

		switch (head.id) {
		case FT_RequireFileInfo::ID:
			_onMessage_QueryFileInfo(index, head, conn);
			break;

		case FT_RequireFileFragment::ID:
			_onMessage_RequireFileFragment(index, head, conn);
			break;
		}
	}

	void _onClientClose(int32_t index, ConnectionPtr conn)
	{
		assert(index >= 0 && index < (int32_t)m_threadContext.size());
		ThreadContext& ctx = *(m_threadContext[index]);

		if (ctx.status == TS_Sending) {
			ctx.fileHandle.close();
		}
		ctx.status = TS_Idle;
		ctx.conn.reset();
		ctx.sendSpeed = 0.f;

		this->m_workingCounts -= 1;
	}

public:
	bool prepare(const std::string& pathName, int32_t workThreadCounts)
	{
		m_strPathName = pathName;

		//get file name
		std::string::size_type dotPos = pathName.find_last_of("/\\");
		if (dotPos == std::string::npos) {
			m_strFileName = pathName;
		}
		else {
			m_strFileName = pathName.substr(dotPos + 1);
		}

		//get file info
		std::ifstream fileHandle;
		fileHandle.open(m_strPathName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
		if (fileHandle.fail()) 	{
			CY_LOG(L_ERROR, "Can't open file %s", m_strPathName.c_str());
			return false;
		}
		
		//get file size
		m_fileSize = (size_t)fileHandle.tellg();
		fileHandle.close();

		CY_LOG(L_DEBUG, "file to be transmitted: %s(%zd)", m_strPathName.c_str(), m_fileSize);

		//prepare work thread
		m_threadContext.resize(workThreadCounts);
		for (int32_t i = 0; i < (int32_t)m_threadContext.size(); i++) {
			m_threadContext[i] = new ThreadContext;
		}
		return true;
	}

	void startAndJoin(int16_t listenPort) 
	{
		m_server.m_listener.on_master_thread_start = std::bind(&FileTransferServer::_onMasterThreadStart, this, _2);
		m_server.m_listener.on_work_thread_start = std::bind(&FileTransferServer::_onWorkThreadStart, this, _2, _3);
		m_server.m_listener.on_connected = std::bind(&FileTransferServer::_onClientConnected, this, _2, _3);
		m_server.m_listener.on_message = std::bind(&FileTransferServer::_onClientMessage, this, _2, _3);
		m_server.m_listener.on_close = std::bind(&FileTransferServer::_onClientClose, this, _2, _3);
		
		// listen port and start server
		if (!m_server.bind(Address(listenPort, false), true))
		{
			CY_LOG(L_ERROR, "Listen port %d failed", listenPort);
			return;
		}

		if (!m_server.start((int32_t)m_threadContext.size()))
		{
			CY_LOG(L_ERROR, "Start server failed");
			return;
		}

		m_server.join();
	}

public:
	FileTransferServer() 
		: m_server("FileTransfer", nullptr)
		, m_workingCounts(0)
	{
	}

	~FileTransferServer() {

	}
};


////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	uint16_t local_port = 3000;
	int32_t work_thread_counts = sys_api::get_cpu_counts();
	std::string filePath;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_PORT) {
				local_port = (uint16_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_FILE_PATH) {
				filePath = args.OptionArg();
			}
			else if (args.OptionId() == OPT_THREADS) {
				work_thread_counts = (int32_t)atoi(args.OptionArg());
			}
		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	if (local_port == 0 || filePath.empty()) {
		printUsage(argv[0]);
		return 0;
	}

	if (work_thread_counts < 1) {
		work_thread_counts = 1;
	}

	CY_LOG(L_DEBUG, "listen port: %d", local_port);

	FileTransferServer server;
	if (!server.prepare(filePath, work_thread_counts)) {
		return 0;
	}

	server.startAndJoin(local_port);
	return 0;
}
