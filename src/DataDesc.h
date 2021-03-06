
#pragma once
#include "pch.h"
#include "FileReaders.h"
#include "MathExpr.h"
#include "ImageParsers.h"
#include "Markers.h"
#include "DataDescStruct.h"


extern ui::Color4f colorFloat32;
extern ui::Color4f colorInt16;
extern ui::Color4f colorInt32;
extern ui::Color4f colorNearFileSize32;
extern ui::Color4f colorNearFileSize64;
extern ui::Color4f colorASCII;
extern ui::Color4f colorInst;
extern ui::Color4f colorCurInst;


struct OffModRange
{
	uint64_t off = 0;
	uint64_t size = 0;
	uint64_t base = 0;
	uint64_t mul = 1;
};

struct OffModResult
{
	uint64_t newOffset;
	bool wasModified;
	bool valid;
};

struct OffModRanges
{
	std::vector<OffModRange> ranges;

	OffModResult TransformOffset(uint64_t pos, uint64_t val, uint64_t validLimit = UINT64_MAX);
	void Load(const char* key, NamedTextSerializeReader& r);
	void Save(const char* key, NamedTextSerializeWriter& w);
	void Edit();
};


struct DDFile
{
	uint64_t id = UINT64_MAX;
	std::string name;
	std::string path;
	uint64_t off = 0;
	uint64_t size = UINT64_MAX;
	ui::RCHandle<struct IDataSource> dataSource;
	ui::RCHandle<struct IDataSource> origDataSource;
	MarkerData markerData;
	MarkerDataSource mdSrc;
	OffModRanges offModRanges;

	std::string GetFileInfo() const
	{
		if (off == 0 && size == UINT64_MAX)
			return name;
		return ui::Format("%s[%" PRIu64 "-%" PRIu64 "]", name.c_str(), off, off + size);
	}
};


extern ui::MulticastDelegate<DataDesc*, DDStruct*> OnCurStructChanged;
extern ui::MulticastDelegate<DataDesc*, DDStructInst*> OnCurStructInstChanged;
struct DataDesc
{
	struct Image
	{
		DDFile* file = nullptr;
		ImageInfo info;
		std::string format;
		ui::HashMap<std::string, ImageFormatSettingsHandle> allSettings;
		std::string _savedSettings;
		std::string notes;
		bool userCreated = true;

		IImageFormatSettings& GetOrCreateSettings()
		{
			if (auto h = allSettings.get(format))
				return *h;
			ImageFormatSettingsHandle h = CreateImageFormatSettings(format);
			allSettings.insert(format, h);
			return *h;
		}
	};

	// data
	std::vector<DDFile*> files;
	std::unordered_map<std::string, DDStruct*> structs;
	std::vector<DDStructInst*> instances;
	std::vector<Image> images;

	// ID allocation
	uint64_t fileIDAlloc = 0;
	int64_t instIDAlloc = 0;

	// ui state
	int editMode = 0;
	DDStructInst* curInst = nullptr;
	uint32_t curImage = 0;
	uint32_t curField = 0;

	void EditStructuralItems();
	void EditInstance();
	void EditStruct();
	void EditField();

	void EditImageItems();

	DDStructInst* AddInstance(const DDStructInst& src);
	void DeleteInstance(DDStructInst* inst);
	void SetCurrentInstance(DDStructInst* inst);
	void _OnDeleteInstance(DDStructInst* inst);
	void ExpandAllInstances(DDFile* filterFile = nullptr);
	void DeleteAllInstances(DDFile* filterFile = nullptr, DDStruct* filterStruct = nullptr);
	DataDesc::Image GetInstanceImage(const DDStructInst& SI);

	~DataDesc();
	void Clear();
	DDFile* CreateNewFile();
	DDFile* FindFileByID(uint64_t id);
	DDStruct* CreateNewStruct(const std::string& name);
	DDStruct* FindStructByName(const std::string& name);
	DDStructInst* FindInstanceByID(int64_t id);

	void DeleteImage(size_t id);
	size_t DuplicateImage(size_t id);

	void Load(const char* key, NamedTextSerializeReader& r);
	void Save(const char* key, NamedTextSerializeWriter& w);
};

struct DataDescInstanceSource : ui::TableDataSource, ui::ISelectionStorage
{
	size_t GetNumRows() override;
	size_t GetNumCols() override;
	std::string GetRowName(size_t row) override;
	std::string GetColName(size_t col) override;
	std::string GetText(size_t row, size_t col) override;

	void ClearSelection() override;
	bool GetSelectionState(uintptr_t item) override;
	void SetSelectionState(uintptr_t item, bool sel) override;

	void Edit();

	void _Refilter();

	std::vector<size_t> _indices;
	bool refilter = true;

	DataDesc* dataDesc = nullptr;

	bool filterStructEnable = true;
	bool filterStructFollow = true;
	DDStruct* filterStruct = nullptr;

	bool filterHideStructsEnable = true;
	std::unordered_set<DDStruct*> filterHideStructs;

	bool filterFileEnable = true;
	bool filterFileFollow = true;
	DDFile* filterFile = nullptr;

	uint32_t showBytes = 0;

	CreationReason filterCreationReason = CreationReason::Query;
};

struct DataDescImageSource : ui::TableDataSource, ui::ISelectionStorage
{
	size_t GetNumRows() override;
	size_t GetNumCols() override;
	std::string GetRowName(size_t row) override;
	std::string GetColName(size_t col) override;
	std::string GetText(size_t row, size_t col) override;

	void ClearSelection() override;
	bool GetSelectionState(uintptr_t item) override;
	void SetSelectionState(uintptr_t item, bool sel) override;

	void Edit();

	void _Refilter();

	std::vector<size_t> _indices;
	bool refilter = true;

	DataDesc* dataDesc = nullptr;
	DDFile* filterFile = nullptr;
	bool filterFileEnable = true;
	bool filterFileFollow = true;
	bool filterUserCreated = false;
};

struct CachedImage
{
	ui::draw::ImageHandle GetImage(DataDesc::Image& imgDesc)
	{
		auto& settings = imgDesc.GetOrCreateSettings();
		auto expSettings = settings.ExportSettingsBlob();
		if (curImg)
		{
			if (imgDesc.file == curImgDesc.file &&
				imgDesc.info == curImgDesc.info &&
				imgDesc.format == curImgDesc.format &&
				expSettings == curImgDesc._savedSettings)
			{
				return curImg;
			}
		}

		curImg = CreateImageFrom(imgDesc.file->dataSource, imgDesc.format.c_str(), &settings, imgDesc.info);
		curImgDesc = imgDesc;
		curImgDesc._savedSettings = expSettings;
		return curImg;
	}

	ui::draw::ImageHandle curImg;
	DataDesc::Image curImgDesc;
};
