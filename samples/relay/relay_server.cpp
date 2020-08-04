#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <cy_network.h>
#include <SimpleOpt.h>

#include "relay_protocol.h"

using namespace cyclone;
using namespace std::placeholders;

////////////////////////////////////////////////////////////////////////////////////////////
enum { OPT_PORT, OPT_UP_HOST, OPT_UP_PORT, OPT_VERBOSE_MODE, OPT_ENCRYPT_MODE, OPT_THREADS, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP },  // "-p LISTEN_PORT"
	{ OPT_UP_HOST, "-uh",  SO_REQ_SEP }, // "-uh UP_SERVER_HOST"
	{ OPT_UP_PORT, "-up",  SO_REQ_SEP }, // "-up UP_SERVER_PORT"
	{ OPT_ENCRYPT_MODE, "-e",  SO_NONE }, // "-e"
	{ OPT_THREADS, "-t",  SO_REQ_SEP }, // "-t THREAD_COUNTS"
	{ OPT_VERBOSE_MODE, "-v",  SO_NONE },	// "-v"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	 // "--help"
	SO_END_OF_OPTIONS                   // END
};

////////////////////////////////////////////////////////////////////////////////////////////
class RelaySession
{
public:
	enum UpState
	{
		kConnecting,
		kConnected,
		kDisconnecting,
		kDisconnected
	};

	int32_t m_id;
	TcpClientPtr m_upClient;
	UpState m_upState;
public:
	RelaySession(int32_t id)
		: m_id(id)
		, m_upClient(nullptr)
		, m_upState(kConnecting)
	{

	}
	~RelaySession()
	{
		assert(m_upState==kDisconnected);
		if (m_upClient) {
			assert(m_upClient->get_connection_state() == Connection::kDisconnected);
            m_upClient = nullptr;
		}
	}
};
typedef std::shared_ptr<RelaySession> RelaySessionPtr;

////////////////////////////////////////////////////////////////////////////////////////////
class RelayServer
{
private:
	enum DownClientState
	{
		kWaitConnecting,
		kWaitHandshaking,
		kHandshaked,
		kDisconnected
	};
	TcpServer* m_downServer;
	Address m_upAddress;
	bool m_encryptMode;

	typedef std::map< int32_t, RelaySessionPtr > RelaySessionMap;

	struct RelayPipe
	{
		int32_t m_workthread_index;
		Looper* m_looper;
		ConnectionPtr m_downConnection;
		DownClientState m_downState;
		dhkey_t m_publicKey;
		dhkey_t m_privateKey;
		dhkey_t m_secretKey;
		Rijndael* m_encrypt;
		Rijndael* m_decrypt;
		RelaySessionMap m_relaySessionMap;

		RelayPipe(bool encryptMode) : m_workthread_index(0), m_looper(nullptr), m_downState(kWaitConnecting), m_encrypt(nullptr), m_decrypt(nullptr)
		{
			if (encryptMode)
				DH_generate_key_pair(m_publicKey, m_privateKey);
			else
				m_publicKey.dq.high = m_publicKey.dq.low = 0;
		}
		~RelayPipe() 
		{
			if (m_encrypt)
			{
				delete m_encrypt; m_encrypt = nullptr;
			}

			if (m_decrypt)
			{
				delete m_decrypt; m_decrypt = nullptr;
			}
		}
	};

	typedef std::map<int32_t, RelayPipe*> RelayPipeMap;
	RelayPipeMap m_relayPipes;
	sys_api::mutex_t m_relayPipesLock;

public:
	//-------------------------------------------------------------------------------------
	void startAndJoin(uint16_t local_port, const Address& upAddress, int32_t workThreadCounts)
	{
		m_upAddress = upAddress;

		TcpServer server("rs", nullptr);
		server.m_listener.onWorkThreadStart = std::bind(&RelayServer::onWorkthreadStart, this, _2, _3);
		server.m_listener.onConnected = std::bind(&RelayServer::onDownConnected, this, _1, _2, _3);
		server.m_listener.onMessage = std::bind(&RelayServer::onDownMessage, this, _1, _2, _3);
		server.m_listener.onClose = std::bind(&RelayServer::onDownClose, this, _1, _2, _3);
		server.bind(Address(local_port, false), true);

		m_downServer = &server;

		m_relayPipesLock = sys_api::mutex_create();

		if (!server.start(workThreadCounts)) return;
		server.join();
	}
private:
	//-------------------------------------------------------------------------------------
	void onWorkthreadStart(int32_t /*index*/, Looper* /*looper*/)
	{
	}

	//-------------------------------------------------------------------------------------
	void onDownConnected(TcpServer* /*server*/, int32_t index, ConnectionPtr conn)
	{
		RelayPipe* pipe = new RelayPipe(m_encryptMode);

		pipe->m_workthread_index = index;
		pipe->m_downState = kWaitHandshaking;
		pipe->m_downConnection = conn;
		pipe->m_looper = conn->get_looper();

		conn->set_param(pipe);
	}
	//-------------------------------------------------------------------------------------
	void onDownMessage(TcpServer* server, int32_t /*index*/, ConnectionPtr conn)
	{
		RelayPipe* pipe = (RelayPipe*)(conn->get_param());

		for(;;) {
		if (pipe->m_downState == kWaitHandshaking) {
			//peed message id
			uint16_t packetID;
			if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) return;
			packetID = socket_api::ntoh_16(packetID);

			//must be handshake message
			if (packetID != (uint16_t)RELAY_HANDSHAKE_ID) {
				server->shutdown_connection(conn);
				pipe->m_downState = kWaitConnecting;
				return;
			}

			//get handshake message
			Packet handshakePacket;
			if (!handshakePacket.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

			//check size
			if (handshakePacket.get_packet_size() != sizeof(RelayHandshakeMsg)) {
				server->shutdown_connection(conn);
				pipe->m_downState = kWaitConnecting;
				return;
			}

			RelayHandshakeMsg handshake;
			memcpy(&handshake, handshakePacket.get_packet_content(), sizeof(handshake));

			bool bRemoteEncrypt = (handshake.dh_key.dq.high != 0 && handshake.dh_key.dq.low != 0);
			if (m_encryptMode != bRemoteEncrypt)
			{
				CY_LOG(L_ERROR, "Encrypt Mode not match, relay_server:%s , relay_local: %s", m_encryptMode ? "true" : "false", bRemoteEncrypt ? "true" : "false");
				server->shutdown_connection(conn);
				pipe->m_downState = kWaitConnecting;
				return;
			}

			if (bRemoteEncrypt)
			{
				//handshake
				DH_generate_key_secret(pipe->m_secretKey, pipe->m_privateKey, handshake.dh_key);

				//create encrypt
				pipe->m_decrypt = new Rijndael(pipe->m_secretKey.bytes);

				//create decrypt
				for (size_t i = 0; i < Rijndael::BLOCK_SIZE; i++)
					pipe->m_privateKey.bytes[i] = (uint8_t)(~(pipe->m_privateKey.bytes[i]));
				pipe->m_encrypt = new Rijndael(pipe->m_secretKey.bytes);
			}

			//reply my public key
			handshake.dh_key = pipe->m_publicKey;
			handshakePacket.build((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayHandshakeMsg::ID, sizeof(handshake), (const char*)&handshake);
			conn->send(handshakePacket.get_memory_buf(), handshakePacket.get_memory_size());

			//update state
			pipe->m_downState = kHandshaked;
			//clean secret key memory(for safe)
			memset(pipe->m_secretKey.bytes, 0, Rijndael::BLOCK_SIZE);

			CY_LOG(L_DEBUG, "Down client handshaked(%s:%d)", conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());
		}
		else if (pipe->m_downState == kHandshaked) {
			//peed message id
			uint16_t packetID;
			if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) return;
			packetID = socket_api::ntoh_16(packetID);

			switch (packetID)
			{
			case RELAY_NEW_SESSION:
			{
				//get packet 
				Packet packet;
				if (!packet.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

				RelayNewSessionMsg newSessionMsg;
				memcpy(&newSessionMsg, packet.get_packet_content(), sizeof(newSessionMsg));

				//create new relay session
				RelaySessionPtr session = std::make_shared<RelaySession>(newSessionMsg.id);
				pipe->m_relaySessionMap.insert({ newSessionMsg.id, session });

				//connect to up server
				CY_LOG(L_TRACE, "Connect to up server %s:%d", m_upAddress.get_ip(), m_upAddress.get_port());
				session->m_upClient = std::make_shared<TcpClient>(pipe->m_looper, pipe, newSessionMsg.id);
				session->m_upClient->m_listener.onConnected = std::bind(&RelayServer::onServerConnected, this, _1, _2, _3);
				session->m_upClient->m_listener.onMessage = std::bind(&RelayServer::onServerMessage, this, _1, _2);
				session->m_upClient->m_listener.onClose = std::bind(&RelayServer::onServerClose, this, _1, _2);

				if (!(session->m_upClient->connect(m_upAddress))) {
					_kickDownSession(pipe, newSessionMsg.id, session);
				}
				CY_LOG(L_INFO, "[%d]Receive new session message, connect to %s:%d", newSessionMsg.id, m_upAddress.get_ip(), m_upAddress.get_port());
			}
			break;

			case RELAY_CLOSE_SESSION:
			{
				//get packet 
				Packet packet;
				if (!packet.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

				RelayCloseSessionMsg closeSessionMsg;
				memcpy(&closeSessionMsg, packet.get_packet_content(), sizeof(closeSessionMsg));

				auto it = pipe->m_relaySessionMap.find(closeSessionMsg.id);
				if (it == pipe->m_relaySessionMap.end()) break;

				RelaySessionPtr session = it->second;
				session->m_upState = RelaySession::kDisconnecting;
				session->m_upClient->disconnect();
				session->m_upState = RelaySession::kDisconnected;
				pipe->m_relaySessionMap.erase(it);

				CY_LOG(L_INFO, "[%d]Receive close session message, close it", closeSessionMsg.id);
			}
			break;

			case RELAY_FORWARD:
			{
				//get packet 
				Packet packet;
				if (!packet.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

				RelayForwardMsg forwardMsg;
				memcpy(&forwardMsg, packet.get_packet_content(), sizeof(forwardMsg));

				int32_t id = forwardMsg.id;
				auto it = pipe->m_relaySessionMap.find(id);
				if (it == pipe->m_relaySessionMap.end()) {
					//send close session msg to down client
					_kickDownSession(pipe, id, nullptr);
					break;
				}

				RelaySessionPtr session = it->second;
				if (session->m_upState == RelaySession::kDisconnected) {
					//send close session msg to down client
					_kickDownSession(pipe, id, session);
					break;
				}

				//decrypt and send
				uint8_t* buf = (uint8_t*)packet.get_packet_content() + sizeof(RelayForwardMsg);
				if (m_encryptMode)
				{
					pipe->m_decrypt->decrypt(buf, buf, packet.get_packet_size() - sizeof(RelayForwardMsg));
				}
				session->m_upClient->send((const char*)buf, (size_t)forwardMsg.size);

				CY_LOG(L_TRACE, "[%d]receive from DOWN client(%zd/%zd), send to UP!", id,
						(size_t)forwardMsg.size, (size_t)(packet.get_packet_size() - sizeof(RelayForwardMsg)));
			}
			break;

			default:
				CY_LOG(L_ERROR, "receive invalid packet(%d)", packetID);
				server->shutdown_connection(conn);
				break;
			}
		}
		}
	}
	//-------------------------------------------------------------------------------------
	void onDownClose(TcpServer* /*server*/, int32_t /*index*/, ConnectionPtr conn)
	{
		RelayPipe* pipe = (RelayPipe*)(conn->get_param());

		pipe->m_downState = kDisconnected;

		//close all tunnel
		for (auto it = pipe->m_relaySessionMap.begin(); it != pipe->m_relaySessionMap.end(); it++) {
			RelaySessionPtr session = it->second;

			session->m_upClient->disconnect();
			session->m_upState = RelaySession::kDisconnected;
		}
		pipe->m_relaySessionMap.clear();

		pipe->m_downConnection.reset();

		if (m_encryptMode)
		{
			DH_generate_key_pair(pipe->m_publicKey, pipe->m_privateKey);
			delete pipe->m_encrypt; pipe->m_encrypt = nullptr;
			delete pipe->m_decrypt; pipe->m_decrypt = nullptr;
		}

		//reset down state 
		pipe->m_downState = kWaitConnecting;
	}

private:
	//-------------------------------------------------------------------------------------
	uint32_t onServerConnected(TcpClientPtr client, ConnectionPtr conn, bool success)
	{
		RelayPipe* pipe = (RelayPipe*)(client->get_param());
		if (pipe->m_downState != kHandshaked) return 0;

		if (success) {
			int32_t id = conn->get_id();
			auto it = pipe->m_relaySessionMap.find(id);
			assert(it != pipe->m_relaySessionMap.end());
			if (it == pipe->m_relaySessionMap.end()) {
				return 0;
			}
			RelaySessionPtr session = it->second;
			session->m_upState = RelaySession::kConnected;

			CY_LOG(L_TRACE, "[%d]Connect to UP success(%s:%d)", id, conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());
		}else {
			//send close session msg to down
			int32_t id = conn->get_id();
			auto it = pipe->m_relaySessionMap.find(id);
			if (it == pipe->m_relaySessionMap.end()) {
				return 0;
			}
			RelaySessionPtr session = it->second;
			assert(session->m_upState == RelaySession::kConnecting);

			RelayCloseSessionMsg msg;
			msg.id = session->m_id;

			Packet packet;
			packet.build(RELAY_PACKET_HEADSIZE, RelayCloseSessionMsg::ID, (uint16_t)(sizeof(RelayCloseSessionMsg)), (const char*)&msg);

			//send to down client
			pipe->m_downConnection->send((const char*)packet.get_memory_buf(), packet.get_memory_size());
			session->m_upState = RelaySession::kDisconnected;

			//send to work thread to delete session and client
			m_downServer->send_work_message(pipe->m_workthread_index, &packet);
			CY_LOG(L_TRACE, "[%d]Connect to UP failed!", id);
		}
		return 0;
	}

	//-------------------------------------------------------------------------------------
	void onServerMessage(TcpClientPtr client, ConnectionPtr conn)
	{
		RelayPipe* pipe = (RelayPipe*)(client->get_param());
		if (pipe->m_downState != kHandshaked) return;

		int32_t id = conn->get_id();
		auto it = pipe->m_relaySessionMap.find(id);
		assert(it != pipe->m_relaySessionMap.end());
		if (it == pipe->m_relaySessionMap.end()) return;

		RelaySessionPtr session = it->second;
		RingBuf& ringBuf = conn->get_input_buf();

		while (!ringBuf.empty()) {
			size_t msgSize = (ringBuf.size() < 0xFF00u) ? ringBuf.size() : 0xFF00u;

			RelayForwardMsg msg;
			msg.id = session->m_id;
			msg.size = (int32_t)msgSize;

			size_t buf_round_size = m_encryptMode ? _round16(msgSize) : msgSize;
			CY_LOG(L_TRACE, "[%d]receive from UP(%zd/%zd), send to DOWN after encrypt", msg.id, (size_t)msg.size, buf_round_size);

			Packet packet;
			packet.build(RELAY_PACKET_HEADSIZE, RelayForwardMsg::ID, (uint16_t)(sizeof(RelayForwardMsg) + buf_round_size), nullptr);
			memcpy(packet.get_packet_content(), &msg, sizeof(RelayForwardMsg));
			ringBuf.memcpy_out(packet.get_packet_content() + sizeof(RelayForwardMsg), msgSize);

			//encrypt and send to down client
			uint8_t* buf = (uint8_t*)packet.get_packet_content() + sizeof(RelayForwardMsg);
			if (m_encryptMode)
			{
				pipe->m_encrypt->encrypt(buf, buf, buf_round_size);
			}
			pipe->m_downConnection->send((const char*)packet.get_memory_buf(), packet.get_memory_size());
		}
	}

	//-------------------------------------------------------------------------------------
	void onServerClose(TcpClientPtr client, ConnectionPtr conn)
	{
		RelayPipe* pipe = (RelayPipe*)(client->get_param());
		int32_t id = conn->get_id();

		auto it = pipe->m_relaySessionMap.find(id);
		assert(it != pipe->m_relaySessionMap.end());
		if (it == pipe->m_relaySessionMap.end()) return;
		
		RelaySessionPtr session = it->second;
		assert(session->m_upState == RelaySession::kConnected || session->m_upState == RelaySession::kDisconnecting);

		if (session->m_upState == RelaySession::kConnected) {
			_kickDownSession(pipe, session->m_id, session);
		}

		CY_LOG(L_TRACE, "[%d]UP shutdown the client, send kick msg to DOWN", id);
	}

private:
	//-------------------------------------------------------------------------------------
	size_t _round16(size_t size) {
		return ((size & 0xF) == 0) ? size : ((size & (size_t)(~0xF)) + 0x10);
	}
	
	//-------------------------------------------------------------------------------------
	void _kickDownSession(RelayPipe* pipe, int32_t session_id, RelaySessionPtr session) {
		if (pipe->m_downState != kHandshaked) return;

		RelayCloseSessionMsg msg;
		msg.id = session_id;

		Packet packet;
		packet.build(RELAY_PACKET_HEADSIZE, RelayCloseSessionMsg::ID, (uint16_t)(sizeof(RelayCloseSessionMsg)), (const char*)&msg);

		//send to down client
		pipe->m_downConnection->send((const char*)packet.get_memory_buf(), packet.get_memory_size());
		if(session)
			session->m_upState = RelaySession::kDisconnected;

		pipe->m_relaySessionMap.erase(session_id);
	}

public:
	RelayServer(bool encryptMode)
		: m_encryptMode(encryptMode)
	{
	}
	~RelayServer() 
	{
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
static void printUsage(const char* moduleName)
{
	printf("===== Relay Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -p  LISTEN_PORT\t Local Listen Port, Default 3000\n");
	printf("\t -uh UP_HOST\tUp Server(Target Server) IP, Default 127.0.0.1\n");
	printf("\t -up UP_PORT\tUp Server(Target Server) Port\n");
	printf("\t -t THREAD_COUNTS\tWork thread counts(must be 1 when relay_pipe used)\n");
	printf("\t -e\t\tEncrypt Message\n");
	printf("\t -v\t\tVerbose Mode\n");
	printf("\t --help -?\tShow this help\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	uint16_t local_port = 3000;
	std::string up_ip = "127.0.0.1";
	uint16_t up_port = 0;
	bool verbose_mode = false;
	bool encrypt_mode = false;
	int32_t work_thread_counts = sys_api::get_cpu_counts();

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

		}
		else {
			printf("Invalid argument: %s\n", args.OptionText());
			return 1;
		}
	}

	if (up_port == 0) {
		printUsage(argv[0]);
		return 0;
	}
	setLogThreshold(verbose_mode ? L_TRACE : L_DEBUG);

	CY_LOG(L_DEBUG, "listen port %d", local_port);
	CY_LOG(L_DEBUG, "final up address %s:%d", up_ip.c_str(), up_port);
	CY_LOG(L_DEBUG, "encrypt mode %s", encrypt_mode?"true":"false");
	CY_LOG(L_DEBUG, "work thread counts %d", work_thread_counts);

	RelayServer server(encrypt_mode);
	server.startAndJoin(local_port, Address(up_ip.c_str(), up_port), work_thread_counts);
	return 0;
}
