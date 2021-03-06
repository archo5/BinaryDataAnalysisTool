
#pragma once
#include "pch.h"
#include "FileReaders.h"
#include "DataDesc.h"


struct Int32Highlight
{
	ui::Color4b color = { 255, 225, 0, 127 };
	int32_t vmin = 0;
	int32_t vmax = 0;
	int32_t vspec = 0;
	bool enabled = true;
	bool range = true;
};

struct HighlightSettings
{
	bool excludeZeroes = true;
	bool enableFloat32 = true;
	float minFloat32 = 0.0001f;
	float maxFloat32 = 10000;
	bool enableInt16 = true;
	int32_t minInt16 = -2000;
	int32_t maxInt16 = 2000;
	bool enableInt32 = true;
	int32_t minInt32 = -10000;
	int32_t maxInt32 = 10000;
	bool enableASCII = true;
	unsigned minASCIIChars = 3;
	bool enableNearFileSize32 = true;
	bool enableNearFileSize64 = true;
	float nearFileSizePercent = 99.0f;

	std::vector<Int32Highlight> customInt32;

	void AddCustomInt32(int32_t v);

	void Load(const char* key, NamedTextSerializeReader& r);
	void Save(const char* key, NamedTextSerializeWriter& w);

	void EditUI();
};

struct ByteColors
{
	ui::Color4f hexColor = { 0, 0 };
	ui::Color4f asciiColor = { 0, 0 };
	ui::Color4f leftBracketColor = { 0, 0 };
	ui::Color4f rightBracketColor = { 0, 0 };
};

enum class HighlightType : uint8_t
{
	ValueInRange,
	NearFileSize,
	CustomHighlight,
};

struct FoundHighlightList : ui::TableDataSource
{
	struct Item
	{
		uint64_t pos;
		DataType dataType;
		HighlightType hlType;
		union
		{
			int64_t ival;
			uint64_t uval;
			double dval;
		};
	};

	std::vector<Item> items;

	void SortByOffset();

	// TableDataSource (GenericGridDataSource)
	size_t GetNumCols() override;
	std::string GetColName(size_t col) override;
	std::string GetText(uintptr_t id, size_t col) override;
	// TableDataSource
	size_t GetNumRows() override;
	std::string GetRowName(size_t row) override;
};

struct HexViewerState
{
	uint64_t basePos = 0;
	uint32_t byteWidth = 8;
	Endianness endianness = Endianness::Little;

	ui::Color4f colorHover{ 1, 1, 1, 0.3f };
	ui::Color4f colorSelect{ 1, 0.7f, 0.65f, 0.5f };
	uint64_t hoverByte = UINT64_MAX;
	int hoverSection = -1;
	uint64_t selectionStart = UINT64_MAX;
	uint64_t selectionEnd = UINT64_MAX;
	bool mouseDown = false;

	FoundHighlightList highlightList;

	uint64_t GetInspectPos();
	void GoToPos(int64_t pos);
};
extern ui::MulticastDelegate<const HexViewerState*> OnHexViewerStateChanged;
extern ui::MulticastDelegate<const HexViewerState*> OnHexViewerInspectTargetChanged;

struct HexViewer : ui::FillerElement
{
	ui::FontSettings contentFont;

	void OnReset() override
	{
		ui::FillerElement::OnReset();

		flags |= ui::UIObject_IsFocusable | ui::UIObject_DB_FocusOnLeftClick;
		contentFont.family = ui::FONT_FAMILY_MONOSPACE;
	}
	void OnEvent(ui::Event& e) override;
	void OnPaint(const ui::UIPaintContext& ctx) override;

	ui::UIRect GetByteRect(uint64_t pos);

	uint64_t GetBasePos()
	{
		return state->basePos;
	}

	void Init(DataDesc* dd, DDFile* f, HexViewerState* hvs, HighlightSettings* hs)
	{
		dataDesc = dd;
		file = f;
		state = hvs;
		highlightSettings = hs;
	}

	// input data
	DataDesc* dataDesc = nullptr;
	DDFile* file = nullptr;
	HexViewerState* state = nullptr;
	HighlightSettings* highlightSettings = nullptr;
};
