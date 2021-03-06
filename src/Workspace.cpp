
#include "pch.h"
#include "Workspace.h"


ui::MulticastDelegate<OpenedFile*> OnCurrentFileChanged;


void OpenedFile::Load(NamedTextSerializeReader& r)
{
	r.BeginDict("");
	fileID = r.ReadUInt64("fileID");
	hexViewerState.basePos = r.ReadUInt64("basePos");
	hexViewerState.byteWidth = r.ReadUInt("byteWidth");
	hexViewerState.endianness = EndiannessFromString(r.ReadString("endianness"));
	highlightSettings.Load("highlighter", r);
	r.EndDict();
}

void OpenedFile::Save(NamedTextSerializeWriter& w)
{
	w.BeginDict("");
	w.WriteInt("fileID", fileID);
	w.WriteInt("basePos", hexViewerState.basePos);
	w.WriteInt("byteWidth", hexViewerState.byteWidth);
	w.WriteString("endianness", EndiannessToString(hexViewerState.endianness));
	highlightSettings.Save("highlighter", w);
	w.EndDict();
}

void Workspace::Load(NamedTextSerializeReader& r)
{
	Clear();
	r.BeginDict("workspace");

	desc.Load("desc", r);
	for (auto* F : desc.files)
	{
		std::string path = F->path;
		// TODO workspace-relative paths?
		F->origDataSource = GetFileDataSource(path.c_str());
		F->dataSource = GetSlice(F->origDataSource, F->off, F->size);
		F->mdSrc.dataSource = F->dataSource;
	}

	r.BeginArray("openedFiles");
	for (auto E : r.GetCurrentRange())
	{
		r.BeginEntry(E);

		auto* F = new OpenedFile;
		F->Load(r);
		F->ddFile = desc.FindFileByID(F->fileID);
		openedFiles.push_back(F);

		r.EndEntry();
	}
	r.EndArray();

	r.EndDict();
}

void Workspace::Save(NamedTextSerializeWriter& w)
{
	w.BeginDict("workspace");
	w.WriteString("version", "1");

	desc.Save("desc", w);

	w.BeginArray("openedFiles");
	for (auto* F : openedFiles)
		F->Save(w);
	w.EndArray();

	w.EndDict();
}

bool Workspace::LoadFromFile(ui::StringView path)
{
	auto data = ui::ReadTextFile(path);
	if (data.result != ui::IOResult::Success)
		return false;
	NamedTextSerializeReader ntsr;
	bool parsed = ntsr.Parse(data.data->GetStringView());
	printf("parsed: %s\n", parsed ? "yes" : "no");
	Load(ntsr);
	return true;
}

bool Workspace::SaveToFile(ui::StringView path)
{
	NamedTextSerializeWriter ntsw;
	Save(ntsw);
	return ui::WriteTextFile(path, ntsw.data);
}
