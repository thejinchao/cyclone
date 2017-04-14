#pragma once

#include <cy_crypt.h>

enum {
	RELAY_PACKET_HEADSIZE = 4
};

enum {
	RELAY_HANDSHAKE_ID = 100,
	RELAY_NEW_SESSION,
	RELAY_CLOSE_SESSION,
	RELAY_FORWARD
};

struct RelayHandshakeMsg
{
	enum { ID = RELAY_HANDSHAKE_ID };

	cyclone::dhkey_t dh_key;
};

struct RelayNewSessionMsg
{
	enum { ID = RELAY_NEW_SESSION };
	int32_t id;
};

struct RelayCloseSessionMsg
{
	enum { ID = RELAY_CLOSE_SESSION };

	int32_t id;
};

struct RelayForwardMsg
{
	enum { ID = RELAY_FORWARD };

	int32_t id;
	int32_t size;
};