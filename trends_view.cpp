#include "trends_view.h"

#include "imgui.h"
#include "trends.h"
#include "trends_export.h"
#include "ui_layout.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Trends screen
// ---------------------------------------------------------------------------

static const ImVec4 kGoodText(0.35f, 0.85f, 0.55f, 1.00f);   // same as "points earned"
static const ImVec4 kWrongText(0.95f, 0.45f, 0.45f, 1.00f);  // same as "points lost"
static const ImVec4 kMutedText(0.58f, 0.60f, 0.66f, 1.00f);
static const ImVec4 kWarningText(0.98f, 0.78f, 0.22f, 1.00f);

static const ImVec4 kToggleOn(0.30f, 0.35f, 0.75f, 1.00f);     // app's normal button colour
static const ImVec4 kToggleOnHover(0.40f, 0.48f, 0.90f, 1.00f);
static const ImVec4 kToggleOff(0.16f, 0.18f, 0.29f, 1.00f);    // dim, like input frames
static const ImVec4 kToggleOffHover(0.22f, 0.25f, 0.40f, 1.00f);

static const ImVec4 kExportButton(0.12f, 0.55f, 0.35f, 1.00f); // matches History's button
static const ImVec4 kExportButtonHover(0.18f, 0.70f, 0.45f, 1.00f);
static const ImVec4 kExportButtonActive(0.10f, 0.45f, 0.28f, 1.00f);
static const ImVec2 kExportButtonSize(260.0f, 55.0f);

// Background for the calendar and hover hints: solid, so nothing shows through
static const ImVec4 kPickerBg(0.11f, 0.12f, 0.21f, 1.00f);

// Alternating row shading (drawn by hand so rows can be shaded per hour group)
static const ImVec4 kRowShade(1.00f, 1.00f, 1.00f, 0.05f);

static const char *kWeekdayShort[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *kMonthShort[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Everything the parent picked stays put while the app is open.
struct TrendsScreenState
{
    TrendsReportOptions options;
    Screen returnTo = Screen::Main;
    std::string lastToday;     // detects midnight while the screen is in use
    int presetDays = 0;        // 7 / 14 / 31 while a "Last N days" preset is active
    bool forceTabSelect = false; // one-frame flag set by "Reset to Defaults"
};
static TrendsScreenState g_trends;

void OpenTrendsScreen(Screen &screen)
{
    if (screen != Screen::Trends)
        g_trends.returnTo = screen;
    screen = Screen::Trends;
}

// "Sat, Oct 10"
static std::string ShortDateLabel(const HistoryDay &day)
{
    return std::string(kWeekdayShort[day.weekday]) + ", " + kMonthShort[day.month - 1] + " " +
           std::to_string(day.day);
}

static void ApplyPreset(TrendFilter &filter, const std::vector<HistoryDay> &days, int dayCount)
{
    const int count = static_cast<int>(days.size());
    if (dayCount > count) dayCount = count;
    filter.fromDate = days[count - dayCount].date;
    filter.toDate = days.back().date;
}

// Keeps the chosen dates inside the history window. If the range ended
// "today", it keeps ending today after midnight, so new logs show up.
static void KeepRangeInWindow(TrendsScreenState &state, const std::vector<HistoryDay> &days)
{
    TrendFilter &filter = state.options.filter;
    const std::string &oldest = days.front().date;
    const std::string &today = days.back().date;

    if (state.lastToday.empty())
    {
        // First visit: the whole history window
        state.presetDays = static_cast<int>(days.size());
        ApplyPreset(filter, days, state.presetDays);
    }
    else if (state.lastToday != today)
    {
        if (state.presetDays > 0)
            ApplyPreset(filter, days, state.presetDays);
        else if (filter.toDate == state.lastToday)
            filter.toDate = today;
    }
    state.lastToday = today;

    if (filter.fromDate < oldest) filter.fromDate = oldest;
    if (filter.toDate > today) filter.toDate = today;
    if (filter.toDate < oldest) filter.toDate = oldest;
    if (filter.fromDate > filter.toDate) filter.fromDate = filter.toDate;
}

// Puts every filter, toggle and tab back to what a parent sees on their very
// first visit: the whole 31-day window, every day of the week, both
// rankings most-first, and "Time of Day" showing all choices by hour.
static void ResetTrendsToDefaults(TrendsScreenState &state, const std::vector<HistoryDay> &days)
{
    state.presetDays = static_cast<int>(days.size());
    ApplyPreset(state.options.filter, days, state.presetDays);
    state.options.filter.weekdays.fill(true);

    state.options.goodOrder = RankOrder::MostFirst;
    state.options.wrongOrder = RankOrder::MostFirst;
    state.options.timeKind = ChoiceKind::All;
    state.options.timeSort = TimeOfDaySort::ByHour;

    state.options.tab = TrendsTab::MostLeast;
    state.forceTabSelect = true; // tells the tab bar to jump back to it below
}

// Hover hint for the item just drawn: a short explanation of what it does.
// Shows on disabled items too (ImGui's default), so they can say why.
static void Hint(const char *text)
{
    if (text == nullptr)
        return;
    ImGui::PushStyleColor(ImGuiCol_PopupBg, kPickerBg); // solid, like the calendar
    const bool showing = ImGui::BeginItemTooltip();
    ImGui::PopStyleColor();
    if (!showing)
        return;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// One button of a SegmentedToggle.
template <typename T>
struct ToggleOption
{
    const char *label;
    T value;
    const char *hint; // shown on hover
};

static const float kToggleGap = 4.0f;

// Total width of a SegmentedToggle's buttons when they are all on one row.
static float SegmentedToggleWidth(int count, float buttonWidth)
{
    return static_cast<float>(count) * buttonWidth + static_cast<float>(count - 1) * kToggleGap;
}

// A row of buttons where exactly one is "on". If the window is too narrow,
// the buttons that don't fit wrap onto the next row, under the first one.
template <typename T>
static void SegmentedToggle(const char *id, const std::vector<ToggleOption<T>> &options,
                            T &value, float buttonWidth)
{
    const float startX = ImGui::GetCursorPosX();
    ImGui::PushID(id);
    for (size_t i = 0; i < options.size(); ++i)
    {
        if (i > 0)
            UiLayout::SameLineOrWrap(buttonWidth, kToggleGap, startX);
        const bool on = (value == options[i].value);
        ImGui::PushStyleColor(ImGuiCol_Button, on ? kToggleOn : kToggleOff);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? kToggleOnHover : kToggleOffHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kToggleOn);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button(options[i].label, ImVec2(buttonWidth, 0.0f)))
            value = options[i].value;
        Hint(options[i].hint);
        ImGui::PopID();
        ImGui::PopStyleColor(3);
    }
    ImGui::PopID();
}

// ---- Calendar date picker ----------------------------------------------------

static const ImVec4 kTodayOutline(0.98f, 0.78f, 0.22f, 1.00f);    // same gold as History
static const ImVec4 kRangeEnd(0.42f, 0.50f, 0.98f, 1.00f);        // first and last chosen day
static const ImVec4 kRangeEndHover(0.52f, 0.60f, 1.00f, 1.00f);
static const ImVec4 kInRange(0.20f, 0.24f, 0.45f, 1.00f);         // days between them
static const ImVec4 kInRangeHover(0.28f, 0.33f, 0.60f, 1.00f);
static const ImVec4 kDayText(1.00f, 1.00f, 1.00f, 1.00f);
static const ImVec4 kOutsideDayText(0.58f, 0.60f, 0.66f, 0.35f);  // not in the 31-day history
static const ImVec2 kPickerCell(56.0f, 46.0f);
static const float kPickerGap = 6.0f;
static const float kPickerRounding = 6.0f;
static const float kPickerMonthGap = 40.0f;   // between months shown side by side
static const float kDateComboWidth = 260.0f;

// Draws one month as a Sun-Sat grid. Days inside the history window can be
// clicked; the rest are faded and can't be picked. The first and last day of
// the chosen range are bright, the days between are shaded. Returns true (and
// sets 'picked') when a day is clicked.
static bool DrawPickerMonth(int year, int month,
                            const std::map<int, const HistoryDay *> &windowDaysOfMonth,
                            const std::string &rangeFrom,
                            const std::string &rangeTo, const std::string &today,
                            std::string &picked)
{
    ImGui::BeginGroup();

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.15f);
    ImGui::TextUnformatted(MonthTitle(year, month).c_str());
    ImGui::PopFont();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const float stepX = kPickerCell.x + kPickerGap;
    const float stepY = kPickerCell.y + kPickerGap;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    // Weekday header row
    for (int col = 0; col < 7; ++col)
    {
        const ImVec2 size = ImGui::CalcTextSize(kWeekdayShort[col]);
        draw->AddText(ImVec2(origin.x + col * stepX + (kPickerCell.x - size.x) * 0.5f, origin.y),
                      ImGui::GetColorU32(ImGuiCol_TextDisabled), kWeekdayShort[col]);
    }
    const float headerHeight = ImGui::GetTextLineHeight() + 8.0f;

    const int firstWeekday = FirstWeekdayOfMonth(year, month);
    const int daysInMonth = DaysInMonth(year, month);
    const int rows = (firstWeekday + daysInMonth + 6) / 7;
    bool clicked = false;

    for (int dayOfMonth = 1; dayOfMonth <= daysInMonth; ++dayOfMonth)
    {
        const int slot = firstWeekday + dayOfMonth - 1;
        const ImVec2 pos(origin.x + (slot % 7) * stepX, origin.y + headerHeight + (slot / 7) * stepY);
        const ImVec2 end(pos.x + kPickerCell.x, pos.y + kPickerCell.y);

        char number[4];
        std::snprintf(number, sizeof(number), "%d", dayOfMonth);
        const ImVec2 textSize = ImGui::CalcTextSize(number);
        const ImVec2 textPos(pos.x + (kPickerCell.x - textSize.x) * 0.5f,
                             pos.y + (kPickerCell.y - textSize.y) * 0.5f);

        auto it = windowDaysOfMonth.find(dayOfMonth);
        if (it == windowDaysOfMonth.end())
        {
            draw->AddText(textPos, ImGui::GetColorU32(kOutsideDayText), number);
            continue;
        }
        const HistoryDay &day = *it->second;

        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(dayOfMonth);
        const bool pressed = ImGui::InvisibleButton("day", kPickerCell);
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const bool isRangeEnd = (day.date == rangeFrom || day.date == rangeTo);
        const bool inRange = (day.date > rangeFrom && day.date < rangeTo);
        const ImVec4 &bg = isRangeEnd ? (hovered ? kRangeEndHover : kRangeEnd)
                           : inRange  ? (hovered ? kInRangeHover : kInRange)
                                      : (hovered ? kToggleOffHover : kToggleOff);
        draw->AddRectFilled(pos, end, ImGui::GetColorU32(bg), kPickerRounding);
        if (day.date == today)
            draw->AddRect(pos, end, ImGui::GetColorU32(kTodayOutline), kPickerRounding, 0, 3.0f);
        draw->AddText(textPos, ImGui::GetColorU32(kDayText), number);

        if (pressed)
        {
            picked = day.date;
            clicked = true;
        }
    }

    // Reserve the grid's space so the layout continues after it
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(7 * stepX - kPickerGap, headerHeight + rows * stepY - kPickerGap));

    ImGui::EndGroup();
    return clicked;
}

// A date box that opens a calendar: every month the history window touches,
// side by side in the order they happen (e.g. October, then November).
// rangeFrom/rangeTo highlight the currently chosen range. Returns true if changed.
static bool DateCombo(const char *id, const std::vector<HistoryDay> &days, std::string &date,
                      const std::string &rangeFrom, const std::string &rangeTo, const char *hint)
{
    std::string preview = date;
    for (const HistoryDay &day : days)
        if (day.date == date)
            preview = ShortDateLabel(day);

    bool changed = false;
    ImGui::SetNextItemWidth(kDateComboWidth);
    // Let the popup grow to fit the calendar (a combo normally matches its
    // box's width and a list's height), but no bigger than the app window:
    // in a small window the calendar scrolls instead of being cut off.
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 safePadding = ImGui::GetStyle().DisplaySafeAreaPadding;
    const ImVec2 maxPopupSize(viewport->WorkSize.x - 2.0f * safePadding.x,
                              viewport->WorkSize.y - 2.0f * safePadding.y);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), maxPopupSize);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, kPickerBg); // read when the popup opens
    const bool open = ImGui::BeginCombo(id, preview.c_str());
    ImGui::PopStyleColor();
    if (!open)
        Hint(hint); // (not while the calendar itself is showing)
    if (open)
    {
        // Group the window's days by month; the key (year * 100 + month)
        // sorts chronologically, including December -> January.
        std::map<int, std::map<int, const HistoryDay *>> months;
        for (const HistoryDay &day : days)
            months[day.year * 100 + day.month][day.day] = &day;

        // Months side by side when the app window is wide enough for them,
        // otherwise one above the other.
        const ImGuiStyle &style = ImGui::GetStyle();
        const float monthWidth = 7.0f * (kPickerCell.x + kPickerGap) - kPickerGap;
        const float monthCount = static_cast<float>(months.size());
        const float rowOfMonthsWidth =
            monthCount * monthWidth + (monthCount - 1.0f) * kPickerMonthGap;
        // (room for a scrollbar too, if the window is too short to show it all)
        const float scrollbarWidth = (ImGui::GetScrollMaxY() > 0.0f) ? style.ScrollbarSize : 0.0f;
        const bool sideBySide =
            rowOfMonthsWidth + 2.0f * style.FramePadding.x + scrollbarWidth <= maxPopupSize.x;
        const float calendarsWidth = sideBySide ? rowOfMonthsWidth : monthWidth;

        const std::string &today = days.back().date;
        bool firstMonth = true;
        for (const auto &month : months)
        {
            if (!firstMonth)
            {
                if (sideBySide)
                    ImGui::SameLine(0.0f, kPickerMonthGap);
                else
                    ImGui::Spacing();
            }
            firstMonth = false;

            std::string picked;
            ImGui::PushID(month.first);
            if (DrawPickerMonth(month.first / 100, month.first % 100, month.second,
                                rangeFrom, rangeTo, today, picked))
            {
                date = picked;
                changed = true;
            }
            ImGui::PopID();
        }

        // Explanation under the calendars, wrapped to their width
        ImGui::Spacing();
        const float textRight = ImGui::GetCursorScreenPos().x + calendarsWidth;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + calendarsWidth);
        ImGui::TextColored(kMutedText, "Bright: first and last chosen day. Shaded: days between.");
        UiLayout::SameLineOrWrap(ImGui::CalcTextSize("Gold outline: today.").x, -1.0f, -1.0f,
                                 textRight);
        ImGui::TextColored(kTodayOutline, "Gold outline: today.");
        ImGui::TextColored(kMutedText, "Faded dates are outside the %d-day history.",
                           static_cast<int>(days.size()));
        ImGui::PopTextWrapPos();

        if (changed)
            ImGui::CloseCurrentPopup();
        ImGui::EndCombo();
    }
    return changed;
}

// Date range + days of the week. Shared by both tabs and the PDF export.
// (New filters get their controls here; see TrendFilter in trends.h.)
static void DrawFilterPanel(TrendsScreenState &state, const std::vector<HistoryDay> &days,
                            const FilteredBehaviors &data)
{
    TrendFilter &filter = state.options.filter;
    const float labelColumn = 150.0f;

    // In a narrow window, the controls of each row wrap onto extra rows
    // (lined up after the "Dates:" / "Days:" labels) and the panel grows
    // taller to fit them. It never gets narrower than its widest control
    // ("to" + a date box, after the label column); below that the whole
    // screen scrolls sideways instead.
    const ImGuiStyle &style = ImGui::GetStyle();
    const float minPanelWidth = labelColumn + ImGui::CalcTextSize("to").x + style.ItemSpacing.x +
                                kDateComboWidth + 2.0f * style.WindowPadding.x;
    const float panelWidth =
        (ImGui::GetContentRegionAvail().x < minPanelWidth) ? minPanelWidth : 0.0f; // 0 = full width
    ImGui::BeginChild("Filters", ImVec2(panelWidth, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    const ImVec2 resetButtonSize(230.0f, 0.0f);
    const float dayButtonWidth = 220.0f; // "All days" / "Clear days"

    // ---- Date range ----------------------------------------------------------
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Dates:");
    ImGui::SameLine(labelColumn);
    const std::string rangeFrom = filter.fromDate; // for shading both calendars
    const std::string rangeTo = filter.toDate;
    if (DateCombo("##from", days, filter.fromDate, rangeFrom, rangeTo,
                  "Pick the first day to include. Opens a calendar."))
    {
        state.presetDays = 0;
        if (filter.fromDate > filter.toDate) filter.toDate = filter.fromDate;
    }
    // "to" and the second date box stay together
    UiLayout::SameLineOrWrap(ImGui::CalcTextSize("to").x + style.ItemSpacing.x + kDateComboWidth,
                             -1.0f, labelColumn);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("to");
    ImGui::SameLine();
    if (DateCombo("##to", days, filter.toDate, rangeFrom, rangeTo,
                  "Pick the last day to include. Opens a calendar."))
    {
        state.presetDays = 0;
        if (filter.toDate < filter.fromDate) filter.fromDate = filter.toDate;
    }

    const float presetButtonWidth = 220.0f;
    UiLayout::SameLineOrWrap(SegmentedToggleWidth(3, presetButtonWidth), 40.0f, labelColumn);
    const int windowDays = static_cast<int>(days.size());
    const std::string allLabel = "All " + std::to_string(windowDays) + " days";
    int preset = state.presetDays;
    const std::string allHint = "Count everything stored: today and the " +
                                std::to_string(windowDays - 1) + " days before it.";
    SegmentedToggle<int>("presets",
                         {{"Last 7 days", 7, "Count only the last 7 days, including today."},
                          {"Last 14 days", 14, "Count only the last 14 days, including today."},
                          {allLabel.c_str(), windowDays, allHint.c_str()}},
                         preset, presetButtonWidth);
    if (preset != state.presetDays)
    {
        state.presetDays = preset;
        ApplyPreset(filter, days, preset);
    }

    // "Reset to Defaults" lives at the right edge of the Dates row, clear of
    // the preset buttons, and undoes every filter/sort choice on this screen.
    // (In a narrow window it goes on its own row, under the date boxes.)
    UiLayout::SameLineRightAlignedOrWrap(resetButtonSize.x, labelColumn);
    if (ImGui::Button("Reset to Defaults", resetButtonSize))
        ResetTrendsToDefaults(state, days);
    Hint("Undo every choice on this screen: all stored days, every day of the week, "
         "both lists most first, and Time of Day showing all choices by hour.");

    // ---- Days of the week ----------------------------------------------------
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Days:");
    ImGui::SameLine(labelColumn);
    for (int d = 0; d < 7; ++d)
    {
        if (d > 0)
            UiLayout::SameLineOrWrap(UiLayout::CheckboxWidth(kWeekdayShort[d]), 24.0f, labelColumn);
        static const char *kDayHints[7] = {
            "Include Sundays. Uncheck to leave them out.",
            "Include Mondays. Uncheck to leave them out.",
            "Include Tuesdays. Uncheck to leave them out.",
            "Include Wednesdays. Uncheck to leave them out.",
            "Include Thursdays. Uncheck to leave them out.",
            "Include Fridays. Uncheck to leave them out.",
            "Include Saturdays. Uncheck to leave them out."};
        bool on = filter.weekdays[d];
        if (ImGui::Checkbox(kWeekdayShort[d], &on))
            filter.weekdays[d] = on;
        Hint(kDayHints[d]);
    }
    // "All days" and "Clear days" move to the next row together if needed
    UiLayout::SameLineOrWrap(2.0f * dayButtonWidth + style.ItemSpacing.x, 40.0f, labelColumn);
    ImGui::BeginDisabled(filter.AllWeekdays());
    if (ImGui::Button("All days", ImVec2(dayButtonWidth, 0.0f)))
        filter.weekdays.fill(true);
    ImGui::EndDisabled();
    Hint(filter.AllWeekdays() ? "All seven days are already checked."
                              : "Check all seven days of the week.");

    UiLayout::SameLineOrWrap(dayButtonWidth, -1.0f, labelColumn);
    ImGui::BeginDisabled(filter.NoWeekdays());
    if (ImGui::Button("Clear days", ImVec2(dayButtonWidth, 0.0f)))
        filter.weekdays.fill(false);
    ImGui::EndDisabled();
    Hint(filter.NoWeekdays()
             ? "No days are checked. Check the days you want to see."
             : "Uncheck every day, so you can check just the one or two you want. "
               "At least one day must be checked to see trends.");

    // ---- What the counts below are based on ----------------------------------
    UiLayout::PushTextWrapAtVisibleEdge(); // wrap at the panel's edge in a narrow window
    if (filter.NoWeekdays())
    {
        ImGui::TextColored(kWarningText, "Pick at least one day of the week to see trends.");
    }
    else
    {
        const size_t n = data.entries.size();
        ImGui::TextColored(kMutedText, "Counting %zu logged behavior%s from %d day%s (%s).", n,
                           n == 1 ? "" : "s", data.daysCovered, data.daysCovered == 1 ? "" : "s",
                           DescribeWeekdays(filter.weekdays).c_str());
    }
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
}

// Header row drawn as plain text, like the Day Detail table (TableHeadersRow()
// cells are interactive sort/menu buttons, which we don't want here).
static void PlainHeaderRow(int columns)
{
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int column = 0; column < columns; ++column)
    {
        ImGui::TableSetColumnIndex(column);
        ImGui::TextUnformatted(ImGui::TableGetColumnName(column));
    }
}

static const ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

// Height of one table row.
static float TableRowHeight()
{
    return ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
}

// When the space for a table is narrower than it needs to show every column
// (fixedWidth for the fixed columns + the longest behavior name), turns on
// sideways scrolling and returns the width to lay the columns out in (the
// 'inner_width' of BeginTable). Otherwise the table is unchanged (returns 0).
static float ScrollXIfTooNarrow(ImGuiTableFlags &flags, int columns, float fixedWidth,
                                float nameWidth)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const float needed = fixedWidth + nameWidth +
                         static_cast<float>(columns) * 2.0f * style.CellPadding.x +
                         style.ScrollbarSize;
    if (ImGui::GetContentRegionAvail().x >= needed)
        return 0.0f;
    flags |= ImGuiTableFlags_ScrollX;
    return needed;
}

static const float kRankingToggleWidth = 220.0f; // "Most first" / "Least first"

// One column of the "Most / Least Logged" tab. height 0 = the rest of the window.
static void DrawRankingColumn(const char *id, const char *title, bool good, float width,
                              float height, const std::vector<BehaviorEntry> &entries,
                              const std::vector<BehaviorChoice> &choices, RankOrder &order)
{
    ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_Borders);

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    ImGui::TextColored(good ? kGoodText : kWrongText, "%s", title);
    ImGui::PopFont();

    SegmentedToggle<RankOrder>(
        "order",
        {{"Most first", RankOrder::MostFirst,
          good ? "List good choices from most logged to least logged."
               : "List wrong choices from most logged to least logged."},
         {"Least first", RankOrder::LeastFirst,
          good ? "List good choices from least logged to most logged. "
                 "Good choices never logged (0 times) come first."
               : "List wrong choices from least logged to most logged. "
                 "Wrong choices never logged (0 times) come first."}},
        order, kRankingToggleWidth);
    ImGui::Spacing();

    const std::vector<BehaviorCount> rows = RankBehaviors(entries, choices, good, order);
    if (rows.empty())
    {
        ImGui::TextColored(kMutedText, good ? "There are no good choices yet."
                                            : "There are no wrong choices yet.");
        ImGui::EndChild();
        return;
    }

    float nameWidth = ImGui::CalcTextSize("Behavior").x;
    for (const BehaviorCount &row : rows)
        nameWidth = std::max(nameWidth, ImGui::CalcTextSize(row.name.c_str()).x);
    ImGuiTableFlags tableFlags = kTableFlags;
    const float innerWidth = ScrollXIfTooNarrow(tableFlags, 2, 210.0f, nameWidth);

    if (ImGui::BeginTable("Ranking", 2, tableFlags, ImVec2(0.0f, 0.0f), innerWidth))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Behavior", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Times Logged", ImGuiTableColumnFlags_WidthFixed, 210.0f);
        PlainHeaderRow(2);

        int index = 0;
        for (const BehaviorCount &row : rows)
        {
            ImGui::TableNextRow();
            if (index++ % 2 == 1)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(kRowShade));

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(row.count == 0 ? kMutedText : (good ? kGoodText : kWrongText),
                               "%s", TimesLabel(row.count).c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

static void DrawMostLeastTab(const std::vector<BehaviorEntry> &entries,
                             const std::vector<BehaviorChoice> &choices,
                             TrendsReportOptions &options)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const float spacing = style.ItemSpacing.x;
    const float half = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

    // Smallest useful column: both sort buttons side by side, and the title,
    // sort buttons, table header and a few rows in view.
    const float minColumnWidth =
        SegmentedToggleWidth(2, kRankingToggleWidth) + 2.0f * style.WindowPadding.x;
    const float minColumnHeight = 2.0f * style.WindowPadding.y + ImGui::GetFontSize() * 1.3f +
                                  ImGui::GetFrameHeight() + 3.0f * style.ItemSpacing.y +
                                  TableRowHeight() * 6.0f;

    if (half >= minColumnWidth)
    {
        // Side by side, filling the rest of the window (or scrolling the
        // window when it is too short to show them properly)
        const float height = UiLayout::FillHeightAtLeast(minColumnHeight);
        DrawRankingColumn("GoodRanking", "Good Choices", true, half, height, entries, choices,
                          options.goodOrder);
        ImGui::SameLine();
        DrawRankingColumn("WrongRanking", "Wrong Choices", false, 0.0f, height, entries, choices,
                          options.wrongOrder);
    }
    else
    {
        // Narrow window: one above the other, full width, sharing the height
        float height = (ImGui::GetContentRegionAvail().y - style.ItemSpacing.y) * 0.5f;
        if (height < minColumnHeight) height = minColumnHeight;
        DrawRankingColumn("GoodRanking", "Good Choices", true, 0.0f, height, entries, choices,
                          options.goodOrder);
        DrawRankingColumn("WrongRanking", "Wrong Choices", false, 0.0f, height, entries, choices,
                          options.wrongOrder);
    }
}

static void DrawTimeOfDayTab(const std::vector<BehaviorEntry> &entries,
                             TrendsReportOptions &options)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Show:");
    ImGui::SameLine();
    SegmentedToggle<ChoiceKind>(
        "kind",
        {{"All choices", ChoiceKind::All, "Show good and wrong choices together."},
         {"Good only", ChoiceKind::Good, "Show only good choices."},
         {"Wrong only", ChoiceKind::Wrong, "Show only wrong choices."}},
        options.timeKind, 200.0f);

    // "Sort:" and its buttons move to the next row together in a narrow window
    const float sortButtonWidth = 260.0f;
    UiLayout::SameLineOrWrap(ImGui::CalcTextSize("Sort:").x + ImGui::GetStyle().ItemSpacing.x +
                                 SegmentedToggleWidth(2, sortButtonWidth),
                             50.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Sort:");
    ImGui::SameLine();
    SegmentedToggle<TimeOfDaySort>(
        "sort",
        {{"By time of day", TimeOfDaySort::ByHour,
          "List the hours in order, 12am to 11pm, most logged first within each hour."},
         {"By most logged", TimeOfDaySort::ByCount,
          "List the most logged behavior and hour first, across the whole day. "
          "Hours with nothing logged go last."}},
        options.timeSort, sortButtonWidth);

    UiLayout::PushTextWrapAtVisibleEdge(); // wrap at the window's edge in a narrow window
    ImGui::TextColored(kMutedText, "Each row is one hour of the day, added up across every "
                                   "included day.");
    if (options.timeKind == ChoiceKind::All)
    {
        UiLayout::SameLineOrWrap(ImGui::CalcTextSize("Green = good choice.").x);
        ImGui::TextColored(kGoodText, "Green = good choice.");
        UiLayout::SameLineOrWrap(ImGui::CalcTextSize("Red = wrong choice.").x);
        ImGui::TextColored(kWrongText, "Red = wrong choice.");
    }
    ImGui::PopTextWrapPos();

    const std::vector<HourRow> rows = TimeOfDayRows(entries, options.timeKind, options.timeSort);
    const std::string emptyText = EmptyHourLabel(options.timeKind);

    // Sideways scrolling in a narrow window; a minimum height in a short one
    // (the window scrolls instead of squashing the table)
    float nameWidth = std::max(ImGui::CalcTextSize("Behavior").x,
                               ImGui::CalcTextSize(emptyText.c_str()).x);
    for (const HourRow &row : rows)
        nameWidth = std::max(nameWidth, ImGui::CalcTextSize(row.name.c_str()).x);
    ImGuiTableFlags tableFlags = kTableFlags;
    const float innerWidth = ScrollXIfTooNarrow(tableFlags, 3, 220.0f + 210.0f, nameWidth);
    const ImVec2 tableSize(0.0f, UiLayout::FillHeightAtLeast(TableRowHeight() * 10.0f +
                                                             ImGui::GetStyle().ScrollbarSize));

    if (ImGui::BeginTable("TimeOfDay", 3, tableFlags, tableSize, innerWidth))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Behavior", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Times Logged", ImGuiTableColumnFlags_WidthFixed, 210.0f);
        PlainHeaderRow(3);

        // Sorted by hour: shade alternate hours so each hour reads as a group.
        // Sorted by count: shade alternate rows.
        int shade = 0;
        int previousHour = -1;
        for (const HourRow &row : rows)
        {
            if (options.timeSort == TimeOfDaySort::ByHour)
            {
                if (row.hour != previousHour && previousHour != -1)
                    ++shade;
                previousHour = row.hour;
            }

            ImGui::TableNextRow();
            if (shade % 2 == 1)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(kRowShade));
            if (options.timeSort == TimeOfDaySort::ByCount)
                ++shade;

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(HourBlockLabel(row.hour).c_str());

            ImGui::TableSetColumnIndex(1);
            if (row.IsEmptyHour())
            {
                ImGui::TextColored(kMutedText, "%s", emptyText.c_str());
                continue;
            }
            const ImVec4 &color = row.isGood ? kGoodText : kWrongText;
            ImGui::TextColored(color, "%s", row.name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(color, "%s", TimesLabel(row.count).c_str());
        }
        ImGui::EndTable();
    }
}

void DrawTrendsScreen(const BehaviorLog &log, const std::vector<BehaviorChoice> &choices,
                      Screen &screen)
{
    const std::time_t now = std::time(nullptr);
    const std::vector<HistoryDay> days = log.historyWindow(now);
    KeepRangeInWindow(g_trends, days);

    TrendsReportOptions &options = g_trends.options;
    const FilteredBehaviors data = ApplyTrendFilter(log, options.filter, now);
    const bool canShow = !options.filter.NoWeekdays();

    // ---- Title row: Back, title, and "Export PDF" at the top right ------------
    // In a narrow window the subtitle moves under the title row, so the
    // Export button can stay at the top right (or wraps too, if needed).
    if (ImGui::Button("< Back", ImVec2(200, 55)))
        screen = g_trends.returnTo;
    Hint(g_trends.returnTo == Screen::History ? "Go back to the History calendar."
                                              : "Go back to the main screen.");

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    const float titleWidth = ImGui::CalcTextSize("Trends").x;
    ImGui::PopFont();
    UiLayout::SameLineOrWrap(titleWidth, 30.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    ImGui::TextUnformatted("Trends");
    ImGui::PopFont();

    const char *subtitle = "  Spot patterns in logged behaviors";
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const bool subtitleOnTitleRow =
        ImGui::GetItemRectMax().x + itemSpacing + ImGui::CalcTextSize(subtitle).x + itemSpacing +
            kExportButtonSize.x <= UiLayout::RowRight();
    if (subtitleOnTitleRow)
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", subtitle);
    }

    UiLayout::SameLineRightAlignedOrWrap(kExportButtonSize.x);
    ImGui::PushStyleColor(ImGuiCol_Button, kExportButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kExportButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kExportButtonActive);
    ImGui::BeginDisabled(!canShow);
    if (ImGui::Button("Export PDF", kExportButtonSize))
        ShowPdfExportResult(ExportTrendsPdf(log, choices, options, now), "Your trends report");
    ImGui::EndDisabled();
    Hint(canShow ? "Save exactly what is on screen (this tab, your dates, days and sorting) "
                   "as a PDF in your Downloads folder, then open it."
                 : "Check at least one day of the week before exporting.");
    ImGui::PopStyleColor(3);

    DrawExportPopup();

    if (!subtitleOnTitleRow)
    {
        UiLayout::PushTextWrapAtVisibleEdge();
        ImGui::TextDisabled("Spot patterns in logged behaviors");
        ImGui::PopTextWrapPos();
    }

    ImGui::Separator();

    DrawFilterPanel(g_trends, days, data);
    ImGui::Spacing();

    if (!canShow)
        return;

    // ---- Tabs ------------------------------------------------------------------
    // Same on/off colours as the toggle buttons, so the open tab is obvious.
    // (Tab colours are read as each tab is drawn, so they stay pushed until
    // EndTabBar; they don't affect anything inside the tabs.)
    ImGui::PushStyleColor(ImGuiCol_Tab, kToggleOff);
    ImGui::PushStyleColor(ImGuiCol_TabDimmed, kToggleOff);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, kToggleOnHover);
    ImGui::PushStyleColor(ImGuiCol_TabSelected, kToggleOn);
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, kToggleOn);
    ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, kToggleOnHover);
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelectedOverline, kToggleOnHover);
    if (ImGui::BeginTabBar("TrendsTabs"))
    {
        // "Reset to Defaults" puts the tab bar back on "Most / Least Logged"
        // (it otherwise remembers whichever tab was open, like a real tab bar).
        const ImGuiTabItemFlags selectMostLeast =
            (g_trends.forceTabSelect && options.tab == TrendsTab::MostLeast)
                ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        const ImGuiTabItemFlags selectTimeOfDay =
            (g_trends.forceTabSelect && options.tab == TrendsTab::TimeOfDay)
                ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

        const bool mostLeastOpen =
            ImGui::BeginTabItem("  Most / Least Logged  ", nullptr, selectMostLeast);
        Hint("See which good and wrong choices were logged the most and the least.");
        if (mostLeastOpen)
        {
            options.tab = TrendsTab::MostLeast;
            ImGui::Spacing();
            DrawMostLeastTab(data.entries, choices, options);
            ImGui::EndTabItem();
        }
        const bool timeOfDayOpen =
            ImGui::BeginTabItem("  Time of Day  ", nullptr, selectTimeOfDay);
        Hint("See how many times each behavior was logged in each hour of the day.");
        if (timeOfDayOpen)
        {
            options.tab = TrendsTab::TimeOfDay;
            ImGui::Spacing();
            DrawTimeOfDayTab(data.entries, options);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        g_trends.forceTabSelect = false;
    }
    ImGui::PopStyleColor(7);
}