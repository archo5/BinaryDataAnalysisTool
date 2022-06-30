
#include "pch.h"
#include "ImageParsers.h"


const char* PaletteModeToString(PaletteMode m)
{
	switch (m)
	{
	case PaletteMode::None: return "None";
	case PaletteMode::Index8: return "Index8";
	case PaletteMode::Low4High4: return "Low4High4";
	default: return "???";
	}
}

PaletteMode PaletteModeFromString(ui::StringView s)
{
	if (s == "Index8") return PaletteMode::Index8;
	if (s == "Low4High4") return PaletteMode::Low4High4;
	return PaletteMode::None;
}

std::string IImageFormatSettings::ExportSettingsBlob()
{
	NamedTextSerializeWriter w;
	SaveSettings(w);
	return w.data;
}


static uint32_t divup(uint32_t x, uint32_t d)
{
	return (x + d - 1) / d;
}


static uint32_t RGB5A1_to_RGBA8(uint16_t v)
{
	if (v == 0)
		return 0;
	uint32_t r = ((v >> 0) & 0x1f) << 3;
	uint32_t g = ((v >> 5) & 0x1f) << 3;
	uint32_t b = ((v >> 10) & 0x1f) << 3;
	return (r << 0) | (g << 8) | (b << 16) | 0xff000000;
}

static uint32_t RGB565_to_RGBA8(uint16_t v)
{
	uint32_t b = ((v >> 0) & 0x1f) << 3;
	uint32_t g = ((v >> 5) & 0x3f) << 2;
	uint32_t r = ((v >> 11) & 0x1f) << 3;
	return (r << 0) | (g << 8) | (b << 16) | (0xffUL << 24);
}


struct ReadImageIO
{
	ui::Canvas& canvas;
	uint8_t* bytes;
	uint32_t* pixels;
	IDataSource* ds;
	IImageFormatSettings* settings;
	bool error;
};

typedef void ReadImage(ReadImageIO& io, const ImageInfo& info);
typedef IImageFormatSettings* CreateImgFmtSettings();
struct ImageFormat
{
	ui::StringView category;
	ui::StringView name;
	ReadImage* readFunc;
	CreateImgFmtSettings* cifsFunc;

};


static void ExpandBytes(uint8_t* bytes, size_t count, uint8_t bits)
{
	switch (bits)
	{
	case 1:
		for (size_t i = 0; i < count; i++)
			bytes[i] = bytes[i] ? 255 : 0;
		break;
	case 2:
		for (size_t i = 0; i < count; i++)
		{
			uint8_t b = bytes[i];
			bytes[i] = b | (b << 2) | (b << 4) | (b << 6);
		}
		break;
	case 3:
		for (size_t i = 0; i < count; i++)
		{
			uint8_t b = bytes[i];
			bytes[i] = (b << 5) | (b << 2) | (b >> 1);
		}
		break;
	case 4:
		for (size_t i = 0; i < count; i++)
		{
			uint8_t b = bytes[i];
			bytes[i] = (b << 4) | b;
		}
		break;
	case 5:
		for (size_t i = 0; i < count; i++)
		{
			uint8_t b = bytes[i];
			bytes[i] = (b << 3) | (b >> 2);
		}
		break;
	case 6:
		for (size_t i = 0; i < count; i++)
		{
			uint8_t b = bytes[i];
			bytes[i] = (b << 2) | (b >> 4);
		}
		break;
	case 7:
		for (size_t i = 0; i < count; i++)
		{
			uint8_t b = bytes[i];
			bytes[i] = (b << 1) | (b >> 6);
		}
		break;
	}
}

struct RGBAParserConfigData
{
	enum Flags
	{
		// (PSX) only apply alpha if RGB is black
		BlackOnlyAlpha = 1 << 0,
	};

	bool gray = false;
	uint8_t flags = 0;
	int bytesPerPixelGray = 1;
	int bytesPerPixelRGBA = 4;
	Endianness endianness = Endianness::Little;
	int bitsR = 8, bitsG = 8, bitsB = 8, bitsA = 0, bitsL = 8;
	int shiftR = 24, shiftG = 16, shiftB = 8, shiftA = 0, shiftL = 0;

	bool IsEquivalentTo(const RGBAParserConfigData& o)
	{
		if (gray != o.gray) return false;
		if (flags != o.flags) return false;
		if (gray)
		{
			if (bytesPerPixelGray != o.bytesPerPixelGray) return false;
			if (bytesPerPixelGray > 1 && endianness != o.endianness) return false;

			if (bitsL != o.bitsL) return false;
			if (bitsL && shiftL != o.shiftL) return false;
		}
		else
		{
			if (bytesPerPixelRGBA != o.bytesPerPixelRGBA) return false;
			if (bytesPerPixelRGBA > 1 && endianness != o.endianness) return false;

			if (bitsR != o.bitsR) return false;
			if (bitsR && shiftR != o.shiftR) return false;

			if (bitsG != o.bitsG) return false;
			if (bitsG && shiftG != o.shiftG) return false;

			if (bitsB != o.bitsB) return false;
			if (bitsB && shiftB != o.shiftB) return false;
		}

		if (bitsA != o.bitsA) return false;
		if (bitsA && shiftA != o.shiftA) return false;

		return true;
	}
	void ApplyMinimal(const RGBAParserConfigData& o)
	{
		gray = o.gray;
		flags = o.flags;
		if (gray)
		{
			bytesPerPixelGray = o.bytesPerPixelGray;
			if (bytesPerPixelGray > 1)
				endianness = o.endianness;

			bitsL = o.bitsL;
			shiftL = o.shiftL;
		}
		else
		{
			bytesPerPixelRGBA = o.bytesPerPixelRGBA;
			if (bytesPerPixelRGBA > 1)
				endianness = o.endianness;

			bitsR = o.bitsR;
			shiftR = o.shiftR;
			bitsG = o.bitsG;
			shiftG = o.shiftG;
			bitsB = o.bitsB;
			shiftB = o.shiftB;
		}

		bitsA = o.bitsA;
		shiftA = o.shiftA;
	}

	RGBAParserConfigData& RGBA(int bpp) { gray = false; bytesPerPixelRGBA = bpp; return *this; }
	RGBAParserConfigData& Gray(int bpp, int bits, int shift) { gray = true; bytesPerPixelGray = bpp; bitsL = bits; shiftL = shift; return *this; }
	RGBAParserConfigData& R(int bits, int shift) { bitsR = bits; shiftR = shift; return *this; }
	RGBAParserConfigData& G(int bits, int shift) { bitsG = bits; shiftG = shift; return *this; }
	RGBAParserConfigData& B(int bits, int shift) { bitsB = bits; shiftB = shift; return *this; }
	RGBAParserConfigData& A(int bits, int shift) { bitsA = bits; shiftA = shift; return *this; }
	RGBAParserConfigData& F(uint8_t flg) { flags = flg; return *this; }
};

struct RGBAParserPreset
{
	const char* name;
	RGBAParserConfigData data;
};

static const RGBAParserPreset g_rgbaParserPresets[] =
{
	{ "G8", RGBAParserConfigData().Gray(1, 8, 0) },
	{ "RGBA8", {} }, // same as defaults
	{ "BGRA8", RGBAParserConfigData().R(8, 8).B(8, 24) },
	{ "ARGB8", RGBAParserConfigData().A(8, 24).R(8, 16).G(8, 8).B(8, 0) },
	{ "ABGR8", RGBAParserConfigData().A(8, 24).B(8, 16).G(8, 8).R(8, 0) },
	{ "R5G6B5", RGBAParserConfigData().RGBA(2).R(5, 0).G(6, 5).B(5, 11).A(0, 0) },
	{ "[PSX] RGB5A1", RGBAParserConfigData().RGBA(2).R(5, 0).G(5, 5).B(5, 10).A(1, 15).F(RGBAParserConfigData::BlackOnlyAlpha) },
};

struct RGBAImageFormatSettings : IImageFormatSettings, RGBAParserConfigData
{
	int FindActivePreset()
	{
		for (auto& P : g_rgbaParserPresets)
		{
			if (IsEquivalentTo(P.data))
				return &P - g_rgbaParserPresets;
		}
		return -1;
	}

	void EditUI() override
	{
		using namespace ui::imm;

		int preset = FindActivePreset() + 1;

		auto* opts = ui::BuildAlloc<ui::StringArrayOptionList>();
		opts->options.push_back("-");
		for (auto& P : g_rgbaParserPresets)
			opts->options.push_back(P.name);

		if (PropDropdownMenuList("Preset", preset, opts) && preset > 0)
		{
			ApplyMinimal(g_rgbaParserPresets[preset - 1].data);
		}

		ui::LabeledProperty::Begin("Mode");
		RadioButton(gray, false, "RGBA");
		RadioButton(gray, true, "Gray");
		ui::LabeledProperty::End();

		ui::LabeledProperty::Begin("Flags");
		if (RadioButtonRaw(flags & BlackOnlyAlpha, "Black-only alpha", {}, ButtonStateToggleSkin()))
			flags ^= BlackOnlyAlpha;
		ui::LabeledProperty::End();

		PropDropdownMenuList("Endianness", endianness, ui::BuildAlloc<ui::ZeroSepCStrOptionList>("Little\0Big\0"));

		if (gray)
		{
			PropEditInt("Bytes per pixel", bytesPerPixelGray, {}, {}, { 1, 2 });

			ui::LabeledProperty::Begin("Luminance");
			PropEditInt("\bBits", bitsL, {}, {}, { 0, 8 });
			PropEditInt("\bShift", shiftL, {}, {}, { 0, 31 });
			ui::LabeledProperty::End();
		}
		else
		{
			PropEditInt("Bytes per pixel", bytesPerPixelRGBA, {}, {}, { 1, 4 });

			ui::LabeledProperty::Begin("Red");
			PropEditInt("\bBits", bitsR, {}, {}, { 0, 8 });
			PropEditInt("\bShift", shiftR, {}, {}, { 0, 31 });
			ui::LabeledProperty::End();

			ui::LabeledProperty::Begin("Green");
			PropEditInt("\bBits", bitsG, {}, {}, { 0, 8 });
			PropEditInt("\bShift", shiftG, {}, {}, { 0, 31 });
			ui::LabeledProperty::End();

			ui::LabeledProperty::Begin("Blue");
			PropEditInt("\bBits", bitsB, {}, {}, { 0, 8 });
			PropEditInt("\bShift", shiftB, {}, {}, { 0, 31 });
			ui::LabeledProperty::End();
		}

		ui::LabeledProperty::Begin("Alpha");
		PropEditInt("\bBits", bitsA, {}, {}, { 0, 8 });
		PropEditInt("\bShift", shiftA, {}, {}, { 0, 31 });
		ui::LabeledProperty::End();
	}
	void LoadSettings(ui::NamedTextSerializeReader& r) override
	{
		r.BeginDict("formatSettingsRGBA");

		gray = r.ReadBool("gray");
		flags = r.ReadUInt("flags");
		bytesPerPixelGray = r.ReadInt("bytesPerPixelGray");
		bytesPerPixelRGBA = r.ReadInt("bytesPerPixelRGBA");
		endianness = EndiannessFromString(r.ReadString("endianness"));
		bitsR = r.ReadInt("bitsR");
		bitsG = r.ReadInt("bitsG");
		bitsB = r.ReadInt("bitsB");
		bitsA = r.ReadInt("bitsA");
		bitsL = r.ReadInt("bitsL");
		shiftR = r.ReadInt("shiftR");
		shiftG = r.ReadInt("shiftG");
		shiftB = r.ReadInt("shiftB");
		shiftA = r.ReadInt("shiftA");
		shiftL = r.ReadInt("shiftL");

		r.EndDict();
	}
	void SaveSettings(ui::NamedTextSerializeWriter& w) override
	{
		w.BeginDict("formatSettingsRGBA");

		w.WriteBool("gray", gray);
		w.WriteInt("flags", flags);
		w.WriteInt("bytesPerPixelGray", bytesPerPixelGray);
		w.WriteInt("bytesPerPixelRGBA", bytesPerPixelRGBA);
		w.WriteString("endianness", EndiannessToString(endianness));
		w.WriteInt("bitsR", bitsR);
		w.WriteInt("bitsG", bitsG);
		w.WriteInt("bitsB", bitsB);
		w.WriteInt("bitsA", bitsA);
		w.WriteInt("bitsL", bitsL);
		w.WriteInt("shiftR", shiftR);
		w.WriteInt("shiftG", shiftG);
		w.WriteInt("shiftB", shiftB);
		w.WriteInt("shiftA", shiftA);
		w.WriteInt("shiftL", shiftL);

		w.EndDict();
	}

	void ConvertRowToRGBA(uint8_t* dst, uint8_t* src, uint32_t width)
	{
		static const constexpr uint32_t MAX_PIECE_SIZE = 16;

		uint8_t bpp = gray ? bytesPerPixelGray : bytesPerPixelRGBA;

		alignas(64) uint32_t pixel[MAX_PIECE_SIZE];
		for (uint32_t x = 0; x < width; x += MAX_PIECE_SIZE)
		{
			uint32_t curPieceSize = ui::min(width - x, MAX_PIECE_SIZE);
			switch (bpp)
			{
			case 1:
				for (uint32_t i = 0; i < curPieceSize; i++)
					pixel[i] = src[x + i];
				break;
			case 2:
				switch (endianness)
				{
				case Endianness::Little:
					for (uint32_t i = 0; i < curPieceSize; i++)
						pixel[i] = src[(x + i) * 2] | (src[(x + i) * 2 + 1] << 8);
					break;
				case Endianness::Big:
					for (uint32_t i = 0; i < curPieceSize; i++)
						pixel[i] = src[(x + i) * 2 + 1] | (src[(x + i) * 2] << 8);
					break;
				}
				break;
			case 3:
				switch (endianness)
				{
				case Endianness::Little:
					for (uint32_t i = 0; i < curPieceSize; i++)
					{
						pixel[i] =
							src[(x + i) * 3 + 0] | 
							(src[(x + i) * 3 + 1] << 8) |
							(src[(x + i) * 3 + 2] << 16);
					}
					break;
				case Endianness::Big:
					for (uint32_t i = 0; i < curPieceSize; i++)
					{
						pixel[i] =
							src[(x + i) * 3 + 2] |
							(src[(x + i) * 3 + 1] << 8) |
							(src[(x + i) * 3 + 0] << 16);
					}
					break;
				}
				break;
			case 4:
				switch (endianness)
				{
				case Endianness::Little:
					for (uint32_t i = 0; i < curPieceSize; i++)
					{
						pixel[i] =
							src[(x + i) * 4 + 0] |
							(src[(x + i) * 4 + 1] << 8) |
							(src[(x + i) * 4 + 2] << 16) |
							(src[(x + i) * 4 + 3] << 24);
					}
					break;
				case Endianness::Big:
					for (uint32_t i = 0; i < curPieceSize; i++)
					{
						pixel[i] =
							src[(x + i) * 4 + 3] |
							(src[(x + i) * 4 + 2] << 8) |
							(src[(x + i) * 4 + 1] << 16) |
							(src[(x + i) * 4 + 0] << 24);
					}
					break;
				}
				break;
			}

			uint8_t maskA = (1 << bitsA) - 1;
			bool hasA = bitsA != 0;

			if (gray)
			{
				uint8_t maskL = (1 << bitsL) - 1;

				alignas(64) uint8_t grays[MAX_PIECE_SIZE];
				alignas(64) uint8_t alphas[MAX_PIECE_SIZE];
				for (uint32_t i = 0; i < curPieceSize; i++)
				{
					grays[i] = (pixel[i] >> shiftL) & maskL;
					alphas[i] = hasA ? (pixel[i] >> shiftA) & maskA : 0xff;
				}
				ExpandBytes(grays, curPieceSize, bitsL);
				ExpandBytes(alphas, curPieceSize, bitsA);
				if (flags & BlackOnlyAlpha)
				{
					for (uint32_t i = 0; i < curPieceSize; i++)
						if (grays[i])
							alphas[i] = 255;
				}
				for (uint32_t i = 0; i < curPieceSize; i++)
				{
					dst[(x + i) * 4 + 0] = grays[i];
					dst[(x + i) * 4 + 1] = grays[i];
					dst[(x + i) * 4 + 2] = grays[i];
					dst[(x + i) * 4 + 3] = alphas[i];
				}
			}
			else
			{
				uint8_t maskR = (1 << bitsR) - 1;
				uint8_t maskG = (1 << bitsG) - 1;
				uint8_t maskB = (1 << bitsB) - 1;

				alignas(64) uint8_t reds[MAX_PIECE_SIZE];
				alignas(64) uint8_t grns[MAX_PIECE_SIZE];
				alignas(64) uint8_t blus[MAX_PIECE_SIZE];
				alignas(64) uint8_t alphas[MAX_PIECE_SIZE];
				for (uint32_t i = 0; i < curPieceSize; i++)
				{
					reds[i] = (pixel[i] >> shiftR) & maskR;
					grns[i] = (pixel[i] >> shiftG) & maskG;
					blus[i] = (pixel[i] >> shiftB) & maskB;
					alphas[i] = hasA ? (pixel[i] >> shiftA) & maskA : 0xff;
				}
				ExpandBytes(reds, curPieceSize, bitsR);
				ExpandBytes(grns, curPieceSize, bitsG);
				ExpandBytes(blus, curPieceSize, bitsB);
				ExpandBytes(alphas, curPieceSize, bitsA);
				if (flags & BlackOnlyAlpha)
				{
					for (uint32_t i = 0; i < curPieceSize; i++)
						if (reds[i] || grns[i] || blus[i])
							alphas[i] = 255;
				}
				for (uint32_t i = 0; i < curPieceSize; i++)
				{
					dst[(x + i) * 4 + 0] = reds[i];
					dst[(x + i) * 4 + 1] = grns[i];
					dst[(x + i) * 4 + 2] = blus[i];
					dst[(x + i) * 4 + 3] = alphas[i];
				}
			}
		}
	}
};

static void ReadImage_RGBA(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	auto* S = static_cast<RGBAImageFormatSettings*>(io.settings);

	uint8_t bpp = S->gray ? S->bytesPerPixelGray : S->bytesPerPixelRGBA;

	uint8_t tmp[4096 * 4];
	uint32_t srcw = bpp * info.width;
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + srcw * y, srcw, tmp);

		S->ConvertRowToRGBA(io.bytes + 4 * info.width * y, tmp, info.width);
	}
}


static void ReadImage_RGBA8(ReadImageIO& io, const ImageInfo& info)
{
	io.ds->Read(info.offImg, info.width * info.height * 4, io.bytes);
}

static void ReadImage_RGBX8(ReadImageIO& io, const ImageInfo& info)
{
	io.ds->Read(info.offImg, info.width * info.height * 4, io.bytes);
	for (uint32_t px = 0; px < info.width * info.height; px++)
	{
		io.bytes[px * 4 + 3] = 0xff;
	}
}

static void ReadImage_RGBo8(ReadImageIO& io, const ImageInfo& info)
{
	io.ds->Read(info.offImg, info.width * info.height * 4, io.bytes);
	for (uint32_t px = 0; px < info.width * info.height; px++)
	{
		io.bytes[px * 4 + 3] = io.bytes[px * 4 + 3] ? 0xff : 0;
	}
}

static void ReadImage_G8(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	uint8_t tmp[4096];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + y * info.width, info.width, tmp);
		for (uint32_t x = 0; x < info.width; x++)
			io.pixels[y * info.width + x] = tmp[x] | (tmp[x] << 8) | (tmp[x] << 16) | 0xff000000UL;
	}
}

static void ReadImage_G1(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	int w = divup(info.width, 8);
	uint8_t tmp[4096 / 8];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + y * w, w, tmp);
		for (uint32_t x = 0; x < info.width; x++)
		{
			bool set = (tmp[x / 8] & (1 << (x % 8))) != 0;
			io.pixels[y * info.width + x] = set ? 0xffffffffUL : 0xff000000UL;
		}
	}
}

static void ReadImage_8BPP_RGBA8(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	uint32_t pal[256];
	io.ds->Read(info.offPal, 256 * 4, pal);

	uint8_t line[4096];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + info.width * y, info.width, line);
		for (uint32_t x = 0; x < info.width; x++)
		{
			io.pixels[x + y * info.width] = pal[line[x]];
		}
	}
}

static void ReadImage_RGB5A1(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	uint16_t tmp[4096];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + y * info.width * 2, info.width * 2, tmp);
		for (uint32_t x = 0; x < info.width; x++)
			io.pixels[y * info.width + x] = RGB5A1_to_RGBA8(tmp[x]);
	}
}

static void ReadImage_4BPP_RGB5A1(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	uint16_t palo[16];
	io.ds->Read(info.offPal, 32, palo);

	uint32_t palc[16];
	for (int i = 0; i < 16; i++)
		palc[i] = RGB5A1_to_RGBA8(palo[i]);

	uint8_t line[2048];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + info.width / 2 * y, info.width / 2, line);
		for (uint32_t x = 0; x < info.width / 2; x++)
		{
			uint8_t v1 = line[x] & 0xf;
			uint8_t v2 = line[x] >> 4;

			uint32_t px = x * 2 + y * info.width;
			io.pixels[px] = palc[v1];
			px++;
			io.pixels[px] = palc[v2];
		}
	}
}

static void ReadImage_8BPP_RGB5A1(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	uint16_t palo[256];
	io.ds->Read(info.offPal, sizeof(palo), palo);

	uint32_t palc[256];
	for (int i = 0; i < 256; i++)
		palc[i] = RGB5A1_to_RGBA8(palo[i]);

	uint8_t line[4096];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + info.width * y, info.width, line);
		for (uint32_t x = 0; x < info.width; x++)
		{
			io.pixels[x + y * info.width] = palc[line[x]];
		}
	}
}

static void ReadImage_4BPP_RGBo8(ReadImageIO& io, const ImageInfo& info)
{
	if (info.width > 4096)
	{
		io.error = true;
		return;
	}

	uint32_t pal[16 * 4];
	io.ds->Read(info.offPal, 64, pal);
	for (uint32_t i = 0; i < 16; i++)
		if (pal[i] & 0xff000000)
			pal[i] |= 0xff000000;

	uint8_t line[2048];
	for (uint32_t y = 0; y < info.height; y++)
	{
		io.ds->Read(info.offImg + info.width / 2 * y, info.width / 2, line);
		for (uint32_t x = 0; x < info.width / 2; x++)
		{
			uint8_t v1 = line[x] & 0xf;
			uint8_t v2 = line[x] >> 4;

			uint32_t px = x * 2 + y * info.width;
			io.pixels[px] = pal[v1];
			px++;
			io.pixels[px] = pal[v2];
		}
	}
}

struct DXT1Block
{
	uint16_t c0, c1;
	uint32_t pixels;

	void GenerateColors(uint32_t c[4])
	{
		uint8_t r0 = (c[0] >> 0) & 0xff;
		uint8_t g0 = (c[0] >> 8) & 0xff;
		uint8_t b0 = (c[0] >> 16) & 0xff;
		uint8_t r1 = (c[1] >> 0) & 0xff;
		uint8_t g1 = (c[1] >> 8) & 0xff;
		uint8_t b1 = (c[1] >> 16) & 0xff;
		uint8_t r2 = r0 * 2 / 3 + r1 / 3;
		uint8_t g2 = g0 * 2 / 3 + g1 / 3;
		uint8_t b2 = b0 * 2 / 3 + b1 / 3;
		uint8_t r3 = r0 / 3 + r1 * 2 / 3;
		uint8_t g3 = g0 / 3 + g1 * 2 / 3;
		uint8_t b3 = b0 / 3 + b1 * 2 / 3;
		c[2] = r2 | (g2 << 8) | (b2 << 16) | 0xff000000UL;
		c[3] = r3 | (g3 << 8) | (b3 << 16) | 0xff000000UL;
	}
	uint32_t GetColor(int x, int y)
	{
		uint32_t c[4] = { RGB565_to_RGBA8(c0), RGB565_to_RGBA8(c1) };
		if (c0 > c1)
		{
			GenerateColors(c);
		}
		else
		{
			c[2] = (c[0] >> 1) + (c[1] >> 1);
			c[3] = 0;
		}

		int at = y * 4 + x;
		int idx = (pixels >> (at * 2)) & 0x3;
		return c[idx];
	}
	uint32_t GetColorDXT3(int x, int y, uint8_t alpha)
	{
		uint32_t c[4] = { RGB565_to_RGBA8(c0), RGB565_to_RGBA8(c1) };
		GenerateColors(c);

		int at = y * 4 + x;
		int idx = (pixels >> (at * 2)) & 0x3;
		auto cc = c[idx];
		return (cc & 0xffffff) | (uint32_t(alpha) << 24);
	}
};

static const uint8_t g_DXT3AlphaTable[16] =
{
	255 * 0 / 15,
	255 * 1 / 15,
	255 * 2 / 15,
	255 * 3 / 15,
	255 * 4 / 15,
	255 * 5 / 15,
	255 * 6 / 15,
	255 * 7 / 15,
	255 * 8 / 15,
	255 * 9 / 15,
	255 * 10 / 15,
	255 * 11 / 15,
	255 * 12 / 15,
	255 * 13 / 15,
	255 * 14 / 15,
	255 * 15 / 15,
};

struct DXT3AlphaBlock
{
	uint64_t pixels;

	uint8_t GetAlpha(int x, int y)
	{
		int at = y * 4 + x;
		int idx = (pixels >> (at * 4)) & 0xf;
		return g_DXT3AlphaTable[idx];
	}
};

static void ReadImage_DXT1(ReadImageIO& io, const ImageInfo& info)
{
	uint32_t nbx = divup(info.width, 4);
	uint32_t nby = divup(info.height, 4);

	uint64_t at = info.offImg;
	for (uint32_t by = 0; by < nby; by++)
	{
		for (uint32_t bx = 0; bx < nbx; bx++)
		{
			DXT1Block block;
			io.ds->Read(at, sizeof(block), &block);
			at += sizeof(block);

			for (uint32_t y = by * 4; y < std::min((by + 1) * 4, info.height); y++)
			{
				for (uint32_t x = bx * 4; x < std::min((bx + 1) * 4, info.width); x++)
				{
					io.pixels[x + info.width * y] = block.GetColor(x - bx * 4, y - by * 4);
				}
			}
		}
	}
}

static void ReadImage_DXT3(ReadImageIO& io, const ImageInfo& info)
{
	uint32_t nbx = divup(info.width, 4);
	uint32_t nby = divup(info.height, 4);

	uint64_t at = info.offImg;
	for (uint32_t by = 0; by < nby; by++)
	{
		for (uint32_t bx = 0; bx < nbx; bx++)
		{
			DXT1Block block;
			DXT3AlphaBlock ablock;
			io.ds->Read(at, sizeof(ablock), &ablock);
			at += sizeof(ablock);
			io.ds->Read(at, sizeof(block), &block);
			at += sizeof(block);

			for (uint32_t y = by * 4; y < std::min((by + 1) * 4, info.height); y++)
			{
				for (uint32_t x = bx * 4; x < std::min((bx + 1) * 4, info.width); x++)
				{
					uint8_t alpha = ablock.GetAlpha(x - bx * 4, y - by * 4);
					io.pixels[x + info.width * y] = block.GetColorDXT3(x - bx * 4, y - by * 4, alpha);
				}
			}
		}
	}
}


struct DummyImageFormatSettings : IImageFormatSettings
{
	void EditUI() override
	{
	}
	std::string ExportSettingsBlob() override
	{
		return {};
	}
	void LoadSettings(ui::NamedTextSerializeReader& r) override
	{
	}
	void SaveSettings(ui::NamedTextSerializeWriter& w) override
	{
	}
};


static const ImageFormat g_imageFormats[] =
{
	{ "Generic", "RGBA", ReadImage_RGBA, []() -> IImageFormatSettings* { return new RGBAImageFormatSettings; } },
	{ "Basic", "RGBA8", ReadImage_RGBA8 },
	{ "Basic", "RGBX8", ReadImage_RGBX8 },
	{ "Basic", "RGBo8", ReadImage_RGBo8 },
	{ "Basic", "G8", ReadImage_G8 },
	{ "Basic", "G1", ReadImage_G1 },
	{ "Basic", "8BPP_RGBA8", ReadImage_8BPP_RGBA8 },
	{ "PSX", "RGB5A1", ReadImage_RGB5A1 },
	{ "PSX", "4BPP_RGB5A1", ReadImage_4BPP_RGB5A1 },
	{ "PSX", "8BPP_RGB5A1", ReadImage_8BPP_RGB5A1 },
	{ "PSX", "4BPP_RGBo8", ReadImage_4BPP_RGBo8 },
	{ "S3TC", "DXT1", ReadImage_DXT1 },
	{ "S3TC", "DXT3", ReadImage_DXT3 },
};


size_t GetImageFormatCount()
{
	return sizeof(g_imageFormats) / sizeof(g_imageFormats[0]);
}

ui::StringView GetImageFormatCategory(size_t fid)
{
	return g_imageFormats[fid].category;
}

ui::StringView GetImageFormatName(size_t fid)
{
	return g_imageFormats[fid].name;
}

ImageFormatSettingsHandle CreateImageFormatSettings(ui::StringView fmt)
{
	for (const auto& fmtDesc : g_imageFormats)
	{
		if (fmtDesc.name == fmt)
		{
			if (!fmtDesc.cifsFunc)
				break;
			if (auto* ifs = fmtDesc.cifsFunc())
				return ifs;
		}
	}
	return new DummyImageFormatSettings;
}

static bool ReadImageToCanvas(ui::Canvas& c, IDataSource* ds, ReadImage* readFunc, IImageFormatSettings* settings, const ImageInfo& info)
{
	ReadImageIO io = { c, c.GetBytes(), c.GetPixels(), ds, settings, false };
	readFunc(io, info);
	if (io.error)
		return false;

	if (info.opaque)
	{
		auto* px = c.GetPixels();
		for (size_t i = 0, num = c.GetNumPixels(); i < num; i++)
		{
			px[i] |= 0xff000000;
		}
	}

	return true;
}

ui::draw::ImageHandle CreateImageFrom(IDataSource* ds, ui::StringView fmt, IImageFormatSettings* settings, const ImageInfo& info)
{
	ReadImage* readFunc = nullptr;
	for (auto& F : g_imageFormats)
	{
		if (F.name == fmt)
		{
			readFunc = F.readFunc;
			break;
		}
	}
	if (!readFunc)
		return nullptr;

	ui::Canvas c;
	c.SetSize(info.width, info.height);
	if (info.paletteMode == PaletteMode::None)
	{
		if (!ReadImageToCanvas(c, ds, readFunc, settings, info))
			return nullptr;
	}
	else if (info.paletteMode == PaletteMode::Index8)
	{
		ui::Canvas palc;
		ImageInfo palInfo = info;
		palInfo.offImg = palInfo.offPal;
		palInfo.width = 256;
		palInfo.height = 1;
		palc.SetSize(256, 1);
		if (!ReadImageToCanvas(palc, ds, readFunc, settings, palInfo))
			return nullptr;

		uint8_t line[4096];
		if (info.width > 4096)
			return nullptr;
		auto* pal = palc.GetPixels();
		auto* pixels = c.GetPixels();
		for (uint32_t y = 0; y < info.height; y++)
		{
			ds->Read(info.offImg + info.width * y, info.width, line);
			for (uint32_t x = 0; x < info.width; x++)
			{
				pixels[x] = pal[line[x]];
			}
			pixels += info.width;
		}
	}
	else if (info.paletteMode == PaletteMode::Low4High4)
	{
		ui::Canvas palc;
		ImageInfo palInfo = info;
		palInfo.offImg = palInfo.offPal;
		palInfo.width = 16;
		palInfo.height = 1;
		palc.SetSize(16, 1);
		if (!ReadImageToCanvas(palc, ds, readFunc, settings, palInfo))
			return nullptr;

		uint8_t line[2048];
		if (info.width > 4096)
			return nullptr;
		auto* pal = palc.GetPixels();
		auto* pixels = c.GetPixels();
		for (uint32_t y = 0; y < info.height; y++)
		{
			ds->Read(info.offImg + info.width / 2 * y, info.width / 2, line);
			for (uint32_t x = 0; x < info.width / 2; x++)
			{
				uint8_t v1 = line[x] & 0xf;
				uint8_t v2 = line[x] >> 4;

				uint32_t px = x * 2;
				pixels[px] = pal[v1];
				px++;
				pixels[px] = pal[v2];
			}
			pixels += info.width;
		}
	}
	else return nullptr;

	return ui::draw::ImageCreateFromCanvas(c, ui::draw::TexFlags::Repeat | ui::draw::TexFlags::NoFilter);
}
