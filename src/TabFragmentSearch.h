
#pragma once
#include "pch.h"

#include "TableWithOffsets.h"


struct OpenedFile;


struct TabFragmentSearch : ui::Buildable, TableWithOffsets
{
	void Build() override;

	OpenedFile* of = nullptr;
};

struct TabFileFormatSearch : ui::Buildable, TableWithOffsets
{
	void Build() override;

	OpenedFile* of = nullptr;
};
