#include "history_view.h"

#include "history_export.h"
#include "imgui.h"
#include "localization/strings.h"
#include "trends_view.h"
#include "ui_layout.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// History screen: calendar grids (one per month) + legend + day detail
// ---------------------------------------------------------------------------

// Calendar colours (also used to draw the legend swatches, so they always match)
static const ImVec4 kDataCellBg(0.26f, 0.32f, 0.66f, 1.00f);        // day with behaviors
static const ImVec4 kDataCellBgHover(0.34f, 0.41f, 0.80f, 1.00f);
static const ImVec4 kNoDataCellBg(0.15f, 0.16f, 0.22f, 1.00f);      // grayed-out: no data
static const ImVec4 kNoDataCellBgHover(0.21f, 0.22f, 0.30f, 1.00f);
static const ImVec4 kNoDataText(0.58f, 0.60f, 0.66f, 1.00f);
static const ImVec4 kDataText(1.00f, 1.00f, 1.00f, 1.00f);
static const ImVec4 kTodayBorder(0.98f, 0.78f, 0.22f, 1.00f);       // today's outline
static const ImVec4 kBlankCellOutline(1.00f, 1.00f, 1.00f, 0.07f);  // outside the window
static const ImVec4 kPointsGained(0.35f, 0.85f, 0.55f, 1.00f);
static const ImVec4 kPointsLost(0.95f, 0.45f, 0.45f, 1.00f);

static const float kCellRounding = 8.0f;
static const float kCellGap = 8.0f;
static const float kLegendWidth = 470.0f;       // preferred legend width
static const float kLegendMinWidth = 360.0f;    // narrower than this, it moves below the calendar
static const float kMinCellWidth = 110.0f;
static const float kMaxCellWidth = 210.0f;

static const ImVec4 kExportButton(0.12f, 0.55f, 0.35f, 1.00f);      // green, like Good Choices
static const ImVec4 kExportButtonHover(0.18f, 0.70f, 0.45f, 1.00f);
static const ImVec4 kExportButtonActive(0.10f, 0.45f, 0.28f, 1.00f);
static const ImVec2 kExportButtonSize(260.0f, 55.0f);
// The popup's title is translated; the fixed "###..." part is its ID.
static std::string ExportPopupId()
{
    return std::string(Tr(TextId::ExportPopupTitle)) + "###ExportResultPopup";
}

// Outcome of the last "Export PDF" click, shown in the confirmation popup.
struct ExportStatus
{
    ExportResult result;
    bool openedInViewer = false;
    TextId savedMessage = TextId::ExportSavedHistory; // "... was saved to ..."
};
static ExportStatus g_lastExport;

// Set when leaving a day's detail screen, so the calendar comes back scrolled
// to where the parent left it instead of jumping to the newest days.
static bool g_returningFromDayDetail = false;

int DaysInMonth(int year, int month)
{
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return (month == 2 && leap) ? 29 : kDays[month - 1];
}

// Weekday (0 = Sunday) of the 1st of the given month.
int FirstWeekdayOfMonth(int year, int month)
{
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = 1;
    tm.tm_hour = 12; // noon avoids any DST edge cases
    tm.tm_isdst = -1;
    std::mktime(&tm);
    return tm.tm_wday;
}

std::string MonthTitle(int year, int month)
{
    return Format(Tr(TextId::MonthYearTitle), {MonthName(month), year});
}

WeekRowRange VisibleWeekRows(int year, int month, int firstDay, int lastDay)
{
    const int firstWeekday = FirstWeekdayOfMonth(year, month);
    WeekRowRange range;
    range.firstRow = (firstWeekday + firstDay - 1) / 7;
    range.lastRow = (firstWeekday + lastDay - 1) / 7;
    return range;
}

// Draws one clickable day cell. Returns true when clicked.
static bool DrawDayCell(const HistoryDay &day, bool isToday, ImVec2 pos, ImVec2 size)
{
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(day.date.c_str());
    const bool clicked = ImGui::InvisibleButton("day", size);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);

    const ImVec4 &bg = day.hasData
                           ? (hovered ? kDataCellBgHover : kDataCellBg)
                           : (hovered ? kNoDataCellBgHover : kNoDataCellBg);
    draw->AddRectFilled(pos, end, ImGui::GetColorU32(bg), kCellRounding);

    if (isToday)
        draw->AddRect(pos, end, ImGui::GetColorU32(kTodayBorder), kCellRounding, 0, 4.0f);

    const ImU32 textColor = ImGui::GetColorU32(day.hasData ? kDataText : kNoDataText);
    const float pad = 10.0f;
    const float lineHeight = ImGui::GetTextLineHeight();

    // Date number (top-left)
    const std::string dayNumber = std::to_string(day.day);
    draw->AddText(ImVec2(pos.x + pad, pos.y + pad), textColor, dayNumber.c_str());

    // Ending points, or an explicit "No data logged" so no date looks missing
    const std::string detail = day.hasData
                                   ? TrPlural(PluralId::HistoryCellEnding, day.endingPoints)
                                   : std::string(Tr(TextId::CommonNoDataLogged));

    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                  ImVec2(pos.x + pad, pos.y + pad + lineHeight + 6.0f),
                  textColor, detail.c_str(), nullptr, size.x - 2.0f * pad);

    return clicked;
}

// Draws a calendar grid (Sun-Sat) for one month. Dates outside the history
// window are left blank (faint outline, no number). Returns the date string
// of a clicked day, or an empty string.
static std::string DrawMonthGrid(int year, int month,
                                 const std::map<int, const HistoryDay *> &windowDaysOfMonth,
                                 const std::string &today, ImVec2 cellSize)
{
    std::string clickedDate;

    // Month title (larger text)
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    ImGui::TextUnformatted(MonthTitle(year, month).c_str());
    ImGui::PopFont();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const float stepX = cellSize.x + kCellGap;
    const float stepY = cellSize.y + kCellGap;

    // Weekday header row
    const ImVec2 headerOrigin = ImGui::GetCursorScreenPos();
    for (int col = 0; col < 7; ++col)
    {
        const char *weekday = WeekdayShortName(col);
        const ImVec2 textSize = ImGui::CalcTextSize(weekday);
        draw->AddText(ImVec2(headerOrigin.x + col * stepX + (cellSize.x - textSize.x) * 0.5f,
                             headerOrigin.y),
                      ImGui::GetColorU32(ImGuiCol_TextDisabled), weekday);
    }
    const float headerHeight = ImGui::GetTextLineHeight() + 8.0f;

    // Day cells. Only weeks (Sun-Sat rows) containing at least one day in
    // the history window are drawn; fully blank weeks are hidden. Within a
    // drawn week, days outside the window are blank cells.
    const ImVec2 gridOrigin(headerOrigin.x, headerOrigin.y + headerHeight);
    const int firstWeekday = FirstWeekdayOfMonth(year, month);
    const int daysInMonth = DaysInMonth(year, month);
    const WeekRowRange rows = VisibleWeekRows(year, month,
                                              windowDaysOfMonth.begin()->first,
                                              windowDaysOfMonth.rbegin()->first);

    for (int dayOfMonth = 1; dayOfMonth <= daysInMonth; ++dayOfMonth)
    {
        const int slot = firstWeekday + dayOfMonth - 1;
        const int row = slot / 7;
        if (row < rows.firstRow || row > rows.lastRow)
            continue; // week has no days in the history window

        const ImVec2 pos(gridOrigin.x + (slot % 7) * stepX,
                         gridOrigin.y + (row - rows.firstRow) * stepY);

        auto it = windowDaysOfMonth.find(dayOfMonth);
        if (it != windowDaysOfMonth.end())
        {
            const HistoryDay &day = *it->second;
            if (DrawDayCell(day, day.date == today, pos, cellSize))
                clickedDate = day.date;
        }
        else
        {
            // Outside the 30-day window: blank cell
            draw->AddRect(pos, ImVec2(pos.x + cellSize.x, pos.y + cellSize.y),
                          ImGui::GetColorU32(kBlankCellOutline), kCellRounding);
        }
    }

    // Reserve the space the grid occupies so layout continues below it
    ImGui::SetCursorScreenPos(headerOrigin);
    ImGui::Dummy(ImVec2(7 * stepX - kCellGap, headerHeight + rows.count() * stepY - kCellGap));
    ImGui::Spacing();
    ImGui::Spacing();

    return clickedDate;
}

// A small coloured square followed by wrapped description text.
static void DrawLegendItem(const ImVec4 &fill, const ImVec4 *border, const char *text)
{
    const ImVec2 swatch(40.0f, 30.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + swatch.x, pos.y + swatch.y);

    draw->AddRectFilled(pos, end, ImGui::GetColorU32(fill), 5.0f);
    if (border)
        draw->AddRect(pos, end, ImGui::GetColorU32(*border), 5.0f, 0, 3.0f);

    ImGui::Dummy(swatch);
    ImGui::SameLine();
    ImGui::TextWrapped("%s", text);
}

void ShowPdfExportResult(const ExportResult &result, TextId savedMessage)
{
    g_lastExport.result = result;
    g_lastExport.savedMessage = savedMessage;
    g_lastExport.openedInViewer = result.success && OpenWithDefaultApp(result.path);
    ImGui::OpenPopup(ExportPopupId().c_str());
}

// Confirmation (or error) shown after clicking "Export PDF".
void DrawExportPopup()
{
    // 950 wide, or narrower so it always fits inside the app window.
    ui::CenterNextPopup();
    ImGui::SetNextWindowSize(ImVec2(ui::FitPopupWidth(950.0f), 0.0f));

    if (!ImGui::BeginPopupModal(ExportPopupId().c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return;

    const ExportResult &result = g_lastExport.result;
    if (result.success)
    {
        ImGui::TextColored(kPointsGained, "%s", Tr(TextId::ExportComplete));
        ImGui::Spacing();
        ImGui::TextWrapped("%s", Tr(g_lastExport.savedMessage));
        ImGui::TextWrapped("%s", result.path.u8string().c_str());
        ImGui::Spacing();
        if (g_lastExport.openedInViewer)
            ImGui::TextWrapped("%s", Tr(TextId::ExportOpenedInViewer));
        else
            ImGui::TextWrapped("%s", Tr(TextId::ExportNotOpened));
    }
    else
    {
        ImGui::TextColored(kPointsLost, "%s", Tr(TextId::ExportFailed));
        ImGui::Spacing();
        ImGui::TextWrapped("%s", result.error.c_str());
    }

    ImGui::Spacing();
    if (ImGui::Button(Tr(TextId::ExportOkButton), ImVec2(200, 55)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

static void DrawLegend()
{
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.15f);
    ImGui::TextUnformatted(Tr(TextId::HistoryLegendTitle));
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    DrawLegendItem(kDataCellBg, nullptr, Tr(TextId::HistoryLegendLogged));
    // Quotes the cells' own "No data logged" text, so the two always match.
    const std::string noDataText =
        Format(Tr(TextId::HistoryLegendNoData), {Tr(TextId::CommonNoDataLogged)});
    DrawLegendItem(kNoDataCellBg, nullptr, noDataText.c_str());
    DrawLegendItem(kNoDataCellBg, &kTodayBorder, Tr(TextId::CommonGoldOutlineToday));
    DrawLegendItem(ImVec4(0, 0, 0, 0), &kBlankCellOutline, Tr(TextId::HistoryLegendBlank));

    ImGui::TextWrapped("%s", Tr(TextId::HistoryLegendDetailIntro));
    ImGui::TextColored(kPointsGained, "  +5"); // sample numbers, not text
    ImGui::SameLine();
    ImGui::TextWrapped("%s", Tr(TextId::HistoryLegendPointsEarned));
    ImGui::TextColored(kPointsLost, "  -3");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", Tr(TextId::HistoryLegendPointsLost));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", Tr(TextId::HistoryLegendClickDay));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kTodayBorder);
    ImGui::TextWrapped("%s", Tr(TextId::HistoryLegendRetentionNote));
    ImGui::PopStyleColor();
}

// Calendar view: month grids (oldest month first) with a legend on the right.
void DrawHistoryScreen(BehaviorLog &log, Screen &screen, std::string &selectedDate)
{
    // Title row, with the green "Export PDF" button at the top right. When the
    // window is narrow, the points move below the title (so the button can
    // stay at the top right); if even that doesn't fit, the button wraps.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Tr(TextId::HistoryTitle));
    const std::string pointsString =
        Format(Tr(TextId::CommonCurrentPoints), {log.currentPoints()});
    const char *pointsText = pointsString.c_str();
    const float pointsInlineWidth = ImGui::CalcTextSize("   ").x + ImGui::CalcTextSize(pointsText).x;
    const bool pointsInline =
        ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x * 2.0f + pointsInlineWidth +
            kExportButtonSize.x <= ui::ContentRight();
    if (pointsInline)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("   %s", pointsText);
    }

    ui::SameLineRightAligned(kExportButtonSize.x);
    ImGui::PushStyleColor(ImGuiCol_Button, kExportButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kExportButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kExportButtonActive);
    if (ImGui::Button(Tr(TextId::CommonExportPdfButton), kExportButtonSize))
        ShowPdfExportResult(ExportHistoryPdf(log), TextId::ExportSavedHistory);
    ImGui::PopStyleColor(3);

    if (!pointsInline)
        ImGui::TextDisabled("%s", pointsText);

    DrawExportPopup();

    if (ImGui::Button(Tr(TextId::CommonBackButton), ImVec2(200, 55)))
    {
        screen = Screen::Main;
    }
    ImGui::SameLine();
    if (ImGui::Button(Tr(TextId::CommonTrendsButton), ImVec2(200, 55)))
        OpenTrendsScreen(screen);

    ImGui::Separator();

    const std::time_t now = std::time(nullptr);
    const std::string today = BehaviorLog::formatDate(now);
    const std::vector<HistoryDay> days = log.historyWindow(now);

    // Group the window's days by month: key = year * 100 + month
    std::map<int, std::map<int, const HistoryDay *>> months;
    for (const HistoryDay &day : days)
        months[day.year * 100 + day.month][day.day] = &day;

    // Size the cells to fit 7 columns beside the legend. As the window narrows
    // the legend shrinks a little, then moves below the calendar so the
    // calendar keeps the full width.
    const ImGuiStyle &style = ImGui::GetStyle();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float gridSlack = style.ScrollbarSize + style.WindowPadding.x;
    const float minCalendarWidth = 7.0f * kMinCellWidth + 6.0f * kCellGap + gridSlack;

    float legendWidth = kLegendWidth;
    float calendarWidth = availableWidth - legendWidth - style.ItemSpacing.x;
    if (calendarWidth < minCalendarWidth)
    {
        legendWidth = std::max(kLegendMinWidth,
                               availableWidth - minCalendarWidth - style.ItemSpacing.x);
        calendarWidth = availableWidth - legendWidth - style.ItemSpacing.x;
    }
    const bool legendBeside = calendarWidth >= minCalendarWidth;
    if (!legendBeside)
        calendarWidth = availableWidth;

    const float gridWidth = calendarWidth - gridSlack;
    float cellWidth = (gridWidth - 6.0f * kCellGap) / 7.0f;
    if (cellWidth > kMaxCellWidth) cellWidth = kMaxCellWidth;
    if (cellWidth < kMinCellWidth) cellWidth = kMinCellWidth;
    const float cellHeight = 20.0f + ImGui::GetTextLineHeight() * 3.0f + 6.0f;

    // Fills the rest of the screen; on a very short window it keeps about two
    // weeks visible and the screen scrolls instead.
    const float minCalendarHeight = cellHeight * 2.0f + ImGui::GetTextLineHeightWithSpacing() * 2.0f;
    const float calendarHeight =
        (ImGui::GetContentRegionAvail().y < minCalendarHeight) ? minCalendarHeight : 0.0f;

    // Scrolls sideways too, in case the window is narrower than the smallest
    // readable calendar.
    ImGui::BeginChild("Calendar", ImVec2(calendarWidth, calendarHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Months in the order they happen (oldest first), matching the Trends
    // date pickers. The year * 100 + month key keeps December before January.
    for (auto it = months.begin(); it != months.end(); ++it)
    {
        const int year = it->first / 100;
        const int month = it->first % 100;

        ImGui::PushID(it->first);
        const std::string clicked = DrawMonthGrid(year, month, it->second, today,
                                                  ImVec2(cellWidth, cellHeight));
        ImGui::PopID();

        if (!clicked.empty())
        {
            selectedDate = clicked;
            screen = Screen::DayDetail;
        }
    }

    // Months run oldest to newest, so when the calendar is taller than the
    // screen, open it scrolled to the bottom: today and the latest days are
    // what parents usually want first. (No effect when everything fits.)
    if (ImGui::IsWindowAppearing())
    {
        if (!g_returningFromDayDetail)
            ImGui::SetScrollHereY(1.0f);
        g_returningFromDayDetail = false;
    }

    if (!legendBeside)
    {
        // Narrow window: the legend follows the last month, in the same scroll.
        ImGui::BeginChild("Legend", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        DrawLegend();
        ImGui::EndChild();
    }

    ImGui::EndChild();

    if (legendBeside)
    {
        ImGui::SameLine();

        ImGui::BeginChild("Legend", ImVec2(0, calendarHeight), ImGuiChildFlags_Borders);
        DrawLegend();
        ImGui::EndChild();
    }
}

// Full-screen view of every behavior logged on one day.
void DrawDayDetailScreen(BehaviorLog &log, Screen &screen, const std::string &selectedDate)
{
    if (ImGui::Button(Tr(TextId::DayDetailBackButton), ImVec2(360, 55)))
    {
        screen = Screen::History;
        g_returningFromDayDetail = true;
    }

    ImGui::Separator();

    const std::time_t now = std::time(nullptr);
    const std::vector<HistoryDay> days = log.historyWindow(now);

    const HistoryDay *day = nullptr;
    for (const HistoryDay &d : days)
        if (d.date == selectedDate)
            day = &d;

    if (day == nullptr)
    {
        // Can happen if midnight passes while this screen is open
        ImGui::TextWrapped("%s", Tr(TextId::DayDetailRemoved));
        return;
    }

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    const std::string dayTitle = BehaviorLog::formatFriendlyDate(day->midnight);
    if (day->date == BehaviorLog::formatDate(now))
        ImGui::TextUnformatted(Format(Tr(TextId::DayDetailTitleToday), {dayTitle}).c_str());
    else
        ImGui::TextUnformatted(dayTitle.c_str());
    ImGui::PopFont();

    if (!day->hasData)
    {
        ImGui::Spacing();
        ImGui::TextColored(kNoDataText, "%s", Tr(TextId::DayDetailNoBehaviors));
        return;
    }

    ImGui::TextUnformatted(Format(Tr(TextId::DayDetailEndingPoints), {day->endingPoints}).c_str());
    ImGui::TextDisabled("%s", TrPlural(PluralId::DayDetailBehaviorsLogged,
                                       static_cast<long long>(day->entries.size())).c_str());
    ImGui::Spacing();

    // Thin lines and a small gap between columns keep them (and their
    // headers) visibly apart, even when a narrow window packs them tightly.
    ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ui::kTableColumnDividers |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_ScrollY;

    // When the window is too narrow for the usual column widths, the Time,
    // Points and Total After columns shrink to fit their contents; if the
    // longest behavior still doesn't fit, the table scrolls sideways instead
    // of cutting the names off.
    const ImGuiStyle &tableStyle = ImGui::GetStyle();
    const char *timeHeader = Tr(TextId::ColumnTime);
    const char *behaviorHeader = Tr(TextId::ColumnBehavior);
    const char *pointsHeader = Tr(TextId::ColumnPoints);
    const char *totalHeader = Tr(TextId::ColumnTotalAfter);
    float behaviorWidth = ImGui::CalcTextSize(behaviorHeader).x;
    float timeWidth = ImGui::CalcTextSize(timeHeader).x;
    float pointsWidth = ImGui::CalcTextSize(pointsHeader).x;
    float totalWidth = ImGui::CalcTextSize(totalHeader).x;
    char number[32];
    for (const BehaviorEntry &entry : day->entries)
    {
        behaviorWidth = std::max(behaviorWidth, ImGui::CalcTextSize(entry.label.c_str()).x);
        timeWidth = std::max(timeWidth, ImGui::CalcTextSize(
            BehaviorLog::formatFriendlyTime(entry.timestamp).c_str()).x);
        std::snprintf(number, sizeof(number), "%+d", entry.delta);
        pointsWidth = std::max(pointsWidth, ImGui::CalcTextSize(number).x);
        std::snprintf(number, sizeof(number), "%d", entry.pointsAfter);
        totalWidth = std::max(totalWidth, ImGui::CalcTextSize(number).x);
    }
    // Height and scrolling first (a vertical scrollbar takes width from the
    // columns), then the column widths for the width that's left.
    const float minTableHeight =
        ui::TableRowsHeight(4) + tableStyle.ScrollbarSize; // header + 4 rows
    ui::TableLayout layout = ui::PlanTable(day->entries.size(), minTableHeight);

    const float padding = ui::TableColumnExtraWidth() * 4.0f; // 4 columns
    if (170.0f + behaviorWidth + 120.0f + 180.0f + padding <= layout.room)
    {
        timeWidth = 170.0f; // the usual widths
        pointsWidth = 120.0f;
        totalWidth = 180.0f;
    }
    ui::FinishTable(layout, timeWidth + behaviorWidth + pointsWidth + totalWidth + padding);
    tableFlags |= layout.flags;

    ui::PushTableSpacing();
    if (ImGui::BeginTable("DayEntries", 4, tableFlags, ImVec2(0.0f, layout.height),
                          layout.innerWidth))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // keep the header visible
        ImGui::TableSetupColumn(timeHeader, ImGuiTableColumnFlags_WidthFixed, timeWidth);
        ImGui::TableSetupColumn(behaviorHeader, ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(pointsHeader, ImGuiTableColumnFlags_WidthFixed, pointsWidth);
        ImGui::TableSetupColumn(totalHeader, ImGuiTableColumnFlags_WidthFixed, totalWidth);
        // Header row drawn as plain text (not TableHeadersRow(), whose cells
        // are interactive sort/menu buttons). Keeps the header background.
        // Times (and their header) are right-aligned so they line up on the
        // right ("12:56 PM" over "1:05 AM"), clear of the behavior beside them.
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int column = 0; column < 4; ++column)
        {
            ImGui::TableSetColumnIndex(column);
            if (column == 0)
                ui::TextRightAligned(ImGui::TableGetColumnName(column));
            else
                ImGui::TextUnformatted(ImGui::TableGetColumnName(column));
        }

        for (const BehaviorEntry &entry : day->entries)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ui::TextRightAligned(BehaviorLog::formatFriendlyTime(entry.timestamp).c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.label.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(entry.delta >= 0 ? kPointsGained : kPointsLost,
                               "%+d", entry.delta);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", entry.pointsAfter);
        }

        ui::EndTableRows();
        ImGui::EndTable();
    }
    ui::PopTableSpacing();
}
