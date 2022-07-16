
#pragma once
#include "pch.h"

#include "Common.h"
#include "MathExpr.h"


struct BDSSParam
{
	std::string name;
	int64_t value = 0;
};

struct BDSSFastMask
{
	uint64_t mask;
	uint64_t lastBit;
	uint64_t extMask;
};

struct BDSSField : ui::RefCountedST
{
	std::string name;
	std::string typeName;
	MathExpr elemCountExpr;
	ui::Optional<uint64_t> fixedElemCount;
	MathExpr offsetExpr;
	ui::Optional<uint64_t> fixedOffset;
	Endianness endianness = Endianness::Little;
	BDSSFastMask valueMask = { ~0ULL, 0, 0 };
	bool readUntil0 = false;
	bool excludeZeroes = false;
	bool countIsMaxSize = false;
};

struct BDSSStruct : ui::RefCountedST
{
	std::string name;
	std::vector<BDSSParam> params;
	MathExpr sizeExpr;
	ui::Optional<uint64_t> fixedSize;
	std::vector<ui::RCHandle<BDSSField>> fields;
};

struct BDSScript
{
	std::vector<ui::RCHandle<BDSSStruct>> structs;

	bool Parse(ui::StringView s, bool oneStruct);
};
