
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
		ui::StringView buf(buffer.data(), nread);
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
