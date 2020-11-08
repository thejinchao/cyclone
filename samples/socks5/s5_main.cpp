#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include <utility/cyu_simple_opt.h>

#ifndef CY_SYS_WINDOWS
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "s5_protocol.h"

using namespace cyclone;
using namespace std::placeholders;

////////////////////////////////////////////////////////////////////////////////////////////
enum { OPT_PORT, OPT_THREADS, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT, "-p",     SO_REQ_SEP }, // "-p LISTEN_PORT"
	{ OPT_THREADS, "-t",  SO_REQ_SEP }, // "-t THREAD_COUNTS"
	{ OPT_HELP, "-?",     SO_NONE },	// "-?"
	{ OPT_HELP, "--help", SO_NONE },	// "--help"
	SO_END_OF_OPTIONS                   // END
};

////////////////////////////////////////////////////////////////////////////////////////////
enum S5State {
	S5_OPENING = 0,
	S5_OPENED,
	S5_CONNECTING,
	S5_CONNECTED,
	S5_DISCONNECTED,
};

////////////////////////////////////////////////////////////////////////////////////////////
class S5Tunnel
{
public:
	void set_state(S5State state) { m_state = state; }
	S5State get_state(void) const { return m_state; }

	void connect(const Address& address){
		m_address = address;
        m_remoteConnection = std::make_shared<TcpClient>(m_looper, this);
		m_remoteConnection->m_listener.on_connected = std::bind(&S5Tunnel::onServerConnected, this, _2, _3);
		m_remoteConnection->m_listener.on_message = std::bind(&S5Tunnel::onServerMessage, this, _2);
		m_remoteConnection->m_listener.on_close = std::bind(&S5Tunnel::onServerClose, this);

		m_remoteConnection->connect(address);
	}

	void forword_up(RingBuf& inputBuf) {
		m_remoteConnection->send((const char*)inputBuf.normalize(), inputBuf.size());
		inputBuf.reset();
	}

	void disconnect(void) {
		if (m_remoteConnection)
			m_remoteConnection->disconnect();
	}
private:
	uint32_t onServerConnected(TcpConnectionPtr conn, bool success) 
	{
		assert(get_state()== S5_CONNECTING);
		RingBuf outputBuf;

		//send act to client
		s5_build_connect_act(outputBuf, success ? 0 : 0X04, m_address);
		m_localConnection->send((const char*)outputBuf.normalize(), outputBuf.size());

		//set next state
		if (success) {
			m_state = S5_CONNECTED;
			CY_LOG(L_INFO, "tunnel[%d]: connect to \"%s:%d\" OK",
				m_localConnection->get_id(), m_address.get_ip(), m_address.get_port());
		}
		else {
			m_state = S5_DISCONNECTED;
			m_localServer->shutdown_connection(m_localConnection);
			CY_LOG(L_WARN, "tunnel[%d]: connect to \"%s:%d\" FAILED",
				m_localConnection->get_id(), m_address.get_ip(), m_address.get_port());
		}
		return 0;
	}

	void onServerMessage(TcpConnectionPtr conn) 
	{
		RingBuf& buf = conn->get_input_buf();
		m_localConnection->send((const char*)buf.normalize(), buf.size());
		buf.reset();
	}

	void onServerClose(void) 
	{
		m_state = S5_DISCONNECTED;
		m_localServer->shutdown_connection(m_localConnection);
	}

private:
	S5State m_state;
	TcpServer* m_localServer;
	Looper* m_looper;
	TcpClientPtr m_remoteConnection;
	TcpConnectionPtr m_localConnection;
	Address m_address;

public:
	S5Tunnel(TcpServer* localServer, Looper* looper, TcpConnectionPtr localConnection)
		: m_state(S5_OPENING)
		, m_localServer(localServer)
		, m_looper(looper)
		, m_remoteConnection(nullptr)
		, m_localConnection(localConnection)
	{
	}
	~S5Tunnel() {
		if (m_remoteConnection) {
            assert(m_remoteConnection.use_count()==1);
			m_remoteConnection = nullptr;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
class S5ThreadContext
{
public:
	void set_looper(TcpServer* localServer, Looper* looper) {
		m_looper = looper;
		m_localServer = localServer;
	}
	void add_new_tunnel(TcpConnectionPtr conn) {
		m_tunnelMap.insert({ conn->get_id(), S5Tunnel(m_localServer, m_looper, conn)});
	}
	S5Tunnel* get_tunnel(int32_t conn_id) {
		auto it = m_tunnelMap.find(conn_id);
		if (it == m_tunnelMap.end()) return nullptr;

		return &(it->second);
	}
	void close_tunnel(int32_t conn_id) {
		auto it = m_tunnelMap.find(conn_id);
		if (it == m_tunnelMap.end()) return;

		it->second.disconnect();
		m_tunnelMap.erase(it);
	}

private:
	typedef std::map<int32_t, S5Tunnel> TcpTunnelMap;

	TcpTunnelMap m_tunnelMap;
	Looper* m_looper;
	TcpServer* m_localServer;
};

////////////////////////////////////////////////////////////////////////////////////////////
class S5Server
{
public:
	void startAndJoin(int32_t work_thread_counts, uint16_t server_port)
	{
		m_threadContext.resize((size_t)work_thread_counts);

		TcpServer server;
		server.m_listener.on_work_thread_start = std::bind(&S5Server::onWorkthreadStart, this, _1, _2, _3);
		server.m_listener.on_connected = std::bind(&S5Server::onPeerConnected, this, _1, _2, _3);
		server.m_listener.on_message = std::bind(&S5Server::onPeerMessage, this, _1, _2, _3);
		server.m_listener.on_close = std::bind(&S5Server::onPeerClose, this, _1, _2, _3);

		if (!server.bind(Address(server_port, false), false))
			return;

		if (!(server.start(work_thread_counts)))
			return;

		server.join();
	}

private:
	//-------------------------------------------------------------------------------------
	void onWorkthreadStart(TcpServer* server, int32_t thread_index, Looper* looper)
	{
		assert(thread_index >= 0 && thread_index<(int32_t)m_threadContext.size());
		S5ThreadContext& threadContext = m_threadContext[(size_t)thread_index];
        
		threadContext.set_looper(server, looper);
	};

	//-------------------------------------------------------------------------------------
	void onPeerConnected(TcpServer*, int32_t thread_index, TcpConnectionPtr conn)
	{
		assert(thread_index>=0 && thread_index<(int32_t)m_threadContext.size());
		S5ThreadContext& threadContext = m_threadContext[(size_t)thread_index];

		//create new socks5 tunnel
		threadContext.add_new_tunnel(conn);
		CY_LOG(L_INFO, "tunnel[%d]: new tunnel", conn->get_id());
	}

	//-------------------------------------------------------------------------------------
	void onPeerMessage(TcpServer* server, int32_t thread_index, TcpConnectionPtr conn)
	{
		assert(thread_index >= 0 && thread_index<(int32_t)m_threadContext.size());
		S5ThreadContext& threadContext = m_threadContext[(size_t)thread_index];

		RingBuf& inputBuf = conn->get_input_buf();

		//find tunnel
		S5Tunnel* tunnel = threadContext.get_tunnel(conn->get_id());
		assert(tunnel != nullptr);
		if (tunnel == nullptr) {
			server->shutdown_connection(conn);
			return;
		}

		//switch state
		switch (tunnel->get_state()) {
		case S5_OPENING:
		{
			//handshake
			int32_t s5_ret = s5_get_handshake(inputBuf);
			if (s5_ret != S5ERR_SUCCESS) {
				CY_LOG(L_WARN, "tunnel[%d]: handshake error, code=%d", conn->get_id(), s5_ret);
				server->shutdown_connection(conn);
				return;
			}
			CY_LOG(L_INFO, "tunnel[%d]: handshake ok", conn->get_id());

			//handshake ok, send handshake act
			RingBuf outputBuf;
			s5_build_handshake_act(outputBuf);

			tunnel->set_state(S5_OPENED);
			conn->send((const char*)outputBuf.normalize(), outputBuf.size());
		}
		break;

		case S5_OPENED:
		{
			Address address;
			std::string domain;
			int32_t s5_ret = s5_get_connect_request(inputBuf, address, domain);
			if (s5_ret != S5ERR_SUCCESS) {
				CY_LOG(L_WARN, "get connect request error, code=%d", s5_ret);
				server->shutdown_connection(conn);
				return;
			}

			CY_LOG(L_INFO, "tunnel[%d]: begin connect to \"%s:%d\"", conn->get_id(), domain.c_str(), address.get_port());

			//begin connect to client
			tunnel->set_state(S5_CONNECTING);
			tunnel->connect(address);
		}
		break;

		case S5_CONNECTED:
		default:
		{
			//forword
			tunnel->forword_up(inputBuf);
		}
		break;

		}
	}

	//-------------------------------------------------------------------------------------
	virtual void onPeerClose(TcpServer*, int32_t thread_index, TcpConnectionPtr conn)
	{
		assert(thread_index >= 0 && thread_index<(int32_t)m_threadContext.size());
		S5ThreadContext& threadContext = m_threadContext[(size_t)thread_index];

		threadContext.close_tunnel(conn->get_id());

		CY_LOG(L_INFO, "tunnel[%d]: close", conn->get_id());
	}


private:
	typedef std::vector<S5ThreadContext> ThreadContextVec;
	ThreadContextVec m_threadContext;
};

////////////////////////////////////////////////////////////////////////////////////////////
static void printUsage(const char* moduleName)
{
	printf("===== Socks5 Server(Powerd by Cyclone) =====\n");
	printf("Usage: %s [-p LISTEN_PORT] [-t THREAD_COUNTS] [-?] [--help]\n", moduleName);
}

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);
	uint16_t server_port = 1984;
	int32_t work_thread_counts = sys_api::get_cpu_counts();

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_PORT) {
				server_port = (uint16_t)atoi(args.OptionArg());
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

	S5Server server;
	server.startAndJoin(work_thread_counts, server_port);
	
	return 0;
}
