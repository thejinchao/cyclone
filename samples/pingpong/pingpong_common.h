#pragma once

#include <cy_core.h>

struct PingPong_Head
{
	int32_t size;
	int32_t id;
};

struct PingPong_HandShake : public PingPong_Head
{
	enum { ID = 1 };
};

struct PingPong_PingData : public PingPong_Head
{
	enum { ID = 2 };
	int32_t data_size;
};

struct PingPong_PongData : public PingPong_Head
{
	enum { ID = 3 };
	int32_t data_size;
};

struct PingPong_Close : public PingPong_Head
{
	enum { ID = 4 };
};
