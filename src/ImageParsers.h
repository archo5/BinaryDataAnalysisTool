
#pragma once
#include "pch.h"
#include "FileReaders.h"


enum class PaletteMode : uint8_t
{
	None,
	Index8,
	Low4High4, // [x%2=0] => 4 low bits, [x%2=1] => 4 high bits
};

const char* PaletteModeToString(PaletteMode m);
PaletteMode PaletteModeFromString(ui::StringView s);

struct ImageInfo
{
	int64_t offImg;
	int64_t offPal;
	uint32_t width;
	uint32_t height;
	PaletteMode paletteMode = PaletteMode::None;
	bool opaque = false;

	ImageInfo() {}
	bool operator == (const ImageInfo& o) const
	{
		return offImg == o.offImg
			&& offPal == o.offPal
			&& width == o.width
			&& height == o.height
			&& paletteMode == o.paletteMode
			&& opaque == o.opaque;
	}
};

struct IImageFormatSettings : ui::RefCountedST
{
	virtual void EditUI() = 0;
	virtual std::string ExportSettingsBlob();
	virtual void LoadSettings(ui::NamedTextSerializeReader& r) = 0;
	virtual void SaveSettings(ui::NamedTextSerializeWriter& w) = 0;
};
using ImageFormatSettingsHandle = ui::RCHandle<IImageFormatSettings>;

size_t GetImageFormatCount();
ui::StringView GetImageFormatCategory(size_t fid);
ui::StringView GetImageFormatName(size_t fid);
ImageFormatSettingsHandle CreateImageFormatSettings(ui::StringView fmt);
ui::draw::ImageHandle CreateImageFrom(IDataSource* ds, ui::StringView fmt, IImageFormatSettings* settings, const ImageInfo& info);
