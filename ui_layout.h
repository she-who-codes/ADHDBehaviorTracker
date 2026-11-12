#pragma once

// Small layout helpers that keep the screens usable when the app window is
// made smaller. Rows of buttons "wrap" onto the next line when there is no
// room left (like words in a paragraph) instead of running off the right
// edge, and panels get a minimum size so the window scrolls rather than
// squashing them. When everything fits, these produce exactly the same
// layout as plain ImGui::SameLine().

#include "imgui.h"

namespace UiLayout
{

// Screen x of the right edge of the area the current row can use.
inline float RowRight()
{
    return ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
}

// Width of a checkbox with a visible label (box + gap + text).
inline float CheckboxWidth(const char *label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
           ImGui::CalcTextSize(label, nullptr, true).x;
}

// Width a button needs to show its whole label.
inline float ButtonLabelWidth(const char *label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// Use between two items instead of ImGui::SameLine(0, spacing).
// The next item (nextWidth wide) stays on the same row when it fits before
// rowRight (screen x; default: the window's right edge). Otherwise it starts
// a new row, at window-local x 'wrapX' when given (e.g. to line up under a
// label column). spacing < 0 means the style's normal item spacing.
// Returns true if the item stays on the same row.
inline bool SameLineOrWrap(float nextWidth, float spacing = -1.0f, float wrapX = -1.0f,
                           float rowRight = -1.0f)
{
    const float gap = (spacing < 0.0f) ? ImGui::GetStyle().ItemSpacing.x : spacing;
    const float right = (rowRight < 0.0f) ? RowRight() : rowRight;
    if (ImGui::GetItemRectMax().x + gap + nextWidth <= right + 0.5f)
    {
        ImGui::SameLine(0.0f, gap);
        return true;
    }
    if (wrapX >= 0.0f)
        ImGui::SetCursorPosX(wrapX);
    return false;
}

// Puts the next item (width wide) at the right end of the current row when it
// fits after the last item; otherwise it starts a new row (at window-local x
// 'wrapX' when given, else at the row start). Returns true if it stayed on
// the row.
inline bool SameLineRightAlignedOrWrap(float width, float wrapX = -1.0f)
{
    const float right = RowRight();
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right + 0.5f)
    {
        // Same position as ImGui::SameLine(rowStartX + rowWidth - width)
        ImGui::SameLine(right - width - ImGui::GetWindowPos().x + ImGui::GetScrollX());
        return true;
    }
    if (wrapX >= 0.0f)
        ImGui::SetCursorPosX(wrapX);
    return false;
}

// Like ImGui::PushTextWrapPos(0.0f) (wrap text at the window's right edge),
// but always at the *visible* edge. In a window that can scroll sideways,
// ImGui's default wrap point follows the content width instead, so a long
// line of text could keep the window wider than the screen after shrinking
// it. Call at the start of a row; pair with ImGui::PopTextWrapPos().
inline void PushTextWrapAtVisibleEdge()
{
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
}

// Height for a panel that normally fills the rest of the window (height 0).
// Returns 0 (fill) while there is enough room, or minHeight when the window
// is too short, so the panel keeps a usable size and the window scrolls.
inline float FillHeightAtLeast(float minHeight)
{
    return (ImGui::GetContentRegionAvail().y >= minHeight) ? 0.0f : minHeight;
}

// Re-centres a popup when the app window has been resized while it is open,
// so it can't end up off-screen. Call right before SetNextWindowPos(...,
// ImGuiCond_Appearing) of the popup; 'lastDisplaySize' is per-popup storage.
inline ImGuiCond CenterCondition(ImVec2 &lastDisplaySize)
{
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const bool resized = (size.x != lastDisplaySize.x || size.y != lastDisplaySize.y);
    lastDisplaySize = size;
    return resized ? ImGuiCond_Always : ImGuiCond_Appearing;
}

} // namespace UiLayout