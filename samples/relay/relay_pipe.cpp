#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <cy_network.h>
#include <SimpleOpt.h>

using namespace cyclone;
using namespace std::placeholders;

////////////////////////////////////////////////////////////////////////////////////////////
enum { OPT_PORT1, OPT_PORT2,  OPT_VERBOSE_MODE, OPT_HELP };

CSimpleOptA::SOption g_rgOptions[] = {
	{ OPT_PORT1, "-p1",  SO_REQ_SEP },  // "-p1 FIRST_ADDR"
	{ OPT_PORT2, "-p2",  SO_REQ_SEP },	// "-p2 SECOND_ADDR"
	{ OPT_VERBOSE_MODE, "-v",  SO_NONE },	// "-v"
	{ OPT_HELP, "-?",     SO_NONE },		// "-?"
	{ OPT_HELP, "--help", SO_NONE },		// "--help"
	SO_END_OF_OPTIONS						// END
};

////////////////////////////////////////////////////////////////////////////////////////////
class RelayPipe
{
public:
    RelayPipe(ConnectionPtr port1, ConnectionPtr port2)
    : m_port1(port1), m_port2(port2)
    {
        assert(port1->get_state()==Connection::kConnected);
        assert(port2->get_state()==Connection::kConnected);
        
        //check cache
        forward1To2();
        forward2To1();
    }
    
public:
    //-------------------------------------------------------------------------------------
    void forward1To2(void)
    {
        RingBuf& ringBuf = m_port1->get_input_buf();
        if(ringBuf.empty()) return;
        
        CY_LOG(L_TRACE, "Forward1To2: %zd", ringBuf.size());
        m_port2->send((const char*)ringBuf.normalize(), ringBuf.size());
        ringBuf.reset();
    }
    
    //-------------------------------------------------------------------------------------
    void forward2To1(void)
    {
        RingBuf& ringBuf = m_port2->get_input_buf();
        if(ringBuf.empty()) return;
        
        CY_LOG(L_TRACE, "Forward2To1: %zd", ringBuf.size());
        m_port1->send((const char*)ringBuf.normalize(), ringBuf.size());
        ringBuf.reset();
    }
private:
    ConnectionPtr m_port1, m_port2;
};
typedef std::shared_ptr<RelayPipe> RelayPipePtr;

////////////////////////////////////////////////////////////////////////////////////////////
class RelayPipe_DoubleIn
{
public:
	//-------------------------------------------------------------------------------------
	void startAndJoin(uint16_t listen_port1, uint16_t listen_port2)
	{
		TcpServer server("rp", nullptr);
		server.m_listener.onConnected = std::bind(&RelayPipe_DoubleIn::onConnected, this, _1, _3);
		server.m_listener.onMessage = std::bind(&RelayPipe_DoubleIn::onMessage, this, _3);
		server.m_listener.onClose = std::bind(&RelayPipe_DoubleIn::onClose, this, _1);

		server.bind(Address(m_port1=listen_port1, false), true);
		server.bind(Address(m_port2=listen_port2, false), true);

		if (!server.start(1)) return;

		server.join();
	}

private:
	//-------------------------------------------------------------------------------------
	void onConnected(TcpServer* server, ConnectionPtr conn)
	{
		if (m_port1 == conn->get_local_addr().get_port()) {
			if (m_conn1) {
				server->shutdown_connection(conn);
				return;
			}
            m_conn1 = conn;
            CY_LOG(L_INFO, "Receive connect from port1(%s:%d->%s:%d)",
                   conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port(),
                   conn->get_local_addr().get_ip(), conn->get_local_addr().get_port());
		}
		else
		{
			assert(m_port2 == conn->get_local_addr().get_port());

			if (m_conn2) {
				server->shutdown_connection(conn);
				return;
			}
            m_conn2 = conn;
            CY_LOG(L_INFO, "Receive connect from port2(%s:%d->%s:%d)",
                   conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port(),
                   conn->get_local_addr().get_ip(), conn->get_local_addr().get_port());
        }

		if (m_conn1 && m_conn2) {
            m_pipe = std::make_shared<RelayPipe>(m_conn1, m_conn2);
		}
	}

	//-------------------------------------------------------------------------------------
	void onMessage(ConnectionPtr conn)
	{
		if (!m_pipe) return;

		if (conn == m_conn1) {
            m_pipe->forward1To2();
		}
		else {
            m_pipe->forward2To1();
        }
	}

	//-------------------------------------------------------------------------------------
	void onClose(TcpServer* server)
	{
        m_conn1 = nullptr;
        m_conn2 = nullptr;
        m_pipe = nullptr;
        
		sys_api::thread_create_detached([server](void*) {
			server->stop();
		}, nullptr, nullptr);
	}

private:
	uint16_t m_port1;
	uint16_t m_port2;
	ConnectionPtr m_conn1;
	ConnectionPtr m_conn2;
	RelayPipePtr m_pipe;
};

////////////////////////////////////////////////////////////////////////////////////////////
class RelayPipe_DoubleOut
{
public:
	//-------------------------------------------------------------------------------------
	void startAndJoin(const Address& address1, const Address& address2)
	{
		m_address1 = address1;
		m_address2 = address2;
		thread_t thread = sys_api::thread_create(std::bind(&RelayPipe_DoubleOut::_workThread, this), nullptr, "rp");
		sys_api::thread_join(thread);
	}

private:
	//-------------------------------------------------------------------------------------
	void _workThread(void)
	{
		m_looper = Looper::create_looper();

        TcpClientPtr client1 = std::make_shared<TcpClient>(m_looper, nullptr);
		client1->m_listener.onConnected = std::bind(&RelayPipe_DoubleOut::onConnected, this, _1, _2, _3, 1);
		client1->m_listener.onMessage = std::bind(&RelayPipe_DoubleOut::onMessage, this, _1, _2);
		client1->m_listener.onClose = std::bind(&RelayPipe_DoubleOut::onClose, this, _1);

		TcpClientPtr client2 = std::make_shared<TcpClient>(m_looper, nullptr);
		client2->m_listener.onConnected = std::bind(&RelayPipe_DoubleOut::onConnected, this, _1, _2, _3, 2);
		client2->m_listener.onMessage = std::bind(&RelayPipe_DoubleOut::onMessage, this, _1, _2);
		client2->m_listener.onClose = std::bind(&RelayPipe_DoubleOut::onClose, this, _1);

        CY_LOG(L_INFO, "Connect to port1 %s:%d", m_address1.get_ip(), m_address1.get_port());
        CY_LOG(L_INFO, "Connect to port2 %s:%d", m_address2.get_ip(), m_address2.get_port());
        
		client1->connect(m_address1);
		client2->connect(m_address2);

		m_looper->loop();

        assert(client1.unique());
        assert(client2.unique());
        
		client1 = nullptr;
		client2 = nullptr;

		Looper::destroy_looper(m_looper);
	}

	//-------------------------------------------------------------------------------------
	uint32_t onConnected(TcpClientPtr client, ConnectionPtr conn, bool success, int32_t index)
	{
		if (!success) {
			uint32_t retry_time = 1000 * 5;
			CY_LOG(L_INFO, "Port%d connect failed!, retry after %d milliseconds...", index, retry_time);
			return 1000 * 5;
		}
		CY_LOG(L_INFO, "Port%d connect success(%s:%d -> %s:%d)!", index,
               conn->get_local_addr().get_ip(), conn->get_local_addr().get_port(),
               conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());
        if (index == 1) { m_client1 = client; m_connection1=conn; }
        else { m_client2 = client; m_connection2 = conn; }
        
        if(m_client1 && m_client2) {
            m_pipe = std::make_shared<RelayPipe>(m_connection1, m_connection2);
        }
		return 0;
	}

	//-------------------------------------------------------------------------------------
	void onMessage(TcpClientPtr client, ConnectionPtr conn)
	{
        if(!m_pipe) return;
        
		if (client == m_client1) {
            m_pipe->forward1To2();
        }
		else {
            m_pipe->forward2To1();
        }
	}

	//-------------------------------------------------------------------------------------
	void onClose(TcpClientPtr client)
	{
        if(!m_pipe) return;
        
        m_pipe = nullptr;
        m_connection1 = nullptr;
        m_connection2 = nullptr;
        if(client==m_client1 && m_client2) m_client2->disconnect();
        if(client==m_client2 && m_client1) m_client1->disconnect();
        m_client1 = nullptr;
        m_client2 = nullptr;
        
        sys_api::thread_create_detached([this](void*) {
            m_looper->push_stop_request();
        }, nullptr, nullptr);
	}
private:
	Address m_address1;
	Address m_address2;
	Looper* m_looper;
	TcpClientPtr m_client1;
	TcpClientPtr m_client2;
    ConnectionPtr m_connection1;
    ConnectionPtr m_connection2;
	RelayPipePtr m_pipe;

public:
	RelayPipe_DoubleOut()
		: m_looper(nullptr)
		, m_client1(nullptr)
		, m_client2(nullptr)
        , m_connection1(nullptr)
        , m_connection2(nullptr)
		, m_pipe(nullptr)
	{

	}
};

////////////////////////////////////////////////////////////////////////////////////////////
class RelayPipe_InOut
{
public:
    void startAndJoin(uint16_t bindPort, const Address& addrToConnect)
    {
        m_addrToConnect = addrToConnect;
        
        TcpServer server("rp", nullptr);
        server.m_listener.onWorkThreadStart = std::bind(&RelayPipe_InOut::onWorkthreadStart, this, _1, _3);
        server.m_listener.onConnected = std::bind(&RelayPipe_InOut::onConnectedIn, this, _1, _3);
        server.m_listener.onMessage = std::bind(&RelayPipe_InOut::onMessageIn, this, _3);
        server.m_listener.onClose = std::bind(&RelayPipe_InOut::onCloseIn, this);
        
        server.bind(Address(m_bindPort=bindPort, false), true);
        
        if (!server.start(1)) return;
        
        server.join();
    }
    
private:
    //-------------------------------------------------------------------------------------
    void onWorkthreadStart(TcpServer* server, Looper* looper)
    {
        m_server = server;
        m_looper = looper;
        
        CY_LOG(L_INFO, "Begin connect to %s:%d", m_addrToConnect.get_ip(), m_addrToConnect.get_port());
        
        m_client = std::make_shared<TcpClient>(m_looper, this);
        m_client->m_listener.onConnected = std::bind(&RelayPipe_InOut::onConnectedOut, this, _2, _3);
        m_client->m_listener.onMessage = std::bind(&RelayPipe_InOut::onMessageOut, this, _1, _2);
        m_client->m_listener.onClose = std::bind(&RelayPipe_InOut::onCloseOut, this);
        
        m_client->connect(m_addrToConnect);
    }
    //-------------------------------------------------------------------------------------
    void onConnectedIn(TcpServer* server, ConnectionPtr conn)
    {
        if(m_connIn) {
            server->shutdown_connection(conn);
            return;
        }
       
        CY_LOG(L_INFO, "PortIn connected(%s:%d -> %s:%d)",
               conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port(),
               conn->get_local_addr().get_ip(), conn->get_local_addr().get_port());
        m_connIn = conn;
        if(m_connOut) {
            m_pipe = std::make_shared<RelayPipe>(m_connIn, m_connOut);
        }
    }
    
    //-------------------------------------------------------------------------------------
    void onMessageIn(ConnectionPtr conn)
    {
        if(m_pipe)
            m_pipe->forward1To2();
    }
    //-------------------------------------------------------------------------------------
    void onCloseIn(void)
    {
        m_pipe = nullptr;
        m_connIn = nullptr;
        m_connOut = nullptr;
        if(m_client) m_client->disconnect();
        m_client = nullptr;
    }
    
private:
    //-------------------------------------------------------------------------------------
    uint32_t onConnectedOut(ConnectionPtr conn, bool success)
    {
        if(!success) return 5*1000;
        
        CY_LOG(L_INFO, "PortOut connected(%s:%d -> %s:%d)",
               conn->get_local_addr().get_ip(), conn->get_local_addr().get_port(),
               conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());
        m_connOut = conn;
        if(m_connIn) {
            m_pipe = std::make_shared<RelayPipe>(m_connIn, m_connOut);
        }
        return 0;
    }
    
    //-------------------------------------------------------------------------------------
    void onMessageOut(TcpClientPtr client, ConnectionPtr conn)
    {
        if(m_pipe) 
			m_pipe->forward2To1();
    }
    //-------------------------------------------------------------------------------------
    void onCloseOut(void)
    {
        m_pipe = nullptr;
        m_connIn = nullptr;
        m_connOut = nullptr;
        m_client = nullptr;
        
        sys_api::thread_create_detached([this](void*){
            m_server->stop();
        }, nullptr, nullptr);
    }
    
private:
    uint16_t m_bindPort;
    Address m_addrToConnect;
    TcpServer* m_server;
    Looper* m_looper;
    TcpClientPtr m_client;
    ConnectionPtr m_connIn;
    ConnectionPtr m_connOut;
    RelayPipePtr m_pipe;
    
public:
    RelayPipe_InOut() : m_server(nullptr), m_looper(nullptr), m_client(nullptr), m_pipe(nullptr)
    {
        
    }
    ~RelayPipe_InOut()
    {
        
    }
};

////////////////////////////////////////////////////////////////////////////////////////////
static void printUsage(const char* moduleName)
{
	printf("===== Relay Pipe(Powerd by Cyclone) =====\n");
	printf("Usage: %s [OPTIONS]\n\n", moduleName);
	printf("\t -p1  FIRST_ADDRESS\t First address to connect or listen\n");
	printf("\t -p2  SECOND_ADDRESS\t Second address to connect or listen\n");
	printf("\t -v\t\tVerbose Mode\n");
	printf("\t --help -?\tShow this help\n");
	printf("Example: %s -p1 2001 -p2 192.168.0.98:2002\n", moduleName);
}

////////////////////////////////////////////////////////////////////////////////////////////
bool _parserAddress(const std::string& address, std::string& ip, uint16_t& port)
{
	std::string::size_type colon = address.find(':');

	if (colon!=std::string::npos) {
		ip = address.substr(0, colon);
		port = (uint16_t)atoi(address.substr(colon+1).c_str());
		
		char check[64] = { 0 };
		std::snprintf(check, 64, "%s:%d", ip.c_str(), port);
		return (address.compare(check) == 0);
	}
	else {
		ip = "";
		port = (uint16_t)atoi(address.c_str());

		char check[64] = { 0 };
		std::snprintf(check, 64, "%d", port);
		return (address.compare(check) == 0);
	}
}


////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	CSimpleOptA args(argc, argv, g_rgOptions);

	std::string first_ip, second_ip;
	uint16_t first_port = 0, second_port = 0;
	bool first_in=false, second_in=false;
	bool verbose_mode = false;

	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				printUsage(argv[0]);
				return 0;
			}
			else if (args.OptionId() == OPT_PORT1) {
				if (!_parserAddress(std::string(args.OptionArg()), first_ip, first_port)) {
					printUsage(argv[0]);
					return 0;
				}
				first_in = first_ip.empty();
			}
			else if (args.OptionId() == OPT_PORT2) {
				if (!_parserAddress(std::string(args.OptionArg()), second_ip, second_port)) {
					printUsage(argv[0]);
					return 0;
				}
				second_in = second_ip.empty();
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

	if (first_port == 0 || second_port == 0) {
		printUsage(argv[0]);
		return 0;
	}

	setLogThreshold(verbose_mode ? L_TRACE : L_DEBUG);

	CY_LOG(L_INFO, "First Address[%s]: %s%s%d", (first_in?"IN":"OUT"), first_ip.c_str(), (first_in?"":":"), first_port);
	CY_LOG(L_INFO, "Second Address[%s]: %s%s%d", (second_in?"IN":"OUT"), second_ip.c_str(), (second_in ? "" : ":"), second_port);

	//select mode
	if (first_in && second_in) {
		//double in
		RelayPipe_DoubleIn relayPipe;
		relayPipe.startAndJoin(first_port, second_port);
	}
	else if (!first_in && !second_in) {
		//double out
		RelayPipe_DoubleOut relayPipe;
		relayPipe.startAndJoin(Address(first_ip.c_str(), first_port), Address(second_ip.c_str(), second_port));
    }else {
        //one in one out
        RelayPipe_InOut relayPipe;
        if(first_in)
            relayPipe.startAndJoin(first_port, Address(second_ip.c_str(), second_port));
        else
            relayPipe.startAndJoin(second_port, Address(first_ip.c_str(), first_port));
    }

	return 0;
}
