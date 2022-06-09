#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <cy_crypt.h>
#include <utility/cyu_simple_opt.h>
#include <utility/cyu_string_util.h>

#include <fstream>

#include "ft_common.h"

using namespace cyclone;
using namespace std::placeholders;

enum { OPT_HOST, OPT_PORT, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_HOST, "-h",     SO_REQ_SEP }, // "-h HOST_IP"
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

////////////////////////////////////////////////////////////////////////////////////////////
static void printUsage(const char* moduleName)
{
	printf("===== FileTransfer Client(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -h  HOST_IP\t Transfer Server Address\n");
	printf("\t -p LISTEN_PORT\tTransfer Server Port\n");
	printf("\t --help -?\tShow this help\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
class FileTransferClient
{
public:
	std::string m_server_ip;
	uint16_t m_server_port;
	size_t m_fileSize;
	std::string m_strFileName;
	int32_t m_threadTounts;
	bool m_bGotFileInfo;
	int64_t m_beginDownloadTime;
	int64_t m_endDownloadTime;
	PeriodValue<size_t, true> m_downloadSpeed;

	enum ThreadStatus
	{
		TS_Connecting,
		TS_RequireFragment,
		TS_Receiving,
		TS_Complete,
		TS_Success,
		TS_Error,
	};

	struct DownloadThreadContext
	{
		int32_t index;
		TcpClientPtr client;
		ThreadStatus status;
		Looper* looper;
		size_t fileOffset;
		int32_t fragmentSize;
		atomic_int32_t receivedSize;
		uint32_t fragmentCRC;
		thread_t downloadThread;
		std::string strFileName;
		std::ofstream fileHandle;
		int64_t beginTime;
		int64_t endTime;
	};
	std::vector< DownloadThreadContext* > m_downloadContext;

	enum { DOWNLOAD_STATISTICS_TIME = 1 }; //1 seconds

public:
	FileTransferClient() :
		m_downloadSpeed(DOWNLOAD_STATISTICS_TIME*1000)
	{

	}
	bool queryFileInfo(const std::string& server_ip, uint16_t server_port)
	{
		m_server_ip = server_ip;
		m_server_port = server_port;
		m_bGotFileInfo = false;

		Looper* looper = Looper::create_looper();

		TcpClientPtr client = std::make_shared<TcpClient>(looper, nullptr);

		client->m_listener.on_connected = [](TcpClientPtr _client, TcpConnectionPtr _conn, bool _success) -> uint32_t {
			if (!_success) {
				uint32_t retry_time = 1000 * 5;
				CY_LOG(L_INFO, "connect failed!, retry after %d milliseconds...", retry_time);
				return 1000 * 5;
			}

			//send query file info packet
			FT_RequireFileInfo require;
			require.id = FT_RequireFileInfo::ID;
			require.size = sizeof(FT_RequireFileInfo);

			_client->send((const char*)&require, sizeof(FT_RequireFileInfo));
			return 0;
		};

		client->m_listener.on_message = [this](TcpClientPtr, TcpConnectionPtr _conn) {
			RingBuf& ringBuf = _conn->get_input_buf();
			if (ringBuf.size() < sizeof(FT_Head)) return;

			FT_Head head;
			ringBuf.peek(0, &head, sizeof(FT_Head));
			if ((int32_t)ringBuf.size() < head.size) return;

			if (head.id != FT_ReplyFileInfo::ID) {
				CY_LOG(L_ERROR, "not expect message, should be 'FT_ReplyFileInfo'");
				_conn->shutdown();
				return;
			}

			FT_ReplyFileInfo* fileInfo = (FT_ReplyFileInfo*)(ringBuf.normalize());
			this->m_fileSize = fileInfo->fileSize;
			this->m_strFileName = ((const char*)fileInfo) + sizeof(FT_ReplyFileInfo);
			this->m_threadTounts = fileInfo->threadCounts;
			this->m_bGotFileInfo = true;

			CY_LOG(L_INFO, "Got file info, size=%s, name:%s", string_util::size_to_string((float)this->m_fileSize).c_str(), this->m_strFileName.c_str());

			ringBuf.discard((size_t)(fileInfo->size));
			_conn->shutdown();
		};

		client->m_listener.on_close = [looper](TcpClientPtr, TcpConnectionPtr) {
			looper->push_stop_request();
			return;
		};

		CY_LOG(L_DEBUG, "connect to %s:%d...", m_server_ip.c_str(), m_server_port);
		client->connect(Address(m_server_ip.c_str(), m_server_port));

		looper->loop();

		client = nullptr;
		Looper::destroy_looper(looper);

		return m_bGotFileInfo;
	}

	uint32_t _onConnected(TcpClientPtr client, bool success)
	{
		DownloadThreadContext* ctx = (DownloadThreadContext*)(client->get_param());
		assert(ctx->status == TS_Connecting);

		if (!success) {
			uint32_t retry_time = 1000 * 5;
			CY_LOG(L_INFO, "connect failed!, retry after %d milliseconds...", retry_time);
			return 1000 * 5;
		}

		char szTemp[MAX_PATH] = { 0 };
		std::snprintf(szTemp, MAX_PATH, "%s.part%d", m_strFileName.c_str(), ctx->index);
		ctx->strFileName = szTemp;
		ctx->status = TS_RequireFragment;
		ctx->fileHandle.open(ctx->strFileName, std::ios::out | std::ios::binary);
		if (ctx->fileHandle.fail()) {
			CY_LOG(L_ERROR, "Write file %s failed!", ctx->strFileName.c_str());
			return 0;
		}

		FT_RequireFileFragment require;
		require.id = FT_RequireFileFragment::ID;
		require.size = sizeof(FT_RequireFileFragment);
		require.fileOffset = ctx->fileOffset;
		require.fragmentSize = ctx->fragmentSize;

		client->send((const char*)&require, sizeof(FT_RequireFileFragment));
		return 0;
	}

	bool _onReceiveFragmentBegin(DownloadThreadContext* ctx, TcpConnectionPtr conn)
	{
		assert(ctx->status == TS_RequireFragment);

		RingBuf& ringBuff = conn->get_input_buf();
		if (ringBuff.size() < sizeof(FT_ReplyFileFragment_Begin)) {
			return true;
		}

		FT_ReplyFileFragment_Begin fileBegin;
		ringBuff.memcpy_out(&fileBegin, sizeof(FT_ReplyFileFragment_Begin));

		if (fileBegin.id != FT_ReplyFileFragment_Begin::ID || fileBegin.fileOffset != ctx->fileOffset || fileBegin.fragmentSize != ctx->fragmentSize) {
			CY_LOG(L_ERROR, "Not expect message or file size error, should be 'FT_ReplyFileFragment_Begin'!");
			ctx->status = TS_Error;

			conn->shutdown();
			return false;
		}

		ctx->status = TS_Receiving;
		ctx->beginTime = sys_api::performance_time_now();
		return false;
	}

	void _onReceiveFileData(DownloadThreadContext* ctx, TcpConnectionPtr conn)
	{
		assert(ctx->status == TS_Receiving);
		RingBuf& ringBuff = conn->get_input_buf();

		int32_t writeSize = (int32_t)(ctx->fragmentSize - ctx->receivedSize.load());
		if (writeSize > (int32_t)ringBuff.size()) {
			writeSize = (int32_t)ringBuff.size();
		}

		ctx->fragmentCRC = cyclone::adler32(ctx->fragmentCRC, ringBuff.normalize(), (size_t)writeSize);
		ctx->fileHandle.write((const char*)ringBuff.normalize(), writeSize);

		ringBuff.discard((size_t)writeSize);

		ctx->receivedSize += writeSize;
		m_downloadSpeed.push((size_t)writeSize);

		if (ctx->receivedSize.load() >= ctx->fragmentSize) {
			ctx->status = TS_Complete;

			ctx->fileHandle.close();
		}
	}

	bool _onReceiveFragmentEnd(DownloadThreadContext* ctx, TcpConnectionPtr conn)
	{
		assert(ctx->status == TS_Complete);

		RingBuf& ringBuff = conn->get_input_buf();
		if (ringBuff.size() < sizeof(FT_ReplyFileFragment_End)) {
			return true;
		}

		FT_ReplyFileFragment_End fileEnd;
		ringBuff.memcpy_out(&fileEnd, sizeof(FT_ReplyFileFragment_End));
		ctx->endTime = sys_api::performance_time_now();

		if (fileEnd.id != FT_ReplyFileFragment_End::ID) {
			CY_LOG(L_ERROR, "Error! Not expect message, should be 'FT_ReplyFileFragment_End'!");
			ctx->status = TS_Error;
		}
		else {
			if (fileEnd.fragmentCRC != ctx->fragmentCRC) {
				CY_LOG(L_ERROR, "Error! fragment %d crc failed, receive data 0X%08x, should be 0x%08x", ctx->index, ctx->fragmentCRC, fileEnd.fragmentCRC);
				ctx->status = TS_Error;
			}
			else {
				size_t speed = (size_t)(ctx->fragmentSize*1000*1000) / (size_t)(ctx->endTime - ctx->beginTime);
				CY_LOG(L_INFO, "Download thread[%d] success, offset=%zd, fragment_size=%s, crc=0x%08x, speed=%s/s, readBufMaxSize=%s", 
					ctx->index, ctx->fileOffset, string_util::size_to_string((float)ctx->fragmentSize).c_str(), ctx->fragmentCRC, string_util::size_to_string(speed).c_str(),
					string_util::size_to_string(conn->get_readebuf_max_size()).c_str()
					);
				ctx->status = TS_Success;
			}
		}
		conn->shutdown();
		return false;
	}

	void _onMessage(TcpClientPtr client, TcpConnectionPtr conn)
	{
		DownloadThreadContext* ctx = (DownloadThreadContext*)(client->get_param());
		RingBuf& ringBuff = conn->get_input_buf();

		do {
			switch (ctx->status)
			{
			case ThreadStatus::TS_RequireFragment:
				if (_onReceiveFragmentBegin(ctx, conn)) return;
				break;

			case ThreadStatus::TS_Receiving:
				_onReceiveFileData(ctx, conn);
				break;

			case ThreadStatus::TS_Complete:
				if (_onReceiveFragmentEnd(ctx, conn)) return;
				break;

			default:
				assert(false && "Error status");
				break;
			}
		} while (!ringBuff.empty());
	}

	void _onClose(TcpClientPtr client)
	{
		DownloadThreadContext* ctx = (DownloadThreadContext*)(client->get_param());

		ctx->fileHandle.close();
		ctx->looper->push_stop_request();
	}

	void _downloadThread(void* param)
	{
		DownloadThreadContext* ctx = (DownloadThreadContext*)param;
		ctx->looper = Looper::create_looper();
		ctx->client = std::make_shared<TcpClient>(ctx->looper, ctx);
		ctx->status = TS_Connecting;
		ctx->client->m_listener.on_connected = std::bind(&FileTransferClient::_onConnected, this, _1, _3);
		ctx->client->m_listener.on_message = std::bind(&FileTransferClient::_onMessage, this, _1, _2);
		ctx->client->m_listener.on_close = std::bind(&FileTransferClient::_onClose, this, _1);
		
		CY_LOG(L_INFO, "Begin download thread[%d], offset=%zd, fragment_size=%d", ctx->index, ctx->fileOffset, ctx->fragmentSize);

		//begin connect to server and download in work thread
		ctx->client->connect(Address(m_server_ip.c_str(), m_server_port));
		ctx->looper->loop();

		ctx->client = nullptr;
		Looper::destroy_looper(ctx->looper);
	}

	bool beginDownloadFile(void)
	{
		if (!m_bGotFileInfo) {
			CY_LOG(L_ERROR, "Query file info first!");
			return false;
		}

		const size_t MIN_FRAMEMENT_SIZE = 64 * 1024; // 64kb
		int32_t fragmentCounts = 0;
		int32_t fragmentSize = 0;

		if (m_fileSize < MIN_FRAMEMENT_SIZE*(size_t)m_threadTounts) {
			//small file
			fragmentCounts = (int32_t)(m_fileSize / MIN_FRAMEMENT_SIZE);
		}
		else {
			fragmentCounts = m_threadTounts;
		}
		fragmentSize = (int32_t)((int32_t)m_fileSize / fragmentCounts) & (~0xF);

		m_beginDownloadTime = sys_api::performance_time_now();

		size_t fileOffset = 0;
		for (int32_t i = 0; i < fragmentCounts; i++) {

			char threadName[32] = { 0 };
			std::snprintf(threadName, 32, "download%d", i);

			DownloadThreadContext* ctx = new DownloadThreadContext;
			ctx->index = i;
			ctx->fileOffset = fileOffset;
			ctx->fragmentSize = (i==fragmentCounts-1) ? (int32_t)(m_fileSize - ctx->fileOffset) : fragmentSize;
			ctx->receivedSize = 0;
			ctx->fragmentCRC = INITIAL_ADLER;
			ctx->downloadThread = sys_api::thread_create(std::bind(&FileTransferClient::_downloadThread, this, _1), ctx, threadName);

			m_downloadContext.push_back(ctx);
			fileOffset += (size_t)(ctx->fragmentSize);
		}

		int32_t completedFragmentCounts = 0;
		while (completedFragmentCounts < fragmentCounts) {

			size_t totalDownloadSize = 0;
			for (int32_t i = 0; i < fragmentCounts; i++) {
				DownloadThreadContext* ctx = m_downloadContext[(size_t)i];
				totalDownloadSize += (size_t)(ctx->receivedSize.load());

				if (ctx->downloadThread && sys_api::thread_join(ctx->downloadThread, 0)) {
					completedFragmentCounts++;
					ctx->downloadThread = nullptr;
				}
			}
			auto downloadSpeed = m_downloadSpeed.sum_and_counts();

			CY_LOG(L_INFO, "Download speed: %s/s, Size:%s Percent:[%.2f%%]", 
				string_util::size_to_string(downloadSpeed.first / ((size_t)m_downloadSpeed.get_time_period()/ (size_t)1000)).c_str(),
				string_util::size_to_string(totalDownloadSize).c_str(),
				(float)(totalDownloadSize*100 / m_fileSize));

			sys_api::thread_sleep(1000);
		}

		//check download thread status
		for (int32_t i = 0; i < fragmentCounts; i++) {
			DownloadThreadContext* ctx = m_downloadContext[(size_t)i];
			if (ctx->status != TS_Success) {
				CY_LOG(L_INFO, "Download thread %d status error", i);
				return false;
			}
		}

		m_endDownloadTime = sys_api::performance_time_now();

		return true;
	}

	bool combineFile(void)
	{
		std::ofstream outputFileHandle;
		outputFileHandle.open(m_strFileName, std::ios::out | std::ios::binary);
		if (outputFileHandle.fail()) {
			CY_LOG(L_ERROR, "Write file %s failed!", m_strFileName.c_str());
			return 0;
		}

		const int32_t kBufSize = 1024 * 1024;
		char* pBuffer = (char*)CY_MALLOC(kBufSize);

		for (int32_t i = 0; i < (int32_t)m_downloadContext.size(); i++) {
			DownloadThreadContext* ctx = m_downloadContext[(size_t)i];

			std::ifstream inputFileHandle;
			inputFileHandle.open(ctx->strFileName, std::ios::in | std::ios::binary);
			if (inputFileHandle.fail()) {
				CY_LOG(L_ERROR, "Write file %s failed!", m_strFileName.c_str());
				return 0;
			}

			while (!inputFileHandle.eof()) {
				size_t readSize = (size_t)inputFileHandle.read(pBuffer, (std::streamsize)kBufSize).gcount();
				outputFileHandle.write(pBuffer, (std::streamsize)readSize);
			}
			inputFileHandle.close();
			//delete file
			std::remove(ctx->strFileName.c_str());
		}
		outputFileHandle.close();
		CY_FREE(pBuffer);
		return true;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	std::string server_ip = "127.0.0.1";
	uint16_t server_port = 3000;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_HOST) {
				server_ip = args.OptionArg();
			}
			else if (args.OptionId() == OPT_PORT) {
				server_port = (uint16_t)atoi(args.OptionArg());
			}
		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	// query file info
	FileTransferClient client;
	if (!client.queryFileInfo(server_ip, server_port)) {
		CY_LOG(L_ERROR, "Can't get file info from server side");
		return 1;
	}

	//receive file
	if (!client.beginDownloadFile()) {
		CY_LOG(L_ERROR, "Download file failed");
		return 1;
	}

	size_t downloadTime = (size_t)(client.m_endDownloadTime - client.m_beginDownloadTime)/(1000ll*1000ll);
	CY_LOG(L_INFO, "Download complete, time=%d(sec), speed=%s/s", downloadTime, string_util::size_to_string(client.m_fileSize/downloadTime).c_str());

	//combine file
	if (!client.combineFile()) {
		CY_LOG(L_ERROR, "Combine file failed");
		return 1;
	}
	CY_LOG(L_INFO, "Combine file complete");

	return 0;
}

