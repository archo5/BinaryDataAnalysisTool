
#include "pch.h"
#include "HexViewer.h"


uint64_t HexViewerState::GetInspectPos()
{
	if (selectionStart != UINT64_MAX)
		return ui::min(selectionStart, selectionEnd);
	return hoverByte;
}

void HexViewerState::GoToPos(int64_t pos)
{
	basePos = std::max(0LL, pos - byteWidth);
	selectionStart = std::max(0LL, pos);
	selectionEnd = std::max(0LL, pos);
	OnHexViewerStateChanged.Call(this);
}

ui::MulticastDelegate<const HexViewerState*> OnHexViewerStateChanged;
ui::MulticastDelegate<const HexViewerState*> OnHexViewerInspectTargetChanged;


void HighlightSettings::AddCustomInt32(int32_t v)
{
	Int32Highlight h;
	h.range = false;
	h.vspec = v;
	customInt32.push_back(h);
}

void HighlightSettings::Load(const char* key, NamedTextSerializeReader& r)
{
	r.BeginDict(key);

	excludeZeroes = r.ReadBool("excludeZeroes");
	enableFloat32 = r.ReadBool("enableFloat32");
	minFloat32 = r.ReadFloat("minFloat32");
	maxFloat32 = r.ReadFloat("maxFloat32");
	enableInt16 = r.ReadBool("enableInt16");
	minInt16 = r.ReadInt("minInt16");
	maxInt16 = r.ReadInt("maxInt16");
	enableInt32 = r.ReadBool("enableInt32");
	minInt32 = r.ReadInt("minInt32");
	maxInt32 = r.ReadInt("maxInt32");
	enableASCII = r.ReadBool("enableASCII", true);
	minASCIIChars = r.ReadUInt("minASCIIChars");
	enableNearFileSize32 = r.ReadBool("enableNearFileSize32", true);
	enableNearFileSize64 = r.ReadBool("enableNearFileSize64", true);
	nearFileSizePercent = r.ReadFloat("nearFileSizePercent", 99.0f);

	r.BeginArray("customInt32");
	for (auto* e : r.GetCurrentRange())
	{
		r.BeginEntry(e);
		r.BeginDict("");

		Int32Highlight h;
		h.color.r = r.ReadUInt("color.r");
		h.color.g = r.ReadUInt("color.g");
		h.color.b = r.ReadUInt("color.b");
		h.color.a = r.ReadUInt("color.a");
		h.vmin = r.ReadInt("vmin");
		h.vmax = r.ReadInt("vmax");
		h.vspec = r.ReadInt("vspec");
		h.enabled = r.ReadBool("enabled");
		h.range = r.ReadBool("range");
		customInt32.push_back(h);

		r.EndDict();
		r.EndEntry();
	}
	r.EndArray();

	r.EndDict();
}

void HighlightSettings::Save(const char* key, NamedTextSerializeWriter& w)
{
	w.BeginDict(key);
	
	w.WriteBool("excludeZeroes", excludeZeroes);
	w.WriteBool("enableFloat32", enableFloat32);
	w.WriteFloat("minFloat32", minFloat32);
	w.WriteFloat("maxFloat32", maxFloat32);
	w.WriteBool("enableInt16", enableInt16);
	w.WriteInt("minInt16", minInt16);
	w.WriteInt("maxInt16", maxInt16);
	w.WriteBool("enableInt32", enableInt32);
	w.WriteInt("minInt32", minInt32);
	w.WriteInt("maxInt32", maxInt32);
	w.WriteBool("enableASCII", enableASCII);
	w.WriteInt("minASCIIChars", minASCIIChars);
	w.WriteBool("enableNearFileSize32", enableNearFileSize32);
	w.WriteBool("enableNearFileSize64", enableNearFileSize64);
	w.WriteFloat("nearFileSizePercent", nearFileSizePercent);

	w.BeginArray("customInt32");
	for (auto& h : customInt32)
	{
		w.BeginDict("");
		w.WriteInt("color.r", h.color.r);
		w.WriteInt("color.g", h.color.g);
		w.WriteInt("color.b", h.color.b);
		w.WriteInt("color.a", h.color.a);
		w.WriteInt("vmin", h.vmin);
		w.WriteInt("vmax", h.vmax);
		w.WriteInt("vspec", h.vspec);
		w.WriteBool("enabled", h.enabled);
		w.WriteBool("range", h.range);
		w.EndDict();
	}
	w.EndArray();

	w.EndDict();
}

void HighlightSettings::EditUI()
{
	ui::imm::PropEditBool("Exclude zeroes", excludeZeroes, { ui::AddLabelTooltip("Typically enabled since every type can represent 0") });

	ui::LabeledProperty::Begin("float32");
	ui::imm::PropEditBool(nullptr, enableFloat32);
	ui::imm::PropEditFloat("\bMin", minFloat32, {}, 0.01f);
	ui::imm::PropEditFloat("\bMax", maxFloat32);
	ui::LabeledProperty::End();

	ui::LabeledProperty::Begin("int16");
	ui::imm::PropEditBool(nullptr, enableInt16);
	ui::imm::PropEditInt("\bMin", minInt16);
	ui::imm::PropEditInt("\bMax", maxInt16);
	ui::LabeledProperty::End();

	ui::LabeledProperty::Begin("int32");
	ui::imm::PropEditBool(nullptr, enableInt32);
	ui::imm::PropEditInt("\bMin", minInt32);
	ui::imm::PropEditInt("\bMax", maxInt32);
	ui::LabeledProperty::End();

	ui::LabeledProperty::Begin("ASCII");
	ui::imm::PropEditBool(nullptr, enableASCII);
	ui::imm::PropEditInt("\bMin chars", minASCIIChars, {}, 1, { 1, 128 });
	ui::LabeledProperty::End();

	ui::LabeledProperty::Begin("Near file size");
	ui::imm::PropEditBool("\bu32", enableNearFileSize32);
	ui::imm::PropEditBool("\bu64", enableNearFileSize64);
	ui::imm::PropEditFloat("\bPercent", nearFileSizePercent, {}, 0.1f, { 0, 100 });
	ui::LabeledProperty::End();

	ui::Push<ui::PaddingElement>().SetPaddingTop(20);
	ui::MakeWithText<ui::LabelFrame>("Custom int32");
	ui::Pop();

	auto& seqEd = ui::Make<ui::SequenceEditor>();
	seqEd.SetSequence(ui::BuildAlloc<ui::StdSequence<decltype(customInt32)>>(customInt32));
	seqEd.itemUICallback = [this](ui::SequenceEditor* se, size_t idx, void* ptr)
	{
		auto& h = *static_cast<Int32Highlight*>(ptr);
		ui::imm::EditBool(h.enabled, nullptr);
		ui::imm::EditColor(h.color);
		if (h.range)
			ui::imm::PropEditInt("\bMin", h.vmin);
		else
			ui::imm::PropEditInt("\bValue", h.vspec);
		ui::imm::EditBool(h.range, nullptr);
		if (h.range)
			ui::imm::PropEditInt("\bMax", h.vmax);
	};
	if (ui::imm::Button("Add"))
	{
		customInt32.push_back({});
	}
}


void FoundHighlightList::SortByOffset()
{
	std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.pos < b.pos; });
}

enum FHL_Cols
{
	FHL_COL_Pos,
	FHL_COL_HLType,
	FHL_COL_DataType,
	FHL_COL_Value,

	FHL_COL__COUNT,
};

size_t FoundHighlightList::GetNumCols()
{
	return FHL_COL__COUNT;
}

std::string FoundHighlightList::GetColName(size_t col)
{
	switch (col)
	{
	case FHL_COL_Pos: return "Offset";
	case FHL_COL_HLType: return "Highlight type";
	case FHL_COL_DataType: return "Data type";
	case FHL_COL_Value: return "Value";
	default: return "???";
	}
}

std::string FoundHighlightList::GetText(uintptr_t id, size_t col)
{
	switch (col)
	{
	case FHL_COL_Pos: return std::to_string(items[id].pos);
	case FHL_COL_HLType:
		switch (items[id].hlType)
		{
		case HighlightType::ValueInRange: return "Value in range";
		case HighlightType::NearFileSize: return "Near file size";
		case HighlightType::CustomHighlight: return "Custom highlight";
		default: return "???";
		}
	case FHL_COL_DataType: return GetDataTypeName(items[id].dataType);
	case FHL_COL_Value:
	{
		switch (items[id].dataType)
		{
		case DT_I8:
		case DT_I16:
		case DT_I32:
		case DT_I64: return std::to_string(items[id].ival);
		case DT_U8:
		case DT_U16:
		case DT_U32:
		case DT_U64: return std::to_string(uint64_t(items[id].uval));
		case DT_F32:
		case DT_F64: return std::to_string(reinterpret_cast<double&>(items[id].dval));
		default: return "???";
		}
	}
	default: return "???";
	}
}

size_t FoundHighlightList::GetNumRows()
{
	return items.size();
}

std::string FoundHighlightList::GetRowName(size_t row)
{
	return std::to_string(row + 1);
}


static bool IsASCII(uint8_t v)
{
	return v >= 0x20 && v < 0x7f;
}

static void Highlight(HighlightSettings* hs, HexViewerState* hvs, DataDesc* desc, DDFile* file, uint64_t basePos, Endianness endianness, ByteColors* outColors, uint8_t* bytes, size_t numBytes)
{
	hvs->highlightList.items.clear();

	for (auto& M : file->markerData.markers)
	{
		uint64_t start = M.at;
		uint64_t end = M.GetEnd();
		size_t ss = start > basePos ? start - basePos : 0;
		size_t se = end > basePos ? end - basePos : 0;
		if (ss > numBytes)
			ss = numBytes;
		if (se > numBytes)
			se = numBytes;
		for (size_t i = ss; i < se; i++)
		{
			ui::Color4f mc;
			if (unsigned flags = M.ContainInfo(basePos + i, &mc))
			{
				auto& oc = outColors[i];
				oc.asciiColor.BlendOver(mc);
				oc.hexColor.BlendOver(mc);
				ui::Color4f mc2 = { sqrtf(mc.r), sqrtf(mc.g), sqrtf(mc.b), sqrtf(mc.a) };
				if (flags & 2)
					oc.leftBracketColor.BlendOver(mc2);
				if (flags & 4)
					oc.rightBracketColor.BlendOver(mc2);
			}
		}
	}

	for (auto* SI : desc->instances)
	{
		if (SI->file != file)
			continue;
		if (SI->off >= int64_t(basePos) && SI->off < int64_t(basePos + numBytes))
		{
			outColors[SI->off - basePos].leftBracketColor.BlendOver(SI == desc->curInst ? colorCurInst : colorInst);
		}
	}

	// auto highlights
	if (hs->enableNearFileSize64)
	{
		for (size_t i = 0; i + 8 < numBytes; i++)
		{
			if (outColors[i].hexColor.a ||
				outColors[i + 1].hexColor.a ||
				outColors[i + 2].hexColor.a ||
				outColors[i + 3].hexColor.a ||
				outColors[i + 4].hexColor.a ||
				outColors[i + 5].hexColor.a ||
				outColors[i + 6].hexColor.a ||
				outColors[i + 7].hexColor.a)
				continue;
			if (hs->excludeZeroes &&
				bytes[i] == 0 &&
				bytes[i + 1] == 0 &&
				bytes[i + 2] == 0 &&
				bytes[i + 3] == 0 &&
				bytes[i + 4] == 0 &&
				bytes[i + 5] == 0 &&
				bytes[i + 6] == 0 &&
				bytes[i + 7] == 0)
				continue;

			if (hs->enableNearFileSize64)
			{
				uint64_t v;
				memcpy(&v, &bytes[i], 8);
				EndiannessAdjust(v, endianness);
				auto fsz = file->dataSource->GetSize();
				if (v >= fsz * (hs->nearFileSizePercent * 0.01f) && v <= fsz)
				{
					for (int j = 0; j < 8; j++)
						outColors[i + j].hexColor.BlendOver(colorNearFileSize32);

					FoundHighlightList::Item item = { basePos + i, DT_U64, HighlightType::NearFileSize };
					item.uval = v;
					hvs->highlightList.items.push_back(item);
				}
			}
		}
	}

	if (!hs->customInt32.empty())
	{
		for (size_t i = 0; i + 4 < numBytes; i++)
		{
			if (outColors[i].hexColor.a ||
				outColors[i + 1].hexColor.a ||
				outColors[i + 2].hexColor.a ||
				outColors[i + 3].hexColor.a)
				continue;
			if (hs->excludeZeroes && bytes[i] == 0 && bytes[i + 1] == 0 && bytes[i + 2] == 0 && bytes[i + 3] == 0)
				continue;

			int32_t i32v;
			memcpy(&i32v, &bytes[i], 4);
			EndiannessAdjust(i32v, endianness);

			for (const auto& h : hs->customInt32)
			{
				if (!h.enabled)
					continue;
				if (!h.range ? h.vspec == i32v :
					h.vmin <= i32v && i32v <= h.vmax)
				{
					for (int j = 0; j < 4; j++)
						outColors[i + j].hexColor.BlendOver(h.color);

					FoundHighlightList::Item item = { basePos + i, DT_I32, HighlightType::CustomHighlight };
					item.ival = i32v;
					hvs->highlightList.items.push_back(item);
					break;
				}
			}
		}
	}

	if (hs->enableFloat32 || hs->enableInt32 || hs->enableNearFileSize32)
	{
		for (size_t i = 0; i + 4 < numBytes; i++)
		{
			if (outColors[i].hexColor.a ||
				outColors[i + 1].hexColor.a ||
				outColors[i + 2].hexColor.a ||
				outColors[i + 3].hexColor.a)
				continue;
			if (hs->excludeZeroes && bytes[i] == 0 && bytes[i + 1] == 0 && bytes[i + 2] == 0 && bytes[i + 3] == 0)
				continue;

			int32_t i32v;
			memcpy(&i32v, &bytes[i], 4);
			EndiannessAdjust(i32v, endianness);

			if (hs->enableFloat32)
			{
				float v;
				memcpy(&v, &bytes[i], 4);
				EndiannessAdjust(v, endianness);
				if ((v >= hs->minFloat32 && v <= hs->maxFloat32) || (v >= -hs->maxFloat32 && v <= -hs->minFloat32))
				{
					for (int j = 0; j < 4; j++)
						outColors[i + j].hexColor.BlendOver(colorFloat32);

					FoundHighlightList::Item item = { basePos + i, DT_F32, HighlightType::ValueInRange };
					item.dval = v;
					hvs->highlightList.items.push_back(item);
				}
			}

			if (hs->enableNearFileSize32)
			{
				uint32_t v;
				memcpy(&v, &bytes[i], 4);
				EndiannessAdjust(v, endianness);
				auto fsz = file->dataSource->GetSize();
				if (v >= fsz * (hs->nearFileSizePercent * 0.01f) && v <= fsz)
				{
					for (int j = 0; j < 4; j++)
						outColors[i + j].hexColor.BlendOver(colorNearFileSize32);

					FoundHighlightList::Item item = { basePos + i, DT_U32, HighlightType::NearFileSize };
					item.uval = v;
					hvs->highlightList.items.push_back(item);
				}
			}

			if (hs->enableInt32)
			{
				if (i32v >= hs->minInt32 && i32v <= hs->maxInt32)
				{
					for (int j = 0; j < 4; j++)
						outColors[i + j].hexColor.BlendOver(colorInt32);

					FoundHighlightList::Item item = { basePos + i, DT_I32, HighlightType::ValueInRange };
					item.ival = i32v;
					hvs->highlightList.items.push_back(item);
				}
			}
		}
	}

	if (hs->enableInt16)
	{
		for (size_t i = 0; i + 2 < numBytes; i++)
		{
			if (outColors[i].hexColor.a ||
				outColors[i + 1].hexColor.a)
				continue;
			if (hs->excludeZeroes && bytes[i] == 0 && bytes[i + 1] == 0)
				continue;

			int16_t v;
			memcpy(&v, &bytes[i], 2);
			EndiannessAdjust(v, endianness);
			if (v >= hs->minInt16 && v <= hs->maxInt16)
			{
				outColors[i].hexColor.BlendOver(colorInt32);
				outColors[i + 1].hexColor.BlendOver(colorInt32);

				FoundHighlightList::Item item = { basePos + i, DT_I16, HighlightType::ValueInRange };
				item.ival = v;
				hvs->highlightList.items.push_back(item);
			}
		}
	}

	if (hs->enableASCII && hs->minASCIIChars > 0)
	{
		size_t start = SIZE_MAX;
		bool prev = false;
		for (size_t i = 0; i < numBytes; i++)
		{
			bool cur = outColors[i].asciiColor.a == 0 && IsASCII(bytes[i]);
			if (cur && !prev)
				start = i;
			else if (prev && !cur && i - start >= hs->minASCIIChars)
			{
				for (size_t j = start; j < i; j++)
					outColors[j].asciiColor.BlendOver(colorASCII);
			}
			prev = cur;
		}
		if (prev && numBytes - start >= hs->minASCIIChars)
		{
			for (size_t j = start; j < numBytes; j++)
				outColors[j].asciiColor.BlendOver(colorASCII);
		}
	}

	hvs->highlightList.SortByOffset();
}


static void MoveSelection(HexViewer* hv, ui::Event& e, int64_t pos)
{
	if (pos < 0)
		pos = 0;
	int64_t size = hv->file->dataSource->GetSize();
	if (pos > size)
		pos = size;
	hv->state->selectionEnd = pos;
	if (!e.GetKeyActionModifier())
		hv->state->selectionStart = pos;

	auto w = hv->state->byteWidth;
	size_t ph = ui::max(size_t(floor(hv->GetFinalRect().GetHeight() / (hv->contentFont.size + 4)) - 2), size_t(0));

	if (hv->state->basePos > ui::max(pos - w, 0LL))
		hv->state->basePos = ui::max((pos - w) / w * w, 0LL);
	if (hv->state->basePos < ui::max(pos - ph * w, 0LL))
		hv->state->basePos = ui::max((pos / w - ph) * w, 0LL);

	OnHexViewerStateChanged.Call(hv->state);
	OnHexViewerInspectTargetChanged.Call(hv->state);
}

void HexViewer::OnEvent(ui::Event& e)
{
	int W = state->byteWidth;

	if (e.type == ui::EventType::ButtonDown)
	{
		if (e.GetButton() == ui::MouseButton::Left)
		{
			state->mouseDown = true;
			state->selectionStart = state->selectionEnd = state->hoverByte;
			OnHexViewerStateChanged.Call(state);
			OnHexViewerInspectTargetChanged.Call(state);
		}
	}
	else if (e.type == ui::EventType::ButtonUp)
	{
		if (e.GetButton() == ui::MouseButton::Left)
		{
			state->mouseDown = false;
			//OnHexViewerStateChanged.Call(state);
			//OnHexViewerInspectTargetChanged.Call(state);
		}
	}
	else if (e.type == ui::EventType::MouseMove)
	{
		ui::Font* font = contentFont.GetFont();

		float fh = contentFont.size + 4;
		float x = GetFinalRect().x0 + 2 + ui::GetTextWidth(font, contentFont.size, "0") * 8;
		float y = GetFinalRect().y0 + fh;
		float x2 = x + 20 * W + 10;

		uint64_t initialInspectPos = state->GetInspectPos();
		uint64_t initialSelEnd = state->selectionEnd;

		state->hoverSection = -1;
		state->hoverByte = UINT64_MAX;
		if (e.position.y >= y && e.position.x >= x && e.position.x < x + W * 20)
		{
			state->hoverSection = 0;
			int xpos = ui::min(ui::max(0, int((e.position.x - x) / 20)), W - 1);
			int ypos = (e.position.y - y) / fh;
			state->hoverByte = GetBasePos() + xpos + ypos * W;
		}
		else if (e.position.y >= y && e.position.x >= x2 && e.position.x < x2 + W * 10)
		{
			state->hoverSection = 1;
			int xpos = ui::min(ui::max(0, int((e.position.x - x2) / 10)), W - 1);
			int ypos = (e.position.y - y) / fh;
			state->hoverByte = GetBasePos() + xpos + ypos * W;
		}
		if (state->mouseDown)
		{
			state->selectionEnd = state->hoverByte;
		}

		if (initialSelEnd != state->selectionEnd)
			OnHexViewerStateChanged.Call(state);

		auto finalInspectPos = state->GetInspectPos();
		if (initialInspectPos != state->GetInspectPos())
			OnHexViewerInspectTargetChanged.Call(state);
	}
	else if (e.type == ui::EventType::MouseLeave)
	{
		state->hoverSection = -1;
		state->hoverByte = UINT64_MAX;
		//OnHexViewerStateChanged.Call(state);
		if (state->selectionEnd == UINT64_MAX)
			OnHexViewerInspectTargetChanged.Call(state);
	}
	else if (e.type == ui::EventType::MouseScroll)
	{
		int64_t diff = round(e.delta.y / 40) * 16;
		if (diff > 0 && diff > state->basePos)
			state->basePos = 0;
		else
			state->basePos -= diff;
		state->basePos = std::min(file->dataSource->GetSize() - 1, state->basePos);
		OnHexViewerStateChanged.Call(state);
	}
	else if (e.type == ui::EventType::KeyAction)
	{
		auto p = state->selectionEnd;
		auto w = state->byteWidth;
		size_t ph = ui::max(size_t(floor(GetFinalRect().GetHeight() / (contentFont.size + 4)) - 2), size_t(0));
		switch (e.GetKeyAction())
		{
		case ui::KeyAction::PageUp:
			state->basePos = std::max(0LL, int64_t(state->basePos) - ph * w);
			OnHexViewerStateChanged.Call(state);
			break;
		case ui::KeyAction::PageDown:
			state->basePos = std::max(0LL, int64_t(state->basePos) + ph * w);
			OnHexViewerStateChanged.Call(state);
			break;

		case ui::KeyAction::PrevLetter: MoveSelection(this, e, p - 1); break;
		case ui::KeyAction::NextLetter: MoveSelection(this, e, p + 1); break;
		case ui::KeyAction::GoToLineStart: MoveSelection(this, e, p - (p % w)); break;
		case ui::KeyAction::GoToLineEnd: MoveSelection(this, e, p + w - (p % w) - 1); break;
		case ui::KeyAction::GoToStart: MoveSelection(this, e, 0); break;
		case ui::KeyAction::GoToEnd: MoveSelection(this, e, INT64_MAX); break;
		case ui::KeyAction::Up: MoveSelection(this, e, p - w); break;
		case ui::KeyAction::Down: MoveSelection(this, e, p + w); break;
		}
	}
}

void HexViewer::OnPaint(const ui::UIPaintContext& ctx)
{
	int W = state->byteWidth;
	ui::Font* font = contentFont.GetFont();

	auto minSel = std::min(state->selectionStart, state->selectionEnd);
	auto maxSel = std::max(state->selectionStart, state->selectionEnd);

	uint8_t buf[256 * 64];
	static ByteColors bcol[256 * 64];
	memset(&bcol, 0, sizeof(bcol));
	size_t sz = file->dataSource->Read(GetBasePos(), W * 64, buf);

	float fh = contentFont.size + 4;
	float x = GetFinalRect().x0 + 2 + ui::GetTextWidth(font, contentFont.size, "0") * 8;
	float y = GetFinalRect().y0 + fh * 2;
	float x2 = x + 20 * W + 10;

	Highlight(highlightSettings, state, dataDesc, file, state->basePos, state->endianness, &bcol[0], buf, sz);

	for (size_t i = 0; i < sz; i++)
	{
		uint8_t v = buf[i];
		auto pos = GetBasePos() + i;

		ui::Color4f col = bcol[i].hexColor;// highlighter->GetByteTypeBin(GetBasePos(), buf, i, sz);
		if (pos >= minSel && pos <= maxSel)
			col.BlendOver(state->colorSelect);
		if (state->hoverByte == pos)
			col.BlendOver(state->colorHover);
		if (col.a > 0)
		{
			float xoff = (i % W) * 20;
			float yoff = (i / W) * fh;
			ui::draw::RectCol(x + xoff - 2, y + yoff - fh + 4, x + xoff + 18, y + yoff + 3, col);
		}

		col = bcol[i].asciiColor;// highlighter->GetByteTypeASCII(GetBasePos(), buf, i, sz);
		if (pos >= minSel && pos <= maxSel)
			col.BlendOver(state->colorSelect);
		if (state->hoverByte == pos)
			col.BlendOver(state->colorHover);
		if (col.a > 0)
		{
			float xoff = (i % W) * 10;
			float yoff = (i / W) * fh;
			ui::draw::RectCol(x2 + xoff - 2, y + yoff - fh + 4, x2 + xoff + 8, y + yoff + 3, col);
		}

		col = bcol[i].leftBracketColor;
		if (col.a > 0)
		{
			float xoff = (i % W) * 20;
			float yoff = (i / W) * fh;
			ui::draw::RectCol(x + xoff - 2, y + yoff - fh + 5, x + xoff - 1, y + yoff + 2, col);
			ui::draw::RectCol(x + xoff - 2, y + yoff - fh + 4, x + xoff + 4, y + yoff - fh + 5, col);
			ui::draw::RectCol(x + xoff - 2, y + yoff + 2, x + xoff + 4, y + yoff + 3, col);
		}

		col = bcol[i].rightBracketColor;
		if (col.a > 0)
		{
			float xoff = (i % W) * 20;
			float yoff = (i / W) * fh;
			ui::draw::RectCol(x + xoff + 17, y + yoff - fh + 5, x + xoff + 18, y + yoff + 2, col);
			ui::draw::RectCol(x + xoff + 12, y + yoff - fh + 4, x + xoff + 18, y + yoff - fh + 5, col);
			ui::draw::RectCol(x + xoff + 12, y + yoff + 2, x + xoff + 18, y + yoff + 3, col);
		}
	}

	ui::Color4b colHalfTransparentWhite(255, 127);
	ui::Color4b colWhite = ui::Color4b::White();

	auto size = file->dataSource->GetSize();
	for (int i = 0; i < 64; i++)
	{
		if (GetBasePos() + i * W >= size)
			break;
		char str[16];
		snprintf(str, 16, "%" PRIX64, GetBasePos() + i * W);
		float w = ui::GetTextWidth(font, contentFont.size, str);
		ui::draw::TextLine(font, contentFont.size, x - w - 10, y + i * fh, str, colHalfTransparentWhite);
	}

	for (int i = 0; i < W; i++)
	{
		char str[3];
		str[0] = "0123456789ABCDEF"[i >> 4];
		str[1] = "0123456789ABCDEF"[i & 0xf];
		str[2] = 0;
		float xoff = (i % W) * 20;
		ui::draw::TextLine(font, contentFont.size, x + xoff, y - fh, str, colHalfTransparentWhite);
	}

	for (size_t i = 0; i < sz; i++)
	{
		uint8_t v = buf[i];
		char str[3];
		str[0] = "0123456789ABCDEF"[v >> 4];
		str[1] = "0123456789ABCDEF"[v & 0xf];
		str[2] = 0;
		float xoff = (i % W) * 20;
		float yoff = (i / W) * fh;
		ui::draw::TextLine(font, contentFont.size, x + xoff, y + yoff, str, colWhite);
		str[1] = IsASCII(v) ? v : '.';
		ui::draw::TextLine(font, contentFont.size, x2 + xoff / 2, y + yoff, str + 1, colWhite);
	}
}

ui::UIRect HexViewer::GetByteRect(uint64_t pos)
{
	int64_t at = pos - GetBasePos();

	int x = at % state->byteWidth;
	if (x < 0)
		x += state->byteWidth;

	int y = at / state->byteWidth;

	auto* font = contentFont.GetFont();
	float fh = contentFont.size + 4;
	float x0 = GetFinalRect().x0 + ui::GetTextWidth(font, contentFont.size, "0") * 8 + x * 20;
	float y0 = GetFinalRect().y0 + 4 + fh * (1 + y);

	return { x0, y0, x0 + 16, y0 + fh };
}
