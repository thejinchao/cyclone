#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <cy_network.h>
#include <utility/cyu_simple_opt.h>
#include <utility/cyu_statistics.h>
#include <utility/cyu_string_util.h>

#include "relay_protocol.h"

using namespace cyclone;
using namespace std::placeholders;

////////////////////////////////////////////////////////////////////////////////////////////
enum { OPT_PORT, OPT_UP_HOST, OPT_UP_PORT, OPT_VERBOSE_MODE, OPT_ENCRYPT_MODE, OPT_THREADS, OPT_STATISTICS, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP },  // "-p LISTEN_PORT"
	{ OPT_UP_HOST, "-uh",  SO_REQ_SEP }, // "-uh UP_SERVER_HOST"
	{ OPT_UP_PORT, "-up",  SO_REQ_SEP }, // "-up UP_SERVER_PORT"
	{ OPT_ENCRYPT_MODE, "-e",  SO_NONE }, // "-e"
	{ OPT_THREADS, "-t",  SO_REQ_SEP }, // "-t THREAD_COUNTS"
	{ OPT_STATISTICS, "-s",  SO_NONE },	// "-s"
	{ OPT_VERBOSE_MODE, "-v",  SO_NONE },	// "-v"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	 // "--help"
	SO_END_OF_OPTIONS                   // END
};

////////////////////////////////////////////////////////////////////////////////////////////
class RelaySession
{
public:
	int32_t m_id;
	ConnectionPtr m_downConnection;

public:
	RelaySession(int32_t id, ConnectionPtr downConnection)
		: m_id(id)
		, m_downConnection(downConnection)
	{

	}
	~RelaySession()
	{

	}
};

////////////////////////////////////////////////////////////////////////////////////////////
class RelayLocal
{
private:
	enum UpState {
		kConnecting = 0,
		kHandshaking,
		kHandshaked,
		kDisConnected
	};
	typedef std::map<int32_t, RelaySession> RelaySessionMap;

	Address m_upAddress;
	TcpServer* m_downServer;
	bool m_encryptMode;

	struct RelayPipe
	{
		TcpClientPtr m_upClient;
		UpState m_upState;
		dhkey_t m_publicKey;
		dhkey_t m_privateKey;
		dhkey_t m_secretKey;
		Rijndael* m_encrypt;
		Rijndael* m_decrypt;
		RelaySessionMap m_sessionMap;

		RelayPipe(bool encryptMode) : m_upClient(nullptr), m_upState(kConnecting), m_encrypt(nullptr), m_decrypt(nullptr)
		{
			if (encryptMode)
				DH_generate_key_pair(m_publicKey, m_privateKey);
			else
				m_publicKey.dq.low = m_publicKey.dq.high = 0;
		}

		~RelayPipe()
		{
			if (m_encrypt) {
				delete m_encrypt; m_encrypt = nullptr;
			}

			if (m_decrypt) {
				delete m_decrypt; m_decrypt = nullptr;
			}
		}
	};
	typedef std::vector<RelayPipe*> RelayPipeVector;
	RelayPipeVector m_relayPipes;

	enum { kSpeedTimePeriod = 5 * 1000 }; //(2 seconds)

	bool m_enable_statistics;

	std::atomic<int64_t> m_up_total;
	PeriodValue<int32_t, true> m_up_statistics;

	std::atomic<int64_t> m_down_total;
	PeriodValue<int32_t, true> m_down_statistics;

public:
	//-------------------------------------------------------------------------------------
	void startAndJoin(uint16_t local_port, const Address& upAddress, int32_t work_thread_counts)
	{
		m_upAddress = upAddress;

		TcpServer server("relay_local", nullptr);
		m_downServer = &server;

		server.m_listener.on_master_thread_start = std::bind(&RelayLocal::onMasterThreadStart, this, _1, _2);
		server.m_listener.on_work_thread_start = std::bind(&RelayLocal::onWorkthreadStart, this, _1, _2, _3);
		server.m_listener.on_connected = std::bind(&RelayLocal::onLocalConnected, this, _1, _2, _3);
		server.m_listener.on_message = std::bind(&RelayLocal::onLocalMessage, this, _1, _2, _3);
		server.m_listener.on_close = std::bind(&RelayLocal::onLocalClose, this, _2, _3);
		server.bind(Address(local_port, false), true);

		m_relayPipes.resize(work_thread_counts);

		if (!server.start(work_thread_counts)) return;
		server.join();
	}
private:
	//-------------------------------------------------------------------------------------
	void onMasterThreadStart(TcpServer* /*server*/, Looper* looper)
	{
		if (m_enable_statistics) {
			looper->register_timer_event(kSpeedTimePeriod, nullptr, std::bind(&RelayLocal::printStatisticsInfo, this));
		}
	}

	//-------------------------------------------------------------------------------------
	void onWorkthreadStart(TcpServer* /*server*/, int32_t index, Looper* looper)
	{
		RelayPipe* newPipe = new  RelayPipe(m_encryptMode);
		m_relayPipes[index] = newPipe;

		newPipe->m_upClient = std::make_shared<TcpClient>(looper, this, index);
		newPipe->m_upClient->m_listener.on_connected = std::bind(&RelayLocal::onUpConnected, this, _2, _3);
		newPipe->m_upClient->m_listener.on_message = std::bind(&RelayLocal::onUpMessage, this, _1, _2);
		newPipe->m_upClient->m_listener.on_close = std::bind(&RelayLocal::onUpClose, this, _1, _2);
		newPipe->m_upClient->connect(m_upAddress);
	}
	//-------------------------------------------------------------------------------------
	void onLocalConnected(TcpServer* server, int32_t index, ConnectionPtr conn)
	{
		RelayPipe* pipe = m_relayPipes[index];

		if (pipe->m_upState!= kHandshaked) {
			server->shutdown_connection(conn);
			return;
		}
		RelaySession newSession(conn->get_id(), conn);
		pipe->m_sessionMap.insert({ conn->get_id(), newSession });

		RelayNewSessionMsg newSessionMsg;
		newSessionMsg.id = conn->get_id();

		Packet packet;
		packet.build_from_memory((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayNewSessionMsg::ID, sizeof(newSessionMsg), (const char*)&newSessionMsg);
		pipe->m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());

		CY_LOG(L_TRACE, "[%d]CLIENT connected, send new session msg to UP", conn->get_id());
	}
	//-------------------------------------------------------------------------------------
	void onLocalMessage(TcpServer* server, int32_t index, ConnectionPtr conn)
	{
		RelayPipe* pipe = m_relayPipes[index];

		if (pipe->m_upState != kHandshaked) {
			server->shutdown_connection(conn);
			return;
		}
		RingBuf& ringBuf = conn->get_input_buf();

		while (!ringBuf.empty()) {
			size_t msgSize = (ringBuf.size() < 0xFF00u) ? ringBuf.size() : 0xFF00u;

			RelayForwardMsg forwardMsg;
			forwardMsg.id = conn->get_id();
			forwardMsg.size = (int32_t)msgSize;

			size_t buf_round_size = m_encryptMode ? _round16(msgSize) : msgSize;
			Packet packet;
			packet.build_from_memory((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayForwardMsg::ID, (uint16_t)(sizeof(RelayForwardMsg) + buf_round_size), nullptr);

			memcpy(packet.get_packet_content(), &forwardMsg, sizeof(forwardMsg));
			ringBuf.memcpy_out(packet.get_packet_content() + sizeof(forwardMsg), msgSize);

			//encrypt and send
			if (m_encryptMode)
			{
				uint8_t* buf = (uint8_t*)packet.get_packet_content() + sizeof(forwardMsg);
				pipe->m_encrypt->encrypt(buf, buf, buf_round_size);
			}

			//Statistics
			if (m_enable_statistics) {
				m_up_total += (int64_t)packet.get_memory_size();
				m_up_statistics.push((int32_t)packet.get_memory_size());
			}

			pipe->m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());
			CY_LOG(L_TRACE, "[%d]receive message from CLIENT(%zd/%zd), send to UP after encrypt", conn->get_id(), (size_t)msgSize, buf_round_size);
		}
	}

	//-------------------------------------------------------------------------------------
	void onLocalClose(int32_t index, ConnectionPtr conn)
	{
		RelayPipe* pipe = m_relayPipes[index];

		auto it = pipe->m_sessionMap.find(conn->get_id());
		if (it == pipe->m_sessionMap.end()) return;

		pipe->m_sessionMap.erase(it);
        if(pipe->m_upState==kHandshaked)
        {
            RelayCloseSessionMsg closeSessionMsg;
            closeSessionMsg.id = conn->get_id();

            Packet packet;
            packet.build_from_memory((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayCloseSessionMsg::ID, sizeof(closeSessionMsg), (const char*)&closeSessionMsg);
			pipe->m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());

            CY_LOG(L_TRACE, "[%d]down client closed!, send close session message to up server", conn->get_id());
        }
	}

	//-------------------------------------------------------------------------------------
	void printStatisticsInfo(void)
	{
		auto up_statistics = m_up_statistics.sum_and_counts();
		auto down_statistics = m_down_statistics.sum_and_counts();

		CY_LOG(L_INFO, "===UP:%s Speed:%s/s DOWN:%s Speed:%s/s",
			string_util::size_to_string((size_t)m_up_total.load()).c_str(),
			string_util::size_to_string((float)(up_statistics.first * 1000 / kSpeedTimePeriod)).c_str(),
			string_util::size_to_string((size_t)m_down_total.load()).c_str(),
			string_util::size_to_string((float)(down_statistics.first * 1000 / kSpeedTimePeriod)).c_str()
		);
	}

private:
	//-------------------------------------------------------------------------------------
	uint32_t onUpConnected(ConnectionPtr conn, bool success)
	{
		if (success) {
			RelayPipe* pipe = m_relayPipes[conn->get_id()];
			assert(pipe->m_upState == kConnecting);

			//send handshake message
			RelayHandshakeMsg handshake;
			handshake.dh_key = pipe->m_publicKey;

			Packet packet;
			packet.build_from_memory((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayHandshakeMsg::ID, sizeof(handshake), (const char*)&handshake);
			pipe->m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());

			//update state
			pipe->m_upState = kHandshaking;
			CY_LOG(L_DEBUG, "Up Server Connected, send handshake message.");
			return 0;
		}else {
			return 10 * 1000;
		}
	}

	//-------------------------------------------------------------------------------------
	void onUpMessage(TcpClientPtr client, ConnectionPtr conn)
	{
		RelayPipe* pipe = m_relayPipes[conn->get_id()];

		for (;;) {
			if (pipe->m_upState == kHandshaking) {
				//peed message id
				uint16_t packetID;
				if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) return;
				packetID = socket_api::ntoh_16(packetID);

				//must be handshake message
				if (packetID != (uint16_t)RELAY_HANDSHAKE_ID) {
					client->disconnect();
					pipe->m_upState = kDisConnected;
					return;
				}

				//get handshake message
				Packet handshakePacket;
				if (!handshakePacket.build_from_ringbuf(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

				//check size
				if (handshakePacket.get_packet_size() != sizeof(RelayHandshakeMsg)) {
					client->disconnect();
					pipe->m_upState = kDisConnected;
					return;
				}

				RelayHandshakeMsg handshake;
				memcpy(&handshake, handshakePacket.get_packet_content(), sizeof(handshake));

				bool bRemoteEncrypt = (handshake.dh_key.dq.low != 0 && handshake.dh_key.dq.low != 0);
				if (m_encryptMode != bRemoteEncrypt)
				{
					CY_LOG(L_ERROR, "Encrypt Mode not match, relay_server:%s , relay_local: %s", bRemoteEncrypt?"true":"false", m_encryptMode ? "true" : "false");
					client->disconnect();
					pipe->m_upState = kDisConnected;
					return;
				}

				//handshake
				if (m_encryptMode)
				{
					DH_generate_key_secret(pipe->m_secretKey, pipe->m_privateKey, handshake.dh_key);

					//create encrypter
					pipe->m_encrypt = new Rijndael(pipe->m_secretKey.bytes);

					//create decrypter
					for (size_t i = 0; i < Rijndael::BLOCK_SIZE; i++)
						pipe->m_privateKey.bytes[i] = (uint8_t)(~(pipe->m_privateKey.bytes[i]));
					pipe->m_decrypt = new Rijndael(pipe->m_secretKey.bytes);
				}

				//update state
				pipe->m_upState = kHandshaked;

				CY_LOG(L_DEBUG, "Connect to up server(%s:%d)", conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());

				//clean secret key memory(for safe)
				memset(pipe->m_secretKey.bytes, 0, Rijndael::BLOCK_SIZE);
			}
			else if (pipe->m_upState == kHandshaked) {
				//peed message id
				uint16_t packetID;
				if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) return;
				packetID = socket_api::ntoh_16(packetID);

				switch (packetID) {
				case RELAY_FORWARD:
				{
					//get packet
					Packet packet;
					if (!packet.build_from_ringbuf(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

					RelayForwardMsg forwardMsg;
					memcpy(&forwardMsg, packet.get_packet_content(), sizeof(RelayForwardMsg));

					//decrypt
					if (m_encryptMode)
					{
						uint8_t* buf = (uint8_t*)packet.get_packet_content() + sizeof(RelayForwardMsg);
						pipe->m_decrypt->decrypt(buf, buf, packet.get_packet_size() - sizeof(RelayForwardMsg));
					}

					auto it = pipe->m_sessionMap.find(forwardMsg.id);
					if (it == pipe->m_sessionMap.end()) {

						RelayCloseSessionMsg closeSessionMsg;
						closeSessionMsg.id = forwardMsg.id;

						Packet packetCloseSession;
						packetCloseSession.build_from_memory((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayCloseSessionMsg::ID, sizeof(closeSessionMsg), (const char*)&closeSessionMsg);
						pipe->m_upClient->send(packetCloseSession.get_memory_buf(), packetCloseSession.get_memory_size());
						break;
					}

					//statistics
					if (m_enable_statistics) {
						m_down_total += forwardMsg.size;
						m_down_statistics.push((int32_t)forwardMsg.size);
					}

					RelaySession& session = it->second;
					session.m_downConnection->send(packet.get_packet_content() + sizeof(forwardMsg), (size_t)forwardMsg.size);
					CY_LOG(L_TRACE, "[%d]receive message from UP(%zd/%zd), send to DOWN after decrypt", forwardMsg.id,
						(size_t)forwardMsg.size, (size_t)(packet.get_packet_size() - sizeof(RelayForwardMsg)));
				}
				break;

				case RELAY_CLOSE_SESSION:
				{
					//get packet
					Packet packet;
					if (!packet.build_from_ringbuf(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

					RelayCloseSessionMsg closeSessionMsg;
					memcpy(&closeSessionMsg, packet.get_packet_content(), sizeof(closeSessionMsg));

					auto it = pipe->m_sessionMap.find(closeSessionMsg.id);
					if (it == pipe->m_sessionMap.end()) {
						break;
					}
					ConnectionPtr downConn = it->second.m_downConnection;

					pipe->m_sessionMap.erase(it);
					m_downServer->shutdown_connection(downConn);
					CY_LOG(L_TRACE, "[%d]receive session close msg from UP, shutdown CLIENT", closeSessionMsg.id);
				}
				break;

				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	void onUpClose(TcpClientPtr client, ConnectionPtr conn)
	{
		RelayPipe* pipe = m_relayPipes[conn->get_id()];
		
		pipe->m_upClient = nullptr;
		pipe->m_upState = kDisConnected;
	}

private:
	//-------------------------------------------------------------------------------------
	size_t _round16(size_t size) {
		return ((size & 0xF) == 0) ? size : ((size & (size_t)(~0xF))+0x10);
	}

public:
	RelayLocal(bool encryptMode, bool enableStatistics)
		: m_downServer(nullptr)
		, m_encryptMode(encryptMode)
		, m_enable_statistics(enableStatistics)
		, m_up_total(0)
		, m_up_statistics(1024, kSpeedTimePeriod)
		, m_down_total(0)
		, m_down_statistics(1024, kSpeedTimePeriod)
	{
	}
	~RelayLocal()
	{
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
static void printUsage(const char* moduleName)
{
	printf("===== Relay Local(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -p  LISTEN_PORT\t Local Listen Port, Default 2000\n");
	printf("\t -uh UP_HOST\tUp Server(Relay Server) IP, Default 127.0.0.1\n");
	printf("\t -up UP_PORT\tUp Server(Relay Server) Port, Default 3000\n");
	printf("\t -t THREAD_COUNTS\tWork thread counts(must be 1 when relay_pipe used)\n");
	printf("\t -e\t\tEncrypt Message\n");
	printf("\t -s\t\tPrint speed statistics\n");
	printf("\t -v\t\tVerbose Mode\n");
	printf("\t --help -?\tShow this help\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	uint16_t local_port = 2000;
	std::string up_ip  = "127.0.0.1";
	uint16_t up_port = 3000;
	bool verbose_mode = false;
	bool encrypt_mode = false;
	int32_t work_thread_counts = sys_api::get_cpu_counts();
	bool enable_statistics = false;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_PORT) {
				local_port = (uint16_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_UP_HOST) {
				up_ip = args.OptionArg();
			}
			else if (args.OptionId() == OPT_UP_PORT) {
				up_port = (uint16_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_THREADS) {
				work_thread_counts = (int32_t)atoi(args.OptionArg());
			}
			else if (args.OptionId() == OPT_ENCRYPT_MODE) {
				encrypt_mode = true;
			}
			else if (args.OptionId() == OPT_VERBOSE_MODE) {
				verbose_mode = true;
			}
			else if (args.OptionId() == OPT_STATISTICS) {
				enable_statistics = true;
			}

		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	set_log_threshold(verbose_mode ? L_TRACE : L_DEBUG);

	CY_LOG(L_DEBUG, "listen port %d", local_port);
	CY_LOG(L_DEBUG, "up address %s:%d", up_ip.c_str(), up_port);
	CY_LOG(L_DEBUG, "encrypt mode %s", encrypt_mode ? "true" : "false");
	CY_LOG(L_DEBUG, "work thread counts %d", work_thread_counts);
	CY_LOG(L_DEBUG, "speed statistics: %s", enable_statistics ? "true" : "false");

	RelayLocal relayLocal(encrypt_mode, enable_statistics);
	relayLocal.startAndJoin(local_port, Address(up_ip.c_str(), up_port), work_thread_counts);
	return 0;
}
