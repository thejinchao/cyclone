#pragma once

#include <cy_core.h>

struct FT_Head
{
	int32_t size;
	int32_t id;
};

struct FT_RequireFileInfo : public FT_Head
{
	enum { ID = 1 };
};

struct FT_ReplyFileInfo : public FT_Head
{
	enum { ID = 2 };
	size_t fileSize;
	int32_t threadCounts;
	int32_t nameLength; //file name length(with \0 end)
};

struct FT_RequireFileFragment : public FT_Head
{
	enum { ID=3 };
	size_t fileOffset;
	int32_t fragmentSize;
};

struct FT_ReplyFileFragment_Begin : public FT_Head
{
	enum { ID = 4 };
	size_t fileOffset;
	int32_t fragmentSize;
};

struct FT_ReplyFileFragment_End : public FT_Head
{
	enum { ID = 5 };
	uint32_t fragmentCRC;
};
