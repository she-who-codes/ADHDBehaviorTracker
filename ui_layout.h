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
