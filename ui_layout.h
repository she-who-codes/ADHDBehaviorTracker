#pragma once

// Small helpers for a fluid layout: rows of controls that wrap onto the next
// line when the window gets narrow (instead of running off the edge and being
// cut off), and widths that shrink with the window but never so far that
// their text gets clipped.

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace ui
{

// Right edge (screen X) of the space the current window gives its items.
inline float ContentRight()
{
    return ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
}

// Width a button needs to show 'label' without clipping it.
inline float ButtonWidthFor(const char *label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// Call between two items, in place of ImGui::SameLine(0, spacing). Keeps the
// next item (nextWidth wide) on the current line when it fits; otherwise it
// starts a new line, at local X 'wrapIndentX' when one is given (e.g. to line
// up under a label column) and the item fits there. Returns true if it stayed
// on the same line.
inline bool SameLineIfFits(float nextWidth, float spacing = -1.0f, float wrapIndentX = -1.0f)
{
    const float gap = (spacing < 0.0f) ? ImGui::GetStyle().ItemSpacing.x : spacing;
    const float right = ContentRight();
    if (ImGui::GetItemRectMax().x + gap + nextWidth <= right)
    {
        ImGui::SameLine(0.0f, gap);
        return true;
    }
    if (wrapIndentX >= 0.0f)
    {
        // Screen X of local X 0 (the cursor is at the start of the new line).
        const float localOrigin = ImGui::GetCursorScreenPos().x - ImGui::GetCursorPosX();
        if (localOrigin + wrapIndentX + nextWidth <= right)
            ImGui::SetCursorPosX(wrapIndentX);
    }
    return false;
}

// Puts the next item (width w) against the right edge: on the current line
// when it fits after the previous item, otherwise right-aligned on a line of
// its own. Never pushes it past the left edge.
inline void SameLineRightAligned(float w, float minGap = -1.0f)
{
    const float gap = (minGap < 0.0f) ? ImGui::GetStyle().ItemSpacing.x : minGap;
    const float right = ContentRight();
    if (ImGui::GetItemRectMax().x + gap + w <= right)
        ImGui::SameLine(0.0f, gap);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(std::max(right - w, cursor.x), cursor.y));
}

// Width for each of 'count' side-by-side buttons: 'preferred' when there is
// room, shrinking to share 'available' (with 'gap' between buttons), but
// never narrower than 'minimum' (so labels stay readable).
inline float FitButtonWidth(float preferred, float minimum, int count, float available, float gap)
{
    const float shared =
        std::floor((available - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
    return std::max(minimum, std::min(preferred, shared));
}

// ---- Results tables ----------------------------------------------------------
// The results tables (History's day detail, Trends' rankings and Time of Day)
// draw a thin vertical line between columns and leave a small gap on each
// side of it, so neighbouring columns and headers never run together (e.g.
// "Points" and "Total After" reading as "Points Total After"), even when a
// narrow window packs the columns tightly.

// Add to the table's flags: thin lines between columns.
inline constexpr ImGuiTableFlags kTableColumnDividers = ImGuiTableFlags_BordersInnerV;

// Space between a cell's text and the column divider (the app's normal cell
// padding is 6), so columns are about 20px apart with the line in the middle.
inline constexpr float kTableCellPaddingX = 10.0f;

// Width each column takes up beyond its contents: the gap on both sides plus
// its divider. Use this when working out how wide a table needs to be.
inline float TableColumnExtraWidth()
{
    return kTableCellPaddingX * 2.0f + 1.0f;
}

// Call PushTableSpacing() before BeginTable() and PopTableSpacing() after the
// table (whether or not BeginTable() returned true).
inline void PushTableSpacing()
{
    ImGui::PushStyleVarX(ImGuiStyleVar_CellPadding, kTableCellPaddingX);
}
inline void PopTableSpacing()
{
    ImGui::PopStyleVar();
}

// Size and scrolling for a results table, worked out before BeginTable():
//
//   ui::TableLayout layout = ui::PlanTable(rows, minimumHeight);
//   ... pick column widths to fit layout.room ...
//   ui::FinishTable(layout, neededWidth);
//   BeginTable(id, columns, flags | layout.flags, ImVec2(0, layout.height),
//              layout.innerWidth);
//
// The table is just tall enough for its rows when they fit in the space left
// (so it ends cleanly at the last row, with no empty strip below it), or it
// fills the space and scrolls if there are more rows (never shorter than
// 'minimumHeight'; the screen scrolls to it instead). It only scrolls
// sideways, and only leaves room for scrollbars, when it really needs them.
struct TableLayout
{
    float room = 0.0f;        // width the columns can use without scrolling sideways
    float height = 0.0f;      // outer height to pass to BeginTable()
    float innerWidth = 0.0f;  // inner width to pass to BeginTable()
    ImGuiTableFlags flags = 0; // ImGuiTableFlags_ScrollX when it scrolls sideways
    bool scrollsDown = false; // more rows than fit: a vertical scrollbar shows
    float contentHeight = 0.0f;
    float available = 0.0f;   // height left in the window
    float minimum = 0.0f;
};

// Height of a table's rows (header included), as ImGui lays them out.
inline float TableRowsHeight(size_t rows)
{
    const float rowHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2.0f;
    return rowHeight * static_cast<float>(rows + 1);
}

// Step 1: height and vertical scrolling for 'rows' rows plus the header.
inline TableLayout PlanTable(size_t rows, float minimumHeight = 0.0f)
{
    TableLayout layout;
    // +1 for the closing line under the last row (see EndTableRows()).
    layout.contentHeight = TableRowsHeight(rows) + 1.0f;
    layout.available = ImGui::GetContentRegionAvail().y;
    layout.minimum = minimumHeight;
    layout.height = (layout.contentHeight <= layout.available)
                        ? layout.contentHeight
                        : std::max(layout.available,
                                   std::min(layout.contentHeight, minimumHeight));
    layout.scrollsDown = layout.contentHeight > layout.height;
    layout.room = ImGui::GetContentRegionAvail().x -
                  (layout.scrollsDown ? ImGui::GetStyle().ScrollbarSize : 0.0f);
    return layout;
}

// Step 2: once the columns' total width is known ('neededWidth', padding
// included), scroll sideways only if they don't fit in layout.room.
inline void FinishTable(TableLayout &layout, float neededWidth)
{
    if (neededWidth <= layout.room)
        return;
    const float scrollbar = ImGui::GetStyle().ScrollbarSize;
    layout.flags |= ImGuiTableFlags_ScrollX;
    layout.innerWidth = neededWidth;
    if (!layout.scrollsDown)
    {
        // Make room under the rows for the sideways scrollbar; if that no
        // longer fits, the table fills the space and scrolls down too.
        const float wanted = layout.contentHeight + scrollbar;
        layout.height = (wanted <= layout.available)
                            ? wanted
                            : std::max(layout.available, std::min(wanted, layout.minimum));
        layout.scrollsDown = wanted > layout.height;
    }
}

// Call after a table's last row, just before EndTable(). Closes the table off
// with the same thin line that separates its rows, so a table never looks
// unfinished. (ImGui draws each separating line as the top edge of the next
// row, and skips the one under the last row when it falls right on the
// bottom edge, which is exactly where it is when a scrolling table is
// scrolled to the end. This ends the rows with a 1px row whose top edge is
// that line, and which is part of what scrolls, so the line is always there.)
inline void EndTableRows()
{
    ImGui::PushStyleVarY(ImGuiStyleVar_CellPadding, 0.0f); // row is exactly 1px
    ImGui::TableNextRow(ImGuiTableRowFlags_None, 1.0f);
    ImGui::PopStyleVar();
}

// Text drawn against the right edge of the current table cell, e.g. times, so
// "12:56 PM" and "1:05 AM" line up on the right, leaving the gap and divider
// before the next column clear.
inline void TextRightAligned(const char *text, const ImVec4 *color = nullptr)
{
    const float textWidth = ImGui::CalcTextSize(text).x;
    const float room = ImGui::GetContentRegionAvail().x;
    if (room > textWidth)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room - textWidth);
    if (color)
        ImGui::TextColored(*color, "%s", text);
    else
        ImGui::TextUnformatted(text);
}

// True for a few frames after the app window changes size (an auto-resizing
// popup needs a frame or two to settle at its new size).
inline bool DisplayRecentlyResized()
{
    static ImVec2 lastSize(0.0f, 0.0f);
    static int lastFrame = -1;
    static int framesSinceResize = 1000;

    const int frame = ImGui::GetFrameCount();
    if (frame != lastFrame)
    {
        const ImVec2 size = ImGui::GetMainViewport()->Size;
        if (lastFrame >= 0 && (size.x != lastSize.x || size.y != lastSize.y))
            framesSinceResize = 0;
        else if (framesSinceResize < 1000)
            ++framesSinceResize;
        lastSize = size;
        lastFrame = frame;
    }
    return framesSinceResize < 3;
}

// Call right before BeginPopupModal(): centers the popup when it opens, and
// re-centers it after the app window is resized so it never ends up partly
// off-screen. (Between resizes it stays wherever the user moved it.)
inline void CenterNextPopup()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            DisplayRecentlyResized() ? ImGuiCond_Always : ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
}

// Width for a popup that would like to be 'preferred' wide, narrowed so it
// still fits inside the app window with a margin on each side.
inline float FitPopupWidth(float preferred, float minimum = 0.0f)
{
    const float margin = ImGui::GetStyle().WindowPadding.x;
    return std::max(minimum, std::min(preferred, ImGui::GetMainViewport()->Size.x - margin * 2.0f));
}

} // namespace ui
