#include "trends_view.h"

#include "trends_export.h"
#include "imgui.h"

#include <ctime>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Trends screen
// ---------------------------------------------------------------------------

static const ImVec4 kGoodText(0.35f, 0.85f, 0.55f, 1.00f);   // same as points gained
static const ImVec4 kWrongText(0.95f, 0.45f, 0.45f, 1.00f);  // same as points lost
static const ImVec4 kMutedText(0.58f, 0.60f, 0.66f, 1.00f);
static const ImVec4 kWarningText(0.98f, 0.78f, 0.22f, 1.00f);
static const ImVec4 kHourBandBg(1.00f, 1.00f, 1.00f, 0.05f);

static const ImVec4 kExportButton(0.12f, 0.55f, 0.35f, 1.00f);
static const ImVec4 kExportButtonHover(0.18f, 0.70f, 0.45f, 1.00f);
static const ImVec4 kExportButtonActive(0.10f, 0.45f, 0.28f, 1.00f);
static const ImVec2 kExportButtonSize(260.0f, 55.0f);
static const char *kExportPopupId = "PDF Export###TrendsExportResultPopup";

static const float kDateComboWidth = 400.0f;

static const char *const kWeekdayShort[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Everything the Trends screen remembers while the app is open.
struct TrendsViewState
{
    TrendsSettings settings;
    // false: the date range follows the whole stored history (it moves
    // forward at midnight). true: the parent picked specific dates.
    bool customRange = false;
    Screen returnTo = Screen::Main;

    ExportResult lastExport;
    bool openedInViewer = false;
};
static TrendsViewState g_state;

// The log's entries with their local date/weekday/hour already worked out,
// plus the stored days. Rebuilt only when the log or today's date changes;
// filtering and counting this list every frame is cheap.
struct PreparedTrendData
{
    bool valid = false;
    size_t entryCount = 0;
    std::time_t firstTimestamp = 0;
    std::time_t lastTimestamp = 0;
    std::string today;

    std::vector<TimedEntry> entries;
    std::vector<HistoryDay> window;
};
static PreparedTrendData g_data;

static const PreparedTrendData &PrepareTrendData(const BehaviorLog &log, std::time_t now)
{
    // Every change to the log alters the entry count (a click adds one,
    // midnight pruning removes the oldest); first/last times are a backstop.
    const std::vector<BehaviorEntry> &entries = log.entries();
    const std::string today = BehaviorLog::formatDate(now);
    const std::time_t first = entries.empty() ? 0 : entries.front().timestamp;
    const std::time_t last = entries.empty() ? 0 : entries.back().timestamp;

    if (g_data.valid && g_data.entryCount == entries.size() && g_data.firstTimestamp == first &&
        g_data.lastTimestamp == last && g_data.today == today)
        return g_data;

    g_data.valid = true;
    g_data.entryCount = entries.size();
    g_data.firstTimestamp = first;
    g_data.lastTimestamp = last;
    g_data.today = today;
    g_data.entries = PrepareTrendEntries(entries);
    g_data.window = log.historyWindow(now);
    return g_data;
}

void OpenTrendsScreen(Screen &screen, Screen returnTo)
{
    g_state.returnTo = returnTo;
    screen = Screen::Trends;
}

// "Sat, Oct 10, 2026"
static std::string ShortDate(std::time_t t)
{
    const std::tm tm = BehaviorLog::toLocalTime(t);
    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), "%a, %b ", &tm) == 0)
        return BehaviorLog::formatDate(t);
    return std::string(buffer) + std::to_string(tm.tm_mday) + ", " +
           std::to_string(tm.tm_year + 1900);
}

static std::string DayOptionLabel(const HistoryDay &day, const std::string &today)
{
    return ShortDate(day.midnight) + (day.date == today ? "  (Today)" : "");
}

// Keeps the date range inside the stored history (it shrinks at midnight).
static void SyncDateRange(const std::vector<HistoryDay> &window)
{
    TrendFilter &filter = g_state.settings.filter;
    const std::string &first = window.front().date;
    const std::string &last = window.back().date;

    if (!g_state.customRange)
    {
        filter.fromDate = first;
        filter.toDate = last;
        return;
    }
    if (filter.fromDate < first) filter.fromDate = first;
    if (filter.fromDate > last) filter.fromDate = last;
    if (filter.toDate > last) filter.toDate = last;
    if (filter.toDate < filter.fromDate) filter.toDate = filter.fromDate;
}

// One "From"/"To" dropdown listing every stored day. Returns true if changed.
static bool DateCombo(const char *id, std::string &date, const std::vector<HistoryDay> &window,
                      const std::string &today)
{
    std::string preview = date;
    for (const HistoryDay &day : window)
        if (day.date == date)
            preview = DayOptionLabel(day, today);

    bool changed = false;
    ImGui::SetNextItemWidth(kDateComboWidth);
    if (ImGui::BeginCombo(id, preview.c_str(), ImGuiComboFlags_HeightLarge))
    {
        for (const HistoryDay &day : window)
        {
            const bool selected = (day.date == date);
            if (ImGui::Selectable(DayOptionLabel(day, today).c_str(), selected))
            {
                date = day.date;
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// Filters shared by both tabs. To add a filter, see the steps in
// behavior_trends.h (TrendFilter) and add its control here.
static void DrawTrendFilters(const std::vector<TimedEntry> &entries,
                             const std::vector<HistoryDay> &window, const std::string &today)
{
    TrendFilter &filter = g_state.settings.filter;

    ImGui::BeginChild("TrendFilters", ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    // ---- Date range (up to the 31 stored days) ----
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Dates:  From");
    ImGui::SameLine();
    if (DateCombo("##from", filter.fromDate, window, today))
    {
        g_state.customRange = true;
        if (filter.toDate < filter.fromDate)
            filter.toDate = filter.fromDate;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("to");
    ImGui::SameLine();
    if (DateCombo("##to", filter.toDate, window, today))
    {
        g_state.customRange = true;
        if (filter.fromDate > filter.toDate)
            filter.fromDate = filter.toDate;
    }
    if (g_state.customRange && filter.fromDate == window.front().date &&
        filter.toDate == window.back().date)
        g_state.customRange = false; // picked the whole history again: keep it current

    ImGui::SameLine(0.0f, 30.0f);
    if (ImGui::Button("Reset Dates & Days"))
    {
        g_state.customRange = false;
        filter.weekdays = TrendFilter().weekdays;
        SyncDateRange(window);
    }

    // ---- Days of the week ----
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Days of week:");
    for (int d = 0; d < 7; ++d)
    {
        ImGui::SameLine(0.0f, d == 0 ? 20.0f : 26.0f);
        ImGui::Checkbox(kWeekdayShort[d], &filter.weekdays[d]);
    }
    ImGui::SameLine(0.0f, 40.0f);
    if (ImGui::Button("All##weekdays"))
        filter.weekdays.fill(true);
    ImGui::SameLine();
    if (ImGui::Button("None##weekdays"))
        filter.weekdays.fill(false);

    // ---- Summary of what the filters include ----
    if (!filter.anyWeekdaySelected())
    {
        ImGui::TextColored(kWarningText, "Select at least one day of the week to see data.");
    }
    else
    {
        const int days = CountDaysIncluded(window, filter);
        const int behaviors = CountEntriesIncluded(entries, filter);
        ImGui::TextColored(kMutedText, "Including %d day%s (%s) and %d logged behavior%s.",
                           days, days == 1 ? "" : "s",
                           DescribeWeekdays(filter.weekdays).c_str(),
                           behaviors, behaviors == 1 ? "" : "s");
    }

    ImGui::EndChild();
}

// Text-only header row (like the Day Detail table: not clickable).
static void PlainHeaderRow(int columns)
{
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int column = 0; column < columns; ++column)
    {
        ImGui::TableSetColumnIndex(column);
        ImGui::TextUnformatted(ImGui::TableGetColumnName(column));
    }
}

// A behavior name in a table cell. If it is too long for the column and gets
// cut off, hovering over it shows the full name.
static void BehaviorNameCell(const std::string &name, const ImVec4 &color)
{
    const bool clipped = ImGui::CalcTextSize(name.c_str()).x > ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(color, "%s", name.c_str());
    if (clipped && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", name.c_str());
}

// One side of the Choice Rankings tab.
static void DrawRankingColumn(const char *id, const char *title, const ImVec4 &titleColor,
                              RankOrder &order, const std::vector<BehaviorCount> &counts)
{
    ImGui::PushID(id);

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
    ImGui::TextColored(titleColor, "%s", title);
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Sort:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Most logged", order == RankOrder::MostFirst))
        order = RankOrder::MostFirst;
    ImGui::SameLine(0.0f, 30.0f);
    if (ImGui::RadioButton("Least logged", order == RankOrder::LeastFirst))
        order = RankOrder::LeastFirst;

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
    if (counts.empty())
    {
        ImGui::TextColored(kMutedText, "There are no choices of this type.");
    }
    else if (ImGui::BeginTable("Ranking", 2, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Behavior", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Times Logged", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        PlainHeaderRow(2);

        for (const BehaviorCount &item : counts)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            BehaviorNameCell(item.label, item.count == 0 ? kMutedText : titleColor);
            ImGui::TableSetColumnIndex(1);
            const std::string times = TimesLabel(item.count);
            if (item.count == 0)
                ImGui::TextColored(kMutedText, "%s", times.c_str());
            else
                ImGui::TextUnformatted(times.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::PopID();
}

static void DrawRankingsTab(const std::vector<TimedEntry> &entries,
                            const std::vector<ChoiceInfo> &knownChoices)
{
    TrendsSettings &s = g_state.settings;
    const ChoiceRankings rankings =
        BuildChoiceRankings(entries, knownChoices, s.filter, s.goodOrder, s.wrongOrder);

    ImGui::TextColored(kMutedText, "Each choice, combined by name, and how many times it was "
                                   "logged. Ties are alphabetical. Choices never logged show 0.");

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float half = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;

    ImGui::BeginChild("GoodRanking", ImVec2(half, 0), ImGuiChildFlags_Borders);
    DrawRankingColumn("good", "Good Choices", kGoodText, s.goodOrder, rankings.good);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("WrongRanking", ImVec2(0, 0), ImGuiChildFlags_Borders);
    DrawRankingColumn("wrong", "Wrong Choices", kWrongText, s.wrongOrder, rankings.wrong);
    ImGui::EndChild();
}

static void DrawTimeOfDayTab(const std::vector<TimedEntry> &entries)
{
    TrendsSettings &s = g_state.settings;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Show:");
    ImGui::SameLine();
    if (ImGui::RadioButton("All", s.show == ChoiceTypeFilter::All))
        s.show = ChoiceTypeFilter::All;
    ImGui::SameLine(0.0f, 30.0f);
    if (ImGui::RadioButton("Good only", s.show == ChoiceTypeFilter::GoodOnly))
        s.show = ChoiceTypeFilter::GoodOnly;
    ImGui::SameLine(0.0f, 30.0f);
    if (ImGui::RadioButton("Wrong only", s.show == ChoiceTypeFilter::WrongOnly))
        s.show = ChoiceTypeFilter::WrongOnly;

    ImGui::SameLine(0.0f, 80.0f);
    ImGui::TextUnformatted("Sort:");
    ImGui::SameLine();
    if (ImGui::RadioButton("By hour of day", s.sort == TimeOfDaySort::ByHour))
        s.sort = TimeOfDaySort::ByHour;
    ImGui::SameLine(0.0f, 30.0f);
    if (ImGui::RadioButton("Most logged", s.sort == TimeOfDaySort::MostLogged))
        s.sort = TimeOfDaySort::MostLogged;

    const std::vector<TimeOfDayRow> rows = BuildTimeOfDay(entries, s.filter, s.show, s.sort);
    const int total = TotalLogged(rows);

    ImGui::TextColored(kMutedText, "How many times each behavior was logged in each hour, "
                                   "added up across all included days. Green = good choice, "
                                   "red = wrong choice. This table: %d behavior%s.",
                       total, total == 1 ? "" : "s");
    const bool byHour = (s.sort == TimeOfDaySort::ByHour);

    // By hour: shade alternate hour blocks so an hour's rows read as a group.
    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter;
    if (!byHour)
        flags |= ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("TimeOfDay", 3, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time of Day", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("Behavior", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Times Logged", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        PlainHeaderRow(3);

        int band = 0;
        int previousHour = -1;
        for (const TimeOfDayRow &row : rows)
        {
            ImGui::TableNextRow();
            if (byHour)
            {
                if (row.hour != previousHour && previousHour != -1)
                    ++band;
                previousHour = row.hour;
                if (band % 2 == 1)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                           ImGui::GetColorU32(kHourBandBg));
            }

            const std::string hour = HourRangeLabel(row.hour);
            ImGui::TableSetColumnIndex(0);
            if (row.isEmptyHour())
                ImGui::TextColored(kMutedText, "%s", hour.c_str());
            else
                ImGui::TextUnformatted(hour.c_str());

            ImGui::TableSetColumnIndex(1);
            if (row.isEmptyHour())
            {
                ImGui::TextColored(kMutedText, "%s", NoBehaviorsLoggedText().c_str());
                continue;
            }
            BehaviorNameCell(row.label, row.isGood ? kGoodText : kWrongText);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(TimesLabel(row.count).c_str());
        }
        ImGui::EndTable();
    }
}

// Confirmation (or error) shown after clicking "Export PDF".
static void DrawExportPopup()
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(950.0f, 0.0f));

    if (!ImGui::BeginPopupModal(kExportPopupId, nullptr,
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return;

    const ExportResult &result = g_state.lastExport;
    if (result.success)
    {
        ImGui::TextColored(kGoodText, "Export complete");
        ImGui::Spacing();
        ImGui::TextWrapped("This trends view was saved to your Downloads folder:");
        ImGui::TextWrapped("%s", result.path.u8string().c_str());
        ImGui::Spacing();
        if (g_state.openedInViewer)
            ImGui::TextWrapped("It has been opened in your default PDF viewer.");
        else
            ImGui::TextWrapped("It could not be opened automatically. You can open it "
                               "from your Downloads folder.");
    }
    else
    {
        ImGui::TextColored(kWrongText, "Export failed");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", result.error.c_str());
    }

    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(200, 55)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void DrawTrendsScreen(const BehaviorLog &log, const std::vector<ChoiceInfo> &knownChoices,
                      Screen &screen)
{
    const std::time_t now = std::time(nullptr);
    const PreparedTrendData &data = PrepareTrendData(log, now);
    const std::vector<HistoryDay> &window = data.window;
    const std::string &today = data.today;
    SyncDateRange(window);

    // ---- Title row: Back, title, Export PDF (top right) ----
    const float rowStartX = ImGui::GetCursorPosX();
    const float rowWidth = ImGui::GetContentRegionAvail().x;

    if (ImGui::Button("< Back", ImVec2(200, 55)))
        screen = g_state.returnTo;

    ImGui::SameLine(0.0f, 30.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
    ImGui::TextUnformatted("Trends");
    ImGui::PopFont();

    ImGui::SameLine(rowStartX + rowWidth - kExportButtonSize.x);
    ImGui::PushStyleColor(ImGuiCol_Button, kExportButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kExportButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kExportButtonActive);
    if (ImGui::Button("Export PDF", kExportButtonSize))
    {
        g_state.lastExport = ExportTrendsPdf(log, knownChoices, g_state.settings, now);
        g_state.openedInViewer = g_state.lastExport.success &&
                                 OpenWithDefaultApp(g_state.lastExport.path);
        ImGui::OpenPopup(kExportPopupId);
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Saves the tab you are viewing, with its filters and sort, as a PDF.");

    DrawExportPopup();

    ImGui::Separator();

    DrawTrendFilters(data.entries, window, today);

    ImGui::Spacing();

    if (ImGui::BeginTabBar("TrendsTabs"))
    {
        if (ImGui::BeginTabItem("Choice Rankings"))
        {
            g_state.settings.tab = TrendsTab::Rankings;
            DrawRankingsTab(data.entries, knownChoices);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Time of Day"))
        {
            g_state.settings.tab = TrendsTab::TimeOfDay;
            DrawTimeOfDayTab(data.entries);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
