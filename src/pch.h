
#pragma once

#pragma warning(disable:4996)
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include "GUI.h"


enum GlobalEvents
{
	GlobalEvent_OpenImage,
	GlobalEvent_OpenImageRsrcEditor,
	GlobalEvent_OpenMeshRsrcEditor,
};

template <class T>
struct WindowT : ui::NativeMainWindow
{
	void OnBuild() override
	{
		rootBuildable = &ui::Make<T>();
	}
	void OnClose() override
	{
		if (subWindow)
			SetVisible(false);
		else
			ui::NativeMainWindow::OnClose();
	}
	T* rootBuildable = nullptr;
	bool subWindow = true;
};

using NamedTextSerializeReader = ui::NamedTextSerializeReader;
using NamedTextSerializeWriter = ui::NamedTextSerializeWriter;
