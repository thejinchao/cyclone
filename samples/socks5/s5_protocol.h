#pragma once

#include <cy_core.h>
#include <cy_network.h>

using namespace cyclone;

//all socks5 function return value
#define S5ERR_SUCCESS			(0)
#define S5ERR_NEED_MORE_DATA	(-1)
#define S5ERR_WRONG_VERSION		(-2)
#define S5ERR_WRONG_METHOD		(-3)
#define S5ERR_PROTOCOL_ERR		(-4)
#define S5ERR_NOTSUPPORT		(-5)
#define S5ERR_DNS_FAILED		(-6)

//supported version
#define S5_VERSION				(0x05)
//supproted method(NO AUTHENTICATION REQUIRED)
#define S5_METHOD_NO_AUTH		(0x00)
//the only command we need care
#define S5_CMD_CONNECT			(0x01)

/* Test version and method
*
* From RFC1928:
* The client connects to the server, and sends a version
* identifier/method selection message:
*
* +----+----------+----------+
* |VER | NMETHODS | METHODS  |
* +----+----------+----------+
* | 1  |    1     | 1 to 255 |
* +----+----------+----------+
*
* The values currently defined for METHOD are:
* o  X'00' NO AUTHENTICATION REQUIRED (supported)
* o  X'01' GSSAPI (not supported)
* o  X'02' USERNAME/PASSWORD (not supported)
* o  X'03' to X'7F' IANA ASSIGNED (not supported)
* o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS (not supported)
* o  X'FF' NO ACCEPTABLE METHODS
*
*/
int32_t s5_get_handshake(RingBuf& inputBuf);

/*Build version packet ack in buf
*
* From RFC1928:
* The server selects from one of the methods given in METHODS, and
* sends a METHOD selection message:
*
* +----+--------+
* |VER | METHOD |
* +----+--------+
* | 1  |   1    |
* +----+--------+
*
*/
void s5_build_handshake_act(RingBuf& outputBuf);

/* Test request packet in buf, and execute the request
*
* Return:
* 	-EINVAL, invalid argument (typ, cmd, udp)
*      -EAGAIN, expects more bytes in buffer
*      -1, other error
* 	 0, success
*
* From RFC1928:
* The SOCKS request is formed as follows:
* +----+-----+-------+------+----------+----------+
* |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
* +----+-----+-------+------+----------+----------+
* | 1  |  1  | X'00' |  1   | Variable |    2     |
* +----+-----+-------+------+----------+----------+
*
* Where:
* o  VER    protocol version: X'05'
* o  CMD
*	 o  CONNECT X'01' ( define in S5_CMD_CONNECT )
*	 o  BIND X'02'    (not supported)
*	 o  UDP ASSOCIATE X'03'  (not supported)
* o  RSV    RESERVED
* o  ATYP   address type of following address
*	 o  IP V4 address: X'01'
*	 o  DOMAINNAME: X'03'
*	 o  IP V6 address: X'04' (not supported)
* o  DST.ADDR       desired destination address
* o  DST.PORT desired destination port in network octet
*	 order
*
*/
int32_t s5_get_connect_request(RingBuf& inputBuf, Address& address, std::string& domain);

/*Build request packet ack in buf
*
* From RFC1928:
* The server verifies the supplied UNAME and PASSWD, and sends the
* following response:
* +----+-----+-------+------+----------+----------+
* |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
* +----+-----+-------+------+----------+----------+
* | 1  |  1  | X'00' |  1   | Variable |    2     |
* +----+-----+-------+------+----------+----------+
*
* Where:
* o  VER    protocol version: X'05'
* o  REP    Reply field:
*	 o  X'00' succeeded
*	 o  X'01' general SOCKS server failure
*	 o  X'02' connection not allowed by ruleset
*	 o  X'03' Network unreachable
*	 o  X'04' Host unreachable
*	 o  X'05' Connection refused
* 	 o  X'06' TTL expired
*	 o  X'07' Command not supported
*	 o  X'08' Address type not supported
*	 o  X'09' to X'FF' unassigned
* o  RSV    RESERVED (must be set to 0x00)
* o  ATYP   address type of following address
* o  IP V4 address: X'01'
*	 o  DOMAINNAME: X'03'
*	 o  IP V6 address: X'04'
* o  BND.ADDR       server bound address
* o  BND.PORT       server bound port in network octet order
*
*/
void s5_build_connect_act(RingBuf& outputBuf, uint8_t reply, const Address& address);
