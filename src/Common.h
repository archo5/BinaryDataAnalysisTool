
#pragma once
#include "pch.h"


enum class Endianness : uint8_t
{
	Little,
	Big,
};

inline Endianness EndiannessFromString(ui::StringView s)
{
	if (s == "big")
		return Endianness::Big;
	// empty string/"little" must always result in ::Little
	return Endianness::Little;
}

inline const char* EndiannessToString(Endianness e)
{
	switch (e)
	{
	case Endianness::Big: return "big";
	default: return "little";
	}
}

inline void EndiannessAdjust(uint8_t& v, Endianness e) {}
inline void EndiannessAdjust(int8_t& v, Endianness e) {}
inline void EndiannessAdjust(char& v, Endianness e) {}

inline void EndiannessAdjust(uint16_t& v, Endianness e)
{
	if (e == Endianness::Big)
	{
		v = (v >> 8) | (v << 8);
	}
}
UI_FORCEINLINE void EndiannessAdjust(int16_t& v, Endianness e)
{
	EndiannessAdjust(reinterpret_cast<uint16_t&>(v), e);
}
inline void EndiannessAdjust(uint32_t& v, Endianness e)
{
	if (e == Endianness::Big)
	{
		v = (v >> 24) |
			(v << 24) |
			((v & 0xff00) << 8) |
			((v & 0xff0000) >> 8);
	}
}
UI_FORCEINLINE void EndiannessAdjust(int32_t& v, Endianness e)
{
	EndiannessAdjust(reinterpret_cast<uint32_t&>(v), e);
}
UI_FORCEINLINE void EndiannessAdjust(float& v, Endianness e)
{
	EndiannessAdjust(reinterpret_cast<uint32_t&>(v), e);
}
inline void EndiannessAdjust(uint64_t& v, Endianness e)
{
	if (e == Endianness::Big)
	{
		v = (v >> 56) |
			(v << 56) |
			((v & (0xffULL << 8)) << 40) |
			((v & (0xffULL << 48)) >> 40) |
			((v & (0xffULL << 16)) << 24) |
			((v & (0xffULL << 40)) >> 24) |
			((v & (0xffULL << 24)) << 8) |
			((v & (0xffULL << 32)) >> 8);
	}
}
UI_FORCEINLINE void EndiannessAdjust(int64_t& v, Endianness e)
{
	EndiannessAdjust(reinterpret_cast<uint64_t&>(v), e);
}
UI_FORCEINLINE void EndiannessAdjust(double& v, Endianness e)
{
	EndiannessAdjust(reinterpret_cast<uint64_t&>(v), e);
}

