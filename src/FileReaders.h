
#pragma once
#include "pch.h"
#include "Common.h"


struct IDataSource : ui::RefCountedST
{
	virtual ~IDataSource() {}
	virtual size_t Read(uint64_t at, size_t size, void* out) = 0;
	virtual uint64_t GetSize() = 0;

	void GetASCIIText(char* buf, size_t bufsz, uint64_t pos, char fallback = '?');
	void GetInt8Text(char* buf, size_t bufsz, uint64_t pos, bool sign);
	void GetInt16Text(char* buf, size_t bufsz, uint64_t pos, Endianness endianness, bool sign);
	void GetInt32Text(char* buf, size_t bufsz, uint64_t pos, Endianness endianness, bool sign);
	void GetInt64Text(char* buf, size_t bufsz, uint64_t pos, Endianness endianness, bool sign);
	void GetFloat32Text(char* buf, size_t bufsz, uint64_t pos, Endianness endianness);
	void GetFloat64Text(char* buf, size_t bufsz, uint64_t pos, Endianness endianness);
};

struct MemoryDataSource : IDataSource
{
	MemoryDataSource(void* mem, size_t size, bool own);
	~MemoryDataSource();

	size_t Read(uint64_t at, size_t size, void* out) override;
	uint64_t GetSize() override;

	void* _mem;
	size_t _size;
	bool _own;
};

struct FileDataSource : IDataSource
{
	FileDataSource(const char* path);
	~FileDataSource();

	size_t Read(uint64_t at, size_t size, void* out) override;
	uint64_t GetSize() override;

	FILE* _fp;
	uint64_t _pos = 0;
	uint64_t _size = 0;
};

FileDataSource* GetFileDataSource(const char* path);

struct SliceDataSource : IDataSource
{
	SliceDataSource(IDataSource* src, uint64_t off, uint64_t size);
	~SliceDataSource();

	size_t Read(uint64_t at, size_t size, void* out) override;
	uint64_t GetSize() override;

	IDataSource* _src;
	uint64_t _off;
	uint64_t _size;
};

IDataSource* GetSlice(IDataSource* src, uint64_t off, uint64_t size);

