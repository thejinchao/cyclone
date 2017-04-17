#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <cy_network.h>
#include <SimpleOpt.h>

#include "relay_protocol.h"

using namespace cyclone;
using namespace std::placeholders;

////////////////////////////////////////////////////////////////////////////////////////////
enum { OPT_PORT, OPT_UP_HOST, OPT_UP_PORT, OPT_VERBOSE_MODE, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP },  // "-p LISTEN_PORT"
	{ OPT_UP_HOST, "-uh",  SO_REQ_SEP }, // "-uh UP_SERVER_HOST"
	{ OPT_UP_PORT, "-up",  SO_REQ_SEP }, // "-up UP_SERVER_PORT"
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
public:
	//-------------------------------------------------------------------------------------
	void startAndJoin(uint16_t local_port, const Address& upAddress)
	{
		m_upAddress = upAddress;

		TcpServer server("rl", nullptr);
		server.m_listener.onWorkThreadStart = std::bind(&RelayLocal::onWorkthreadStart, this, _1, _3);
		server.m_listener.onConnected = std::bind(&RelayLocal::onLocalConnected, this, _1, _3);
		server.m_listener.onMessage = std::bind(&RelayLocal::onLocalMessage, this, _1, _3);
		server.m_listener.onClose = std::bind(&RelayLocal::onLocalClose, this, _3);

		server.bind(Address(local_port, false), true);

		if (!server.start(1)) return;

		server.join();
	}
private:
	//-------------------------------------------------------------------------------------
	void onWorkthreadStart(TcpServer* server, Looper* looper)
	{
		assert(m_upState == kConnecting);
		m_downServer = server;
        m_upClient = std::make_shared<TcpClient>(looper, this);
		m_upClient->m_listener.onConnected = std::bind(&RelayLocal::onUpConnected, this, _2, _3);
		m_upClient->m_listener.onMessage = std::bind(&RelayLocal::onUpMessage, this, _1, _2);
		m_upClient->m_listener.onClose = std::bind(&RelayLocal::onUpClose, this);

		m_upClient->connect(m_upAddress);
	}
	//-------------------------------------------------------------------------------------
	void onLocalConnected(TcpServer* server, ConnectionPtr conn)
	{
		if (m_upState!= kHandshaked) {
			server->shutdown_connection(conn);
			return;
		}
		//set tcp nodelay
		socket_api::set_nodelay(conn->get_socket(), true);

		RelaySession newSession(conn->get_id(), conn);
		m_sessionMap.insert({ conn->get_id(), newSession });

		RelayNewSessionMsg newSessionMsg;
		newSessionMsg.id = conn->get_id();

		Packet packet;
		packet.build((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayNewSessionMsg::ID, sizeof(newSessionMsg), (const char*)&newSessionMsg);
		m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());

		CY_LOG(L_TRACE, "[%d]CLIENT connected, send new session msg to UP", conn->get_id());
	}
	//-------------------------------------------------------------------------------------
	void onLocalMessage(TcpServer* server, ConnectionPtr conn)
	{
		if (m_upState != kHandshaked) {
			server->shutdown_connection(conn);
			return;
		}
		RingBuf& ringBuf = conn->get_input_buf();

		while (!ringBuf.empty()) {
			size_t msgSize = (ringBuf.size() < 0xFF00u) ? ringBuf.size() : 0xFF00u;

			RelayForwardMsg forwardMsg;
			forwardMsg.id = conn->get_id();
			forwardMsg.size = (int32_t)msgSize;

			size_t buf_round_size = _round16(msgSize);
			Packet packet;
			packet.build((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayForwardMsg::ID, (uint16_t)(sizeof(RelayForwardMsg) + buf_round_size), nullptr);

			memcpy(packet.get_packet_content(), &forwardMsg, sizeof(forwardMsg));
			ringBuf.memcpy_out(packet.get_packet_content() + sizeof(forwardMsg), msgSize);

			//encrypt and send
			uint8_t* buf = (uint8_t*)packet.get_packet_content() + sizeof(forwardMsg);
			m_encrypt->encrypt(buf, buf, buf_round_size);
			m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());
			CY_LOG(L_TRACE, "[%d]receive message from CLIENT(%zd/%zd), send to UP after encrypt", conn->get_id(), (size_t)msgSize, buf_round_size);
		}
	}

	//-------------------------------------------------------------------------------------
	void onLocalClose(ConnectionPtr conn)
	{
		auto it = m_sessionMap.find(conn->get_id());
		if (it == m_sessionMap.end()) return;

		m_sessionMap.erase(it);
        if(m_upState==kHandshaked)
        {
            RelayCloseSessionMsg closeSessionMsg;
            closeSessionMsg.id = conn->get_id();

            Packet packet;
            packet.build((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayCloseSessionMsg::ID, sizeof(closeSessionMsg), (const char*)&closeSessionMsg);
            m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());

            CY_LOG(L_TRACE, "[%d]down client closed!, send close session message to up server", conn->get_id());
        }
	}

private:
	//-------------------------------------------------------------------------------------
	uint32_t onUpConnected(ConnectionPtr conn, bool success)
	{
		assert(m_upState == kConnecting);

		if (success) {
			//set tcp nodelay
			socket_api::set_nodelay(conn->get_socket(), true);

			//send handshake message
			RelayHandshakeMsg handshake;
			handshake.dh_key = m_publicKey;

			Packet packet;
			packet.build((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayHandshakeMsg::ID, sizeof(handshake), (const char*)&handshake);
			m_upClient->send(packet.get_memory_buf(), packet.get_memory_size());

			//update state
			m_upState = kHandshaking;
			CY_LOG(L_DEBUG, "Up Server Connected, send handshake message.");
			return 0;
		}else {
			m_upState = kConnecting;
			return 10 * 1000;
		}
	}

	//-------------------------------------------------------------------------------------
	void onUpMessage(TcpClientPtr client, ConnectionPtr conn)
	{
		for (;;) {
			if (m_upState == kHandshaking) {
				//peed message id
				uint16_t packetID;
				if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) return;
				packetID = socket_api::ntoh_16(packetID);

				//must be handshake message
				if (packetID != (uint16_t)RELAY_HANDSHAKE_ID) {
					client->disconnect();
					m_upState = kDisConnected;
					return;
				}

				//get handshake message
				Packet handshakePacket;
				if (!handshakePacket.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

				//check size
				if (handshakePacket.get_packet_size() != sizeof(RelayHandshakeMsg)) {
					client->disconnect();
					m_upState = kDisConnected;
					return;
				}

				RelayHandshakeMsg handshake;
				memcpy(&handshake, handshakePacket.get_packet_content(), sizeof(handshake));

				//handshake
				DH_generate_key_secret(m_secretKey, m_privateKey, handshake.dh_key);

				//create encrypter
				m_encrypt = new Rijndael(m_secretKey.bytes);

				//create decrypter
				for (size_t i = 0; i < Rijndael::BLOCK_SIZE; i++)
					m_privateKey.bytes[i] = (uint8_t)(~m_privateKey.bytes[i]);
				m_decrypt = new Rijndael(m_secretKey.bytes);

				//update state
				m_upState = kHandshaked;

				CY_LOG(L_DEBUG, "Connect to up server(%s:%d)", conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());

				//clean secret key memory(for safe)
				memset(m_secretKey.bytes, 0, Rijndael::BLOCK_SIZE);
			}
			else if (m_upState == kHandshaked) {
				//peed message id
				uint16_t packetID;
				if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) return;
				packetID = socket_api::ntoh_16(packetID);

				switch (packetID) {
				case RELAY_FORWARD:
				{
					//get packet
					Packet packet;
					if (!packet.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

					RelayForwardMsg forwardMsg;
					memcpy(&forwardMsg, packet.get_packet_content(), sizeof(RelayForwardMsg));

					//decrypt
					uint8_t* buf = (uint8_t*)packet.get_packet_content() + sizeof(RelayForwardMsg);
					m_decrypt->decrypt(buf, buf, packet.get_packet_size() - sizeof(RelayForwardMsg));

					auto it = m_sessionMap.find(forwardMsg.id);
					if (it == m_sessionMap.end()) {

						RelayCloseSessionMsg closeSessionMsg;
						closeSessionMsg.id = forwardMsg.id;

						Packet packetCloseSession;
						packetCloseSession.build((size_t)RELAY_PACKET_HEADSIZE, (uint16_t)RelayCloseSessionMsg::ID, sizeof(closeSessionMsg), (const char*)&closeSessionMsg);
						m_upClient->send(packetCloseSession.get_memory_buf(), packetCloseSession.get_memory_size());
						break;
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
					if (!packet.build(RELAY_PACKET_HEADSIZE, conn->get_input_buf())) return;

					RelayCloseSessionMsg closeSessionMsg;
					memcpy(&closeSessionMsg, packet.get_packet_content(), sizeof(closeSessionMsg));

					auto it = m_sessionMap.find(closeSessionMsg.id);
					if (it == m_sessionMap.end()) {
						break;
					}
					ConnectionPtr downConn = it->second.m_downConnection;

					m_sessionMap.erase(it);
					m_downServer->shutdown_connection(downConn);
					CY_LOG(L_TRACE, "[%d]receive session close msg from UP, shutdown CLIENT", closeSessionMsg.id);
				}
				break;

				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	void onUpClose(void)
	{
        m_upClient = nullptr;
        m_upState = kDisConnected;
        
		sys_api::thread_create_detached([](void* server) {
			((TcpServer*)server)->stop();
		}, m_downServer, "");
	}

private:
	//-------------------------------------------------------------------------------------
	size_t _round16(size_t size) {
		return ((size & 0xF) == 0) ? size : ((size & (size_t)(~0xF))+0x10);
	}

private:
	enum UpState {
		kConnecting=0,
		kHandshaking,
		kHandshaked,
		kDisConnected
	};
	Address m_upAddress;
	TcpClientPtr m_upClient;
	UpState m_upState;
	dhkey_t m_publicKey;
	dhkey_t m_privateKey;
	dhkey_t m_secretKey;
	TcpServer* m_downServer;
	Rijndael* m_encrypt;
	Rijndael* m_decrypt;

	typedef std::map<int32_t, RelaySession> RelaySessionMap;
	RelaySessionMap m_sessionMap;

public:
	RelayLocal()
		: m_upClient(nullptr)
		, m_upState(kConnecting)
		, m_downServer(nullptr)
		, m_encrypt(nullptr)
		, m_decrypt(nullptr)
	{
		DH_generate_key_pair(m_publicKey, m_privateKey);
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
			else if (args.OptionId() == OPT_VERBOSE_MODE) {
				verbose_mode = true;
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

	RelayLocal relayLocal;
	relayLocal.startAndJoin(local_port, Address(up_ip.c_str(), up_port));
	return 0;
}
