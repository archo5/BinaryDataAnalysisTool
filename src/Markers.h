
#pragma once
#include "pch.h"
#include "FileReaders.h"
#include "StructScript.h"


enum DataType
{
	DT_CHAR,
	DT_I8,
	DT_U8,
	DT_I16,
	DT_U16,
	DT_I32,
	DT_U32,
	DT_I64,
	DT_U64,
	DT_F32,
	DT_F64,

	DT__COUNT,
};

const char* GetDataTypeName(DataType t);

struct AnalysisResult
{
	enum Flags
	{
		Equal = 1 << 0,
		Asc = 1 << 1,
		AscEq = 1 << 2,
	};

	uint64_t count = 0;
	uint64_t unique = 0;
	uint32_t flags = 0;
	// values
	std::string vmin;
	std::string vmax;
	std::string vgcd;
	// deltas
	std::string dmin;
	std::string dmax;
	std::string dgcd;
};

struct AnalysisData : ui::TableDataSource
{
	std::vector<AnalysisResult> results;

	size_t GetNumRows() override { return results.size(); }
	size_t GetNumCols() override;
	std::string GetRowName(size_t row) override;
	std::string GetColName(size_t col) override;
	std::string GetText(size_t row, size_t col) override;
};

struct Marker
{
	std::string def;
	BDSScript compiled;

	uint64_t at;
	uint64_t repeats;
	uint64_t stride;
	std::string notes;

	unsigned ContainInfo(uint64_t pos, ui::Color4f* col) const; // 1 - overlap, 2 - left edge, 4 - right edge
	uint64_t GetEnd() const;
};
extern ui::MulticastDelegate<const Marker*> OnMarkerChange;

struct MarkerData
{
	void AddMarker(DataType dt, Endianness endianness, uint64_t from, uint64_t to);

	void Load(const char* key, NamedTextSerializeReader& r);
	void Save(const char* key, NamedTextSerializeWriter& w);

	std::vector<Marker> markers;
};
extern ui::MulticastDelegate<const MarkerData*> OnMarkerListChange;

struct MarkerDataSource : ui::TableDataSource, ui::ISelectionStorage
{
	size_t GetNumRows() override { return data->markers.size(); }
	size_t GetNumCols() override;
	std::string GetRowName(size_t row) override;
	std::string GetColName(size_t col) override;
	std::string GetText(size_t row, size_t col) override;

	void ClearSelection() override;
	bool GetSelectionState(uintptr_t item) override;
	void SetSelectionState(uintptr_t item, bool sel) override;

	ui::RCHandle<IDataSource> dataSource;
	MarkerData* data;
	size_t selected = SIZE_MAX;
};

struct MarkedItemEditor : ui::Buildable
{
	void Build() override;

	IDataSource* dataSource;
	Marker* marker;
	AnalysisData analysisData;
};

struct MarkedItemsList : ui::Buildable
{
	void Build() override;

	MarkerData* markerData;
};
