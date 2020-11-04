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
	int32_t work_mode;
	int32_t counts;
};

struct PingPong_PingData : public PingPong_Head
{
	enum { ID = 2 };
	int32_t data_size;
	int32_t index;
};

struct PingPong_PongData : public PingPong_Head
{
	enum { ID = 3 };
	int32_t data_size;
	int32_t index;
};

struct PingPong_Close : public PingPong_Head
{
	enum { ID = 4 };
	uint32_t data_crc;
};
