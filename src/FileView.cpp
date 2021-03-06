
#include "pch.h"
#include "FileView.h"

#include "HexViewer.h"
#include "FileReaders.h"
#include "DataDesc.h"
#include "ImageParsers.h"
#include "Workspace.h"


static bool viewSettingsOpen = false;
void FileView::Build()
{
	ui::Push<ui::EdgeSliceLayoutElement>();

	ui::BuildMulticastDelegateAddNoArgs(OnHexViewerStateChanged, [this]() { Rebuild(); });

	ui::Push<ui::StackLTRLayoutElement>();
	{
		char buf[256];
		if (of->hexViewerState.selectionStart != UINT64_MAX)
		{
			int64_t selMin = ui::min(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
			int64_t selMax = ui::max(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
			int64_t len = selMax - selMin + 1;
			int64_t ciOff = 0, ciSize = 0;
			if (auto* SI = workspace->desc.curInst)
			{
				ciOff = SI->off;
				ciSize = SI->def->size;
			}
			snprintf(buf, 256,
				"Selection: %" PRIu64 " - %" PRIu64 " (%" PRIu64 ") rel: %" PRId64 " fit: %" PRId64 " rem: %" PRId64,
				selMin, selMax, len,
				selMin - ciOff,
				ciSize ? len / ciSize : 0,
				ciSize ? len % ciSize : 0);
		}
		else
			snprintf(buf, 256, "Selection: <none>");
		ui::Text(buf);
	}
	ui::Pop();

	ui::imm::EditBool(viewSettingsOpen, "View settings", {}, ui::imm::TreeStateToggleSkin());

	ui::Push<ui::StackTopDownLayoutElement>(); // tree stabilization box
	if (viewSettingsOpen)
	{
		ui::imm::PropEditInt("Width", of->hexViewerState.byteWidth, {}, {}, { 1, 256 });
		ui::imm::PropEditInt("Position", of->hexViewerState.basePos);
		ui::imm::PropDropdownMenuList("Endianness", of->hexViewerState.endianness, ui::BuildAlloc<ui::ZeroSepCStrOptionList>("Little\0Big\0"));
	}
	ui::Pop(); // end tree stabilization box

	auto& hv = ui::Make<HexViewer>();
	curHexViewer = &hv;
	hv.Init(&workspace->desc, of->ddFile, &of->hexViewerState, &of->highlightSettings);
	hv.HandleEvent(ui::EventType::ButtonUp) = [this](ui::Event& e)
	{
		if (e.GetButton() == ui::MouseButton::Right)
		{
			HexViewer_OnRightClick();
		}
	};
	ui::Pop();
}

void FileView::HexViewer_OnRightClick()
{
	auto* ds = of->ddFile->dataSource.get_ptr();
	int64_t pos = of->hexViewerState.hoverByte;
	auto endianness = of->hexViewerState.endianness;
	auto selMin = ui::min(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
	auto selMax = ui::max(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
	int64_t size = 0;
	if (selMin != UINT64_MAX && selMax != UINT64_MAX && selMin <= pos && pos <= selMax)
	{
		pos = selMin;
		size = selMax - selMin;
	}

	char txt_pos[64];
	snprintf(txt_pos, 64, "@ %" PRIu64 " (0x%" PRIX64 ")", pos, pos);

	char txt_sel[64];
	if (size == 0)
		strcpy(txt_sel, txt_pos);
	else
		snprintf(txt_sel, 64, "%" PRIu64 " - %" PRIu64, pos, pos + size);

	char txt_ascii[32];
	ds->GetASCIIText(txt_ascii, 32, pos);

	char txt_int8[32];
	ds->GetInt8Text(txt_int8, 32, pos, true);
	char txt_uint8[32];
	ds->GetInt8Text(txt_uint8, 32, pos, false);
	char txt_int16[32];
	ds->GetInt16Text(txt_int16, 32, pos, endianness, true);
	char txt_uint16[32];
	ds->GetInt16Text(txt_uint16, 32, pos, endianness, false);
	char txt_int32[32];
	ds->GetInt32Text(txt_int32, 32, pos, endianness, true);
	char txt_uint32[32];
	ds->GetInt32Text(txt_uint32, 32, pos, endianness, false);
	char txt_int64[32];
	ds->GetInt64Text(txt_int64, 32, pos, endianness, true);
	char txt_uint64[32];
	ds->GetInt64Text(txt_uint64, 32, pos, endianness, false);
	char txt_float32[32];
	ds->GetFloat32Text(txt_float32, 32, pos, endianness);
	char txt_float64[32];
	ds->GetFloat64Text(txt_float64, 32, pos, endianness);

	int32_t val_int32;
	ds->Read(pos, sizeof(val_int32), &val_int32);
	EndiannessAdjust(val_int32, endianness);
	uint32_t val_uint32 = val_int32;

	std::vector<ui::MenuItem> structs;
	{
		structs.push_back(ui::MenuItem("Create a new struct (blank)").Func([this, pos]() { CreateBlankStruct(pos); }));
		structs.push_back(ui::MenuItem("Create a new struct (from markers)", {},
			of->hexViewerState.selectionStart == UINT64_MAX || of->hexViewerState.selectionEnd == UINT64_MAX)
			.Func([this, pos]() { CreateStructFromMarkers(pos); }));
		if (workspace->desc.structs.size())
			structs.push_back(ui::MenuItem::Separator());
	}
	for (auto& s : workspace->desc.structs)
	{
		auto fn = [this, pos, s]()
		{
			workspace->desc.SetCurrentInstance(workspace->desc.AddInstance({ -1LL, &workspace->desc, s.second, of->ddFile, pos, "", CreationReason::UserDefined }));
		};
		structs.push_back(ui::MenuItem(s.first).Func(fn));
	}

	std::vector<ui::MenuItem> images;
	ui::StringView prevCat;
	for (size_t i = 0, count = GetImageFormatCount(); i < count; i++)
	{
		ui::StringView cat = GetImageFormatCategory(i);
		if (cat != prevCat)
		{
			prevCat = cat;
			images.push_back(ui::MenuItem(cat, {}, true));
		}
		ui::StringView name = GetImageFormatName(i);
		images.push_back(ui::MenuItem(name).Func([this, pos, name]() { CreateImage(pos, name); }));
	}

	auto omr = of->ddFile->offModRanges.TransformOffset(pos, val_uint32, ds->GetSize());
	char txt_adjuint32[64];
	snprintf(txt_adjuint32, sizeof(txt_adjuint32), "%" PRIu64, omr.newOffset);

	auto& md = of->ddFile->markerData;
	ui::MenuItem items[] =
	{
		ui::MenuItem(txt_pos, {}, true),
		ui::MenuItem::Submenu("Place struct", structs),
		ui::MenuItem::Submenu("Place image", images),
		ui::MenuItem::Separator(),
		ui::MenuItem("Go to adjusted offset (u32)", txt_adjuint32, !omr.valid).Func([this, &omr] { of->hexViewerState.GoToPos(omr.newOffset); }),
		ui::MenuItem("Go to offset (u32)", txt_uint32).Func([this, pos, endianness]() { GoToOffset(pos, endianness); }),
		ui::MenuItem::Separator(),
		ui::MenuItem("Mark ASCII", txt_ascii).Func([&md, pos, endianness]() { md.AddMarker(DT_CHAR, endianness, pos, pos + 1); }),
		ui::MenuItem("Mark int8", txt_int8).Func([&md, pos, endianness]() { md.AddMarker(DT_I8, endianness, pos, pos + 1); }),
		ui::MenuItem("Mark uint8", txt_uint8).Func([&md, pos, endianness]() { md.AddMarker(DT_U8, endianness, pos, pos + 1); }),
		ui::MenuItem("Mark int16", txt_int16).Func([&md, pos, endianness]() { md.AddMarker(DT_I16, endianness, pos, pos + 2); }),
		ui::MenuItem("Mark uint16", txt_uint16).Func([&md, pos, endianness]() { md.AddMarker(DT_U16, endianness, pos, pos + 2); }),
		ui::MenuItem("Mark int32", txt_int32).Func([&md, pos, endianness]() { md.AddMarker(DT_I32, endianness, pos, pos + 4); }),
		ui::MenuItem("Mark uint32", txt_uint32).Func([&md, pos, endianness]() { md.AddMarker(DT_U32, endianness, pos, pos + 4); }),
		ui::MenuItem("Mark int64", txt_int64).Func([&md, pos, endianness]() { md.AddMarker(DT_I64, endianness, pos, pos + 8); }),
		ui::MenuItem("Mark uint64", txt_uint64).Func([&md, pos, endianness]() { md.AddMarker(DT_U64, endianness, pos, pos + 8); }),
		ui::MenuItem("Mark float32", txt_float32).Func([&md, pos, endianness]() { md.AddMarker(DT_F32, endianness, pos, pos + 4); }),
		ui::MenuItem("Mark float64", txt_float64).Func([&md, pos, endianness]() { md.AddMarker(DT_F64, endianness, pos, pos + 8); }),
		ui::MenuItem::Separator(),
		ui::MenuItem("Highlight all int32", txt_int32).Func([this, val_int32]() { of->highlightSettings.AddCustomInt32(val_int32); }),
		ui::MenuItem("Highlight offset as int32", txt_pos + 2).Func([this, pos]() { of->highlightSettings.AddCustomInt32(pos); }),
		ui::MenuItem::Separator(),
		ui::MenuItem("Create a subview", txt_pos).Func([this, pos]() { CreateSubviewAt(pos); }),
		ui::MenuItem("Create off.mod.range", txt_sel).Func([this, pos, size]() { of->ddFile->offModRanges.ranges.push_back({ uint64_t(pos), uint64_t(size) }); }),
	};
	ui::Menu menu(items);
	menu.Show(this);
	Rebuild();
}

DDStruct* FileView::CreateBlankStruct(int64_t pos)
{
	auto* ns = new DDStruct;
	do
	{
		ns->name = "struct" + std::to_string(rand() % 10000);
	}
	while (workspace->desc.structs.count(ns->name));
	int64_t off = pos;
	if (of->hexViewerState.selectionStart != UINT64_MAX && of->hexViewerState.selectionEnd != UINT64_MAX)
	{
		off = ui::min(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
		ns->size = abs(int(of->hexViewerState.selectionEnd - of->hexViewerState.selectionStart)) + 1;
	}
	workspace->desc.structs[ns->name] = ns;
	workspace->desc.SetCurrentInstance(workspace->desc.AddInstance({ -1LL, &workspace->desc, ns, of->ddFile, off, "", CreationReason::UserDefined }));
	return ns;
}

DDStruct* FileView::CreateStructFromMarkers(int64_t pos)
{
	auto* ns = CreateBlankStruct(pos);
	auto selMin = ui::min(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
	auto selMax = ui::max(of->hexViewerState.selectionStart, of->hexViewerState.selectionEnd);
	int at = 0;
	for (Marker& M : of->ddFile->markerData.markers)
	{
		if (M.at < selMin || M.at > selMax)
			continue;
#if 0 // TODO
		for (DDField f;
			f.type = GetDataTypeName(M.type),
			f.name = f.type + "_" + std::to_string(at++),
			f.off = M.at - selMin,
			f.count = M.count,
			ns->fields.push_back(f),
			false;);
#endif
	}
	return ns;
}

void FileView::CreateImage(int64_t pos, ui::StringView fmt)
{
	DataDesc::Image img;
	img.userCreated = true;
	img.info.width = 4;
	img.info.height = 4;
	img.info.offImg = pos;
	img.info.offPal = 0;
	img.format.assign(fmt.data(), fmt.size());
	img.info.opaque = true;
	img.file = of->ddFile;
	workspace->desc.curImage = workspace->desc.images.size();
	workspace->desc.images.push_back(img);
}

void FileView::GoToOffset(int64_t pos, Endianness endianness)
{
	uint32_t off = 0;
	of->ddFile->dataSource->Read(pos, sizeof(off), &off);
	EndiannessAdjust(off, endianness);

	of->hexViewerState.GoToPos(off);
}

void FileView::CreateSubviewAt(uint64_t pos)
{
	auto* srcf = of->ddFile;
	auto* F = workspace->desc.CreateNewFile();
	uint64_t end = srcf->dataSource->GetSize();
	F->name = srcf->name;
	F->path = srcf->path;
	F->off = srcf->off + pos;
	F->size = end - pos;
	F->origDataSource = srcf->origDataSource;
	F->dataSource = GetSlice(srcf->origDataSource, F->off, F->size);
	F->mdSrc.dataSource = F->dataSource;
	for (auto& m : srcf->markerData.markers)
	{
		if (m.at < pos)
			continue;
		auto mcopy = m;
		mcopy.at -= pos;
		mcopy.compiled.structs.clear();
		mcopy.compiled.Parse(mcopy.def, true);
		F->markerData.markers.push_back(std::move(mcopy));
	}
	for (size_t i = 0, count = workspace->desc.images.size(); i < count; i++)
	{
		auto& I = workspace->desc.images[i];
		if (I.file != srcf || I.info.offImg < pos || (I.info.offPal != 0 && I.info.offPal < pos))
			continue;
		size_t di = workspace->desc.DuplicateImage(i);
		auto& DI = workspace->desc.images[di];
		DI.file = F;
		DI.info.offImg -= pos;
		if (DI.info.offPal)
			DI.info.offPal -= pos;
	}

	auto* nof = new OpenedFile;
	nof->ddFile = F;
	nof->fileID = F->id;
	nof->hexViewerState = of->hexViewerState;
	nof->highlightSettings = of->highlightSettings;
	nof->hexViewerState.basePos = ui::max(int64_t(nof->hexViewerState.basePos - pos), 0LL);
	nof->hexViewerState.selectionStart = ui::max(int64_t(nof->hexViewerState.selectionStart - pos), 0LL);
	nof->hexViewerState.selectionEnd = ui::max(int64_t(nof->hexViewerState.selectionEnd - pos), 0LL);
	workspace->openedFiles.push_back(nof);
	workspace->curOpenedFile = workspace->openedFiles.size() - 1;
	OnCurrentFileChanged.Call(nof);
}
