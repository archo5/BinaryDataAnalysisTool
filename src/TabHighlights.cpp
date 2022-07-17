
#include "pch.h"
#include "TabHighlights.h"

#include "Workspace.h"


static float hsplitHighlightsTab1[1] = { 0.6f };

void TabHighlights::Build()
{
	ui::Push<ui::SplitPane>().Init(ui::Direction::Horizontal, hsplitHighlightsTab1);
	{
		ui::Push<ui::EdgeSliceLayoutElement>();

		ui::MakeWithText<ui::LabelFrame>("Highlighted items");

		auto& tv = ui::Make<ui::TableView>();
		curTable = &tv;
		tv.enableRowHeader = false;
		tv.SetDataSource(&of->hexViewerState.highlightList);
		//tv.SetSelectionStorage(&of->hexViewerState.highlightList);
		//tv.SetSelectionMode(ui::SelectionMode::Single);
		tv.CalculateColumnWidths();

		ui::Pop();

		ui::Push<ui::StackTopDownLayoutElement>();
		of->highlightSettings.EditUI();
		ui::Pop();
	}
	ui::Pop();
}
