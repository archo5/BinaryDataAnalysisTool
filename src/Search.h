
#pragma once
#include "pch.h"
#include "FileReaders.h"


struct FragmentSearch : ui::TableDataSource
{
	std::string textFragment;
	uint32_t bytesBeforeMatch = 16;
	uint32_t bytesAfterMatch = 16;

	std::vector<uint64_t> results;
	size_t matchWidth = 0;
	ui::RCHandle<IDataSource> resultSource;

	void PerformSearch(IDataSource* ds);

	void SearchUI(IDataSource* ds);

	// GenericGridDataSource(TableDataSource)
	size_t GetNumCols() override;
	std::string GetColName(size_t col) override;
	std::string GetText(uintptr_t id, size_t col) override;
	// TableDataSource
	size_t GetNumRows() override;
	std::string GetRowName(size_t row) override;
};

struct FileFormatSearch : ui::TableDataSource
{
	struct Result
	{
		uint64_t offset;
		uint64_t size;
		uint32_t format;
		std::string desc;
	};

	uint32_t formats = uint32_t(-1);

	std::vector<Result> results;
	ui::RCHandle<IDataSource> resultSource;

	void PerformSearch(IDataSource* ds);

	void SearchUI(IDataSource* ds);

	// GenericGridDataSource(TableDataSource)
	size_t GetNumCols() override;
	std::string GetColName(size_t col) override;
	std::string GetText(uintptr_t id, size_t col) override;
	// TableDataSource
	size_t GetNumRows() override;
	std::string GetRowName(size_t row) override;
};
