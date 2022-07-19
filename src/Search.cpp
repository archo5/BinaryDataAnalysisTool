
#include "pch.h"
#include "Search.h"


void FragmentSearch::PerformSearch(IDataSource* ds)
{
	ui::StringView frag = textFragment;
	matchWidth = frag.size();
	results.clear();
	resultSource = ds;

	if (frag.empty())
		return;

	uint64_t size = ds->GetSize();
	if (frag.size() > size)
		return;

	// preferred size
	size_t bufSize = size > SIZE_MAX ? SIZE_MAX : size;
	if (bufSize > 65536)
		bufSize = 65536; // don't read the entire file
	if (bufSize < frag.size() * 2)
		bufSize = frag.size() * 2; // absolute minimum

	std::string buffer;
	buffer.resize(frag.size() * 100);

	if (buffer.size() >= size)
	{
		ds->Read(0, size, &buffer[0]);

		ui::StringView buf(buffer.data(), size);
		size_t off = 0;
		for (;;)
		{
			size_t loc = buf.find_first_at(frag, off);
			if (loc == SIZE_MAX)
				break;
			results.push_back(loc);
			off = loc + 1;
		}
		return;
	}

	uint64_t fileoff = 0, readoff = 0;
	size_t nread = ds->Read(0, buffer.size(), &buffer[0]);
	size_t bufsize = nread;
	while (nread)
	{
		ui::StringView buf(buffer.data(), bufsize);
		size_t off = 0;
		for (;;)
		{
			size_t loc = buf.find_first_at(frag, off);
			if (loc == SIZE_MAX)
				break;
			results.push_back(fileoff + loc);
			off = loc + 1;
		}

		if (frag.size() > 1)
		{
			// move the unprocessed part to the beginning
			size_t rem = frag.size() - 1;
			memmove(&buffer[0], &buffer[nread - rem], rem);
			readoff += nread;
			fileoff = readoff - rem;
			nread = ds->Read(readoff, buffer.size() - rem, &buffer[rem]);
			bufsize = rem + nread;
		}
		else
		{
			fileoff += nread;
			nread = ds->Read(fileoff, buffer.size(), &buffer[0]);
		}
	}
}

void FragmentSearch::SearchUI(IDataSource* ds)
{
	ui::Push<ui::StackExpandLTRLayoutElement>();
	ui::imm::PropEditString("\bText", textFragment.c_str(), [this](const char* v) { textFragment = v; });
	auto tmpl = ui::StackExpandLTRLayoutElement::GetSlotTemplate();
	tmpl->DisableScaling();
	if (ui::imm::Button("Search"))
	{
		PerformSearch(ds);
	}
	ui::Pop();

	ui::Push<ui::StackExpandLTRLayoutElement>();
	ui::MakeWithText<ui::LabelFrame>("Context bytes");
	auto range = ui::Range<uint32_t>::AtMost(256);
	ui::imm::PropEditInt("\bBefore", bytesBeforeMatch, {}, {}, range);
	ui::imm::PropEditInt("\bAfter", bytesAfterMatch, {}, {}, range);
	ui::Pop();
}

enum FS_Cols
{
	FS_COL_Offset,
	FS_COL_Match,

	FS_COL__COUNT,
};

size_t FragmentSearch::GetNumCols()
{
	return FS_COL__COUNT;
}

std::string FragmentSearch::GetColName(size_t col)
{
	switch (col)
	{
	case FS_COL_Offset: return "Offset";
	case FS_COL_Match: return "Match";
	default: return "???";
	}
}

std::string FragmentSearch::GetText(uintptr_t id, size_t col)
{
	switch (col)
	{
	case FS_COL_Offset: return std::to_string(results[id]);
	case FS_COL_Match:
	{
		std::string tmp;
		size_t size = bytesBeforeMatch + matchWidth + bytesAfterMatch;
		tmp.resize(size, ' ');
		uint64_t pos = results[id];
		size_t off = 0;
		if (pos >= bytesBeforeMatch)
			pos -= bytesBeforeMatch;
		else
		{
			off = bytesBeforeMatch - pos;
			size = results[id] + matchWidth + bytesAfterMatch - pos;
			pos = 0;
		}
		resultSource->GetASCIIText(&tmp[off], size, pos, ' ');
		return tmp;
	}
	default: return "???";
	}
}

size_t FragmentSearch::GetNumRows()
{
	return results.size();
}

std::string FragmentSearch::GetRowName(size_t row)
{
	return std::to_string(row + 1);
}



struct CSString : std::string
{
	void add(const std::string& s)
	{
		if (!empty())
			*this += ", ";
		*this += s;
	}
};

struct FileInfo
{
	CSString desc;
	CSString misc;
	CSString err;
	uint64_t size = 0;
};

typedef bool FileFormatCheckFunc(char* bytes);
typedef void FileFormatDescFunc(IDataSource* ds, uint64_t off, FileInfo& fi);
struct FileFormatInfo
{
	const char* name;
	const char* ext;
	size_t prefixBytes;
	FileFormatCheckFunc* checkFunc;
	FileFormatDescFunc* descFunc;
};

static void FileFormatDescFunc_DDS(IDataSource* ds, uint64_t off, FileInfo& fi)
{
	using u = unsigned;
	using DWORD = uint32_t;
	struct DDS_PIXELFORMAT
	{
		DWORD dwSize;
		DWORD dwFlags;
		DWORD dwFourCC;
		DWORD dwRGBBitCount;
		DWORD dwRBitMask;
		DWORD dwGBitMask;
		DWORD dwBBitMask;
		DWORD dwABitMask;
	};
	struct DDS_HEADER
	{
		DWORD           dwSize;
		DWORD           dwFlags;
		DWORD           dwHeight;
		DWORD           dwWidth;
		DWORD           dwPitchOrLinearSize;
		DWORD           dwDepth;
		DWORD           dwMipMapCount;
		DWORD           dwReserved1[11];
		DDS_PIXELFORMAT ddspf;
		DWORD           dwCaps;
		DWORD           dwCaps2;
		DWORD           dwCaps3;
		DWORD           dwCaps4;
		DWORD           dwReserved2;
	};
	using DXGI_FORMAT = uint32_t;
	using D3D10_RESOURCE_DIMENSION = uint32_t;
	using UINT = uint32_t;
	struct DDS_HEADER_DXT10
	{
		DXGI_FORMAT              dxgiFormat;
		D3D10_RESOURCE_DIMENSION resourceDimension;
		UINT                     miscFlag;
		UINT                     arraySize;
		UINT                     miscFlags2;
	};

	DDS_HEADER h;
	ds->Read(off + 4, sizeof(h), &h);

	char fourcc[4];
	memcpy(&fourcc, &h.ddspf.dwFourCC, 4);

	fi.desc.add(ui::Format("DDS %ux%ux%u (p/ls=%u, mips=%u) fourcc=%c%c%c%c",
		u(h.dwWidth), u(h.dwHeight), u(h.dwDepth),
		u(h.dwPitchOrLinearSize), u(h.dwMipMapCount),
		fourcc[0], fourcc[1], fourcc[2], fourcc[3]));

	if (h.dwSize != 124)
		fi.err.add(ui::Format("bad header size: %u (need 124)", u(h.dwSize)));
}

#define CHECK4(b, c0, c1, c2, c3) (b[0] == c0 && b[1] == c1 && b[2] == c2 && b[3] == c3)
FileFormatInfo g_formats[] =
{
	{ "DDS", ".dds", 4, [](char* b) { return CHECK4(b, 'D', 'D', 'S', ' '); }, FileFormatDescFunc_DDS },
};

enum FFS_Cols
{
	FFS_COL_Offset,
	FFS_COL_Size,
	FFS_COL_Desc,

	FFS_COL__COUNT,
};

void FileFormatSearch::PerformSearch(IDataSource* ds)
{
	results.clear();
	resultSource = ds;
	size_t minSize = SIZE_MAX;
	size_t maxSize = 0;
	for (auto& fmt : g_formats)
	{
		minSize = ui::min(minSize, fmt.prefixBytes);
		maxSize = ui::max(maxSize, fmt.prefixBytes);
	}
	// there shouldn't be a single format that is defined by only a single byte
	assert(maxSize != 1);

	if (maxSize == 0)
		return;

	uint64_t size = ds->GetSize();
	if (minSize > size)
		return;

	// preferred size
	size_t bufSize = size > SIZE_MAX ? SIZE_MAX : size;
	if (bufSize > 65536)
		bufSize = 65536; // don't read the entire file
	if (bufSize < maxSize * 2)
		bufSize = maxSize * 2; // absolute minimum

	std::string buffer;
	buffer.resize(maxSize * 100);

	if (buffer.size() >= size)
	{
		ds->Read(0, size, &buffer[0]);

		for (size_t i = 0; i < buffer.size(); i++)
		{
			for (auto& fmt : g_formats)
			{
				if (i + fmt.prefixBytes <= buffer.size() && fmt.checkFunc(&buffer[i]))
				{
					FileInfo fi;
					fmt.descFunc(ds, i, fi);
					Result r;
					r.offset = i;
					r.size = fi.size;
					r.format = &fmt - g_formats;
					r.desc = std::move(fi.desc);
					results.push_back(r);
				}
			}
		}
		return;
	}

	bool first = true;
	uint64_t fileoff = 0, readoff = 0;
	size_t nread = ds->Read(0, buffer.size(), &buffer[0]);
	size_t bufsize = nread;
	while (nread)
	{
		for (size_t i = 0; i < buffer.size(); i++)
		{
			for (auto& fmt : g_formats)
			{
				if ((first || i >= maxSize - fmt.prefixBytes) &&
					i + fmt.prefixBytes <= bufsize && fmt.checkFunc(&buffer[i]))
				{
					FileInfo fi;
					fmt.descFunc(ds, fileoff + i, fi);
					Result r;
					r.offset = fileoff + i;
					r.size = fi.size;
					r.format = &fmt - g_formats;
					r.desc = std::move(fi.desc);
					results.push_back(r);
				}
			}
		}

		if (maxSize > 1)
		{
			// move the unprocessed part to the beginning
			size_t rem = maxSize - 1;
			memmove(&buffer[0], &buffer[nread - rem], rem);
			readoff += nread;
			fileoff = readoff - rem;
			nread = ds->Read(readoff, buffer.size() - rem, &buffer[rem]);
			bufsize = rem + nread;
		}
		else
		{
			fileoff += nread;
			nread = ds->Read(fileoff, buffer.size(), &buffer[0]);
		}
		first = false;
	}
}

void FileFormatSearch::SearchUI(IDataSource* ds)
{
	if (ui::imm::Button("Search"))
	{
		PerformSearch(ds);
	}
}

size_t FileFormatSearch::GetNumCols()
{
	return FFS_COL__COUNT;
}

std::string FileFormatSearch::GetColName(size_t col)
{
	switch (col)
	{
	case FFS_COL_Offset: return "Offset";
	case FFS_COL_Size: return "Size";
	case FFS_COL_Desc: return "Description";
	default: return "???";
	}
}

std::string FileFormatSearch::GetText(uintptr_t id, size_t col)
{
	switch (col)
	{
	case FFS_COL_Offset: return std::to_string(results[id].offset);
	case FFS_COL_Size: return std::to_string(results[id].size);
	case FFS_COL_Desc: return results[id].desc;
	default: return "???";
	}
}

size_t FileFormatSearch::GetNumRows()
{
	return results.size();
}

std::string FileFormatSearch::GetRowName(size_t row)
{
	return std::to_string(row + 1);
}
