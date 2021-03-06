
#pragma once
#include "pch.h"


struct ConnectOffset
{
	uint64_t off;
	ui::Point2f tablePos;
	bool sel;
};

struct TableWithOffsets
{
	int GetOffsets(int max, ConnectOffset* buf)
	{
		if (!curTable)
			return 0;


		auto* ds = curTable->GetDataSource();
		size_t col = 0, nc = ds->GetNumCols();
		for (col = 0; col < nc; col++)
		{
			if (ds->GetColName(col) == "Offset")
				break;
		}
		if (col == nc)
			return 0;

		int ret = 0;

		auto hoverRow = curTable->GetHoverRow();
		TryAddRow(hoverRow, col, ret, max, buf);

		if (curTable->GetSelectionStorage())
		{
			auto range = curTable->GetVisibleRange();
			for (auto row = range.min; row < range.max; row++)
			{
				if (curTable->GetSelectionStorage()->GetSelectionState(row))
					TryAddRow(row, col, ret, max, buf);
			}
		}

		return ret;
	}

	void TryAddRow(size_t row, size_t col, int& ret, int max, ConnectOffset* buf)
	{
		if (!curTable->IsValidRow(row))
			return;

		auto* ds = curTable->GetDataSource();
		auto* ss = curTable->GetSelectionStorage();
		auto tcr = curTable->GetContentRect();

		auto offtext = ds->GetText(row, col);
		auto off = std::stoull(offtext);
		auto cr = curTable->GetCellRect(SIZE_MAX, row);
		float y = (cr.y0 + cr.y1) * 0.5f;
		if (tcr.Contains(cr.x0, y))
			buf[ret++] = { off, { cr.x0, y }, ss ? ss->GetSelectionState(row) : false };
	}

	ui::TableView* curTable = nullptr;
};
