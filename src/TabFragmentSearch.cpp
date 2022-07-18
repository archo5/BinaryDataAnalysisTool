
#include "pch.h"
#include "TabFragmentSearch.h"

#include "Workspace.h"


static float hsplitFragmentSearchTab1[1] = { 0.6f };

void TabFragmentSearch::Build()
{
	ui::Push<ui::SplitPane>().Init(ui::Direction::Horizontal, hsplitFragmentSearchTab1);
	{
		ui::Push<ui::EdgeSliceLayoutElement>();

		ui::MakeWithText<ui::LabelFrame>("Found items");

		auto& tv = ui::Make<ui::TableView>();
		curTable = &tv;
		tv.enableRowHeader = false;
		tv.style.cellFont.family = ui::FONT_FAMILY_MONOSPACE;
		tv.SetDataSource(&of->search);
		//tv.SetSelectionStorage(&of->hexViewerState.highlightList);
		//tv.SetSelectionMode(ui::SelectionMode::Single);
		tv.CalculateColumnWidths();
		tv.HandleEvent(&tv, ui::EventType::Click) = [this, &tv](ui::Event& e)
		{
			size_t row = tv.GetHoverRow();
			if (row != SIZE_MAX && e.GetButton() == ui::MouseButton::Left && e.numRepeats == 2)
			{
				auto off = of->search.results[row];
				of->hexViewerState.GoToPos(off);
			}
		};

		ui::Pop();

		ui::Push<ui::StackTopDownLayoutElement>();
		of->search.SearchUI(of->ddFile->dataSource);
		ui::Pop();
	}
	ui::Pop();
}
