
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
