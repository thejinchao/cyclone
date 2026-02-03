#include <cy_core.h>
#include <cy_network.h>
#include "cyt_unit_utils.h"

#include <unordered_set>

using namespace cyclone;

// helper to build sockaddr_in easily
static struct sockaddr_in make_sockaddr(const char* ip, uint16_t port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	socket_api::inet_pton(ip, addr.sin_addr);
	return addr;
}

//-------------------------------------------------------------------------------------
TEST_CASE("Address constructor test", "[Address][Constructor]")
{
	PRINT_CURRENT_TEST_NAME();

	//default constructor
	Address address1;
	REQUIRE_STREQ(address1.get_ip(), "");
	REQUIRE_EQ(address1.get_port(), 0);

	//construct with ip and port
	Address address2("192.168.1.10", (uint16_t)65000);
	REQUIRE_STREQ(address2.get_ip(), "192.168.1.10");
	REQUIRE_EQ(address2.get_port(), 65000);

	//port + loopback
	Address address3(8080, true);
	REQUIRE_STREQ(address3.get_ip(), "127.0.0.1");
	REQUIRE_EQ(address3.get_port(), 8080);

	//port + inaddrany
	Address address4(8080, false);
	REQUIRE_STREQ(address4.get_ip(), "0.0.0.0");
	REQUIRE_EQ(address4.get_port(), 8080);

	//sockaddr_in
	struct sockaddr_in sin = make_sockaddr("10.11.12.13", 12345);
	Address address5(sin);
	REQUIRE_STREQ(address5.get_ip(), "10.11.12.13");
	REQUIRE_EQ(address5.get_port(), 12345);

	//from socket
	socket_t pairSocket[2];
	Pipe::construct_socket_pipe(pairSocket);

	Address pair_sock0_0(false, pairSocket[0]); 
	Address pair_sock0_1(true, pairSocket[1]);
	REQUIRE_EQ(pair_sock0_0, pair_sock0_1);

	Address pair_sock1_0(false, pairSocket[1]);
	Address pair_sock1_1(true, pairSocket[0]);
	REQUIRE_EQ(pair_sock1_0, pair_sock1_1);
}

//-------------------------------------------------------------------------------------
TEST_CASE("Network address copy and assignment", "[Address][Assignment]")
{
	PRINT_CURRENT_TEST_NAME();

	Address a1("172.16.0.5", (uint16_t)5000);
	Address a2(a1); // copy ctor
	REQUIRE_EQ(a2, a1);
	REQUIRE_STREQ(a2.get_ip(), a1.get_ip());
	REQUIRE_EQ(a2.get_port(), a1.get_port());

	Address a3;
	a3 = a1; // assignment operator (defaulted)
	REQUIRE_EQ(a3, a1);
	REQUIRE_STREQ(a3.get_ip(), a1.get_ip());
	REQUIRE_EQ(a3.get_port(), a1.get_port());

	//copy from memory
	char temp[sizeof(Address)];
	memcpy(temp, &a1, sizeof(Address));
	Address a4;
	memcpy(&a4, temp, sizeof(Address));
	REQUIRE_EQ(a4, a1);
	REQUIRE_STREQ(a4.get_ip(), a1.get_ip());
	REQUIRE_EQ(a4.get_port(), a1.get_port());
}

//-------------------------------------------------------------------------------------
TEST_CASE("Network address comparison and ordering", "[Address][Comparison]")
{
	PRINT_CURRENT_TEST_NAME();

	Address a1("10.0.0.1", (uint16_t)0x0100);
	Address a2("10.0.0.1", (uint16_t)0x0200);
	Address a3("10.0.0.2", (uint16_t)0x0100);

	// equality
	REQUIRE_NE(a1, a2);
	REQUIRE_NE(a1, a3);
	REQUIRE_EQ(a1, Address("10.0.0.1", (uint16_t)0x0100));

	// ordering (by ip then port)
	REQUIRE_LT(a1, a2);
	REQUIRE_LT(a1, a3);
	REQUIRE_TRUE(a1 < a2);
	REQUIRE_TRUE(a1 < a3);
}

//-------------------------------------------------------------------------------------
TEST_CASE("Network address hashing and unordered_set integration", "[Address][Hash]")
{
	PRINT_CURRENT_TEST_NAME();

	Address a1("10.0.0.1", (uint16_t)1000);
	Address a2("10.0.0.1", (uint16_t)2000);
	Address a3("10.0.0.2", (uint16_t)1000);

	// direct inspect of Address::hash_value + std::hash<Address>
	uint32_t raw = Address::hash_value(make_sockaddr("10.0.0.1", 1000));
	std::size_t std_hash_value = std::hash<Address>()(a1);
	std::size_t expected = std::hash<uint32_t>()(raw);
	REQUIRE_EQ(std_hash_value, expected);

	// different addresses should (very likely) produce different std::hash values
	std::size_t h1 = std::hash<Address>()(a1);
	std::size_t h2 = std::hash<Address>()(a2);
	std::size_t h3 = std::hash<Address>()(a3);
	REQUIRE_NE(h1, h2);
	REQUIRE_NE(h1, h3);

	// unordered_set usage: uniqueness by Address equality/hash
	std::unordered_set<Address> set;
	set.insert(a1);
	set.insert(a2);
	set.insert(a3);
	// a1 and a2 are different (ports differ) so size should be 3
	REQUIRE_EQ(set.size(), 3u);

	// inserting duplicate does not change size
	set.insert(Address("10.0.0.1", (uint16_t)1000));
	REQUIRE_EQ(set.size(), 3u);
}
