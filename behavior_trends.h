#pragma once

#include <array>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "behavior_log.h"

// ---------------------------------------------------------------------------
// Trends: counting and sorting logged behaviors so parents can spot patterns.
//
// This file has no UI or PDF code. The Trends screen (trends_view.cpp) and
// the Trends PDF (trends_export.cpp) both call the same Build...() functions
// below, so the screen and the exported PDF always show the same results.
// ---------------------------------------------------------------------------

// A logged behavior is a good choice if its points are positive and a wrong
// choice if they are negative. (The app never creates a 0-point choice; if one
// is hand-edited into the data file it is treated as good, matching the green
// colour the Day Detail screen already uses for it.)
bool IsGoodChoice(const BehaviorEntry &entry);

// A behavior button defined in behavior_choices.txt. Used so that choices
// that were never logged still appear in the rankings with "0 times".
struct ChoiceInfo
{
    std::string name;
    bool isGood = true;
};

// When an entry was logged, in local time.
struct EntryTimeInfo
{
    std::string date; // "YYYY-MM-DD"
    int weekday = 0;  // 0 = Sunday ... 6 = Saturday
    int hour = 0;     // 0-23
};
EntryTimeInfo GetEntryTimeInfo(const BehaviorEntry &entry);

// A logged behavior plus its local date/weekday/hour, worked out once.
// Converting timestamps is the slow part, so the Trends screen prepares this
// list only when the log changes, then filters and counts it every frame.
struct TimedEntry
{
    BehaviorEntry entry;
    EntryTimeInfo time;
};
std::vector<TimedEntry> PrepareTrendEntries(const std::vector<BehaviorEntry> &entries);

// ---------------------------------------------------------------------------
// Filters shared by every Trends view (and the PDF export).
//
// HOW TO ADD A NEW FILTER LATER (e.g. "medication given", "slept < 8 hours"):
//   1. Add a field to TrendFilter below. Its default value must let every
//      entry through, so existing views are unaffected until it is used.
//   2. Check it in TrendFilter::matchesDay() (if it depends only on the day)
//      or TrendFilter::matches() (if it depends on the individual behavior).
//   3. Add a line for it in TrendFilter::describe(). That text is shown on
//      the Trends screen and printed in the PDF header automatically.
//   4. Add a control for it in DrawTrendFilters() in trends_view.cpp.
// Both tabs, the "days included" count and the PDF pick it up from there.
// ---------------------------------------------------------------------------
struct TrendFilter
{
    // Inclusive date range, "YYYY-MM-DD". Empty means "no limit".
    std::string fromDate;
    std::string toDate;

    // Days of the week to include, indexed 0 = Sunday ... 6 = Saturday.
    std::array<bool, 7> weekdays{{true, true, true, true, true, true, true}};

    bool anyWeekdaySelected() const;
    bool allWeekdaysSelected() const;

    // Day-level filters (date range, day of week).
    bool matchesDay(const std::string &date, int weekday) const;

    // Every filter, for one logged behavior.
    bool matches(const TimedEntry &item) const;

    // Human-readable (label, value) lines describing the active filters,
    // e.g. {"Date range:", "October 10, 2026 to November 9, 2026 (31 days)"}.
    std::vector<std::pair<std::string, std::string>> describe() const;
};

// "All days", "Mondays only", "Weekdays (Mon-Fri)", "Weekends (Sat-Sun)",
// "Mon, Wed, Fri" or "No days selected".
std::string DescribeWeekdays(const std::array<bool, 7> &weekdays);

// Number of days in `window` that pass the filter's day-level filters.
int CountDaysIncluded(const std::vector<HistoryDay> &window, const TrendFilter &filter);

// Number of logged behaviors that pass the filter.
int CountEntriesIncluded(const std::vector<TimedEntry> &entries, const TrendFilter &filter);

// ---------------------------------------------------------------------------
// Tab 1: Choice Rankings (good and wrong choices, most or least logged)
// ---------------------------------------------------------------------------
enum class RankOrder
{
    MostFirst,
    LeastFirst,
};

struct BehaviorCount
{
    std::string label;
    int count = 0;
};

struct ChoiceRankings
{
    std::vector<BehaviorCount> good;
    std::vector<BehaviorCount> wrong;
};

// Counts matching behaviors by name (point values are ignored, so a choice
// logged at different point values is combined). Every choice in
// `knownChoices` is listed, with 0 if it was not logged. Ties are
// alphabetical (A to Z) in both orders.
ChoiceRankings BuildChoiceRankings(const std::vector<TimedEntry> &entries,
                                   const std::vector<ChoiceInfo> &knownChoices,
                                   const TrendFilter &filter,
                                   RankOrder goodOrder, RankOrder wrongOrder);

void SortBehaviorCounts(std::vector<BehaviorCount> &counts, RankOrder order);

// ---------------------------------------------------------------------------
// Tab 2: Time of Day (behaviors grouped into one-hour blocks)
// ---------------------------------------------------------------------------
enum class ChoiceTypeFilter
{
    All,
    GoodOnly,
    WrongOnly,
};

enum class TimeOfDaySort
{
    ByHour,     // 12am-1am ... 11pm-12am; within an hour, most logged first
    MostLogged, // most logged (hour, behavior) first; empty hours last
};

// One row, e.g. "7am-8am | Being Destructive | 15 times".
// An hour with nothing logged has count == 0 and an empty label.
struct TimeOfDayRow
{
    int hour = 0; // 0-23
    std::string label;
    bool isGood = true;
    int count = 0;

    bool isEmptyHour() const { return count == 0; }
};

// Always contains every one of the 24 hours: hours with no matching
// behaviors get a single "No behaviors logged" row.
// Ties: by hour (earliest first), then alphabetical.
std::vector<TimeOfDayRow> BuildTimeOfDay(const std::vector<TimedEntry> &entries,
                                         const TrendFilter &filter,
                                         ChoiceTypeFilter show, TimeOfDaySort sort);

// Total number of logged behaviors in a Time of Day table.
int TotalLogged(const std::vector<TimeOfDayRow> &rows);

// ---------------------------------------------------------------------------
// Everything the Trends screen is currently showing. Passed to the PDF export
// so the PDF matches the screen.
// ---------------------------------------------------------------------------
enum class TrendsTab
{
    Rankings,
    TimeOfDay,
};

struct TrendsSettings
{
    TrendsTab tab = TrendsTab::Rankings;
    TrendFilter filter;

    // Choice Rankings tab
    RankOrder goodOrder = RankOrder::MostFirst;
    RankOrder wrongOrder = RankOrder::MostFirst;

    // Time of Day tab
    ChoiceTypeFilter show = ChoiceTypeFilter::All;
    TimeOfDaySort sort = TimeOfDaySort::ByHour;
};

// ---------------------------------------------------------------------------
// Text helpers (shared by the screen and the PDF)
// ---------------------------------------------------------------------------
std::string HourRangeLabel(int hour);          // 7 -> "7am-8am", 23 -> "11pm-12am"
std::string TimesLabel(int count);             // "1 time", "15 times"
std::string FormatTimeOfDayRow(const TimeOfDayRow &row); // "7am-8am | Lying | 3 times"
std::string RankOrderLabel(RankOrder order);   // "Most to least" / "Least to most"
std::string ChoiceTypeFilterLabel(ChoiceTypeFilter show);
std::string TimeOfDaySortLabel(TimeOfDaySort sort);
std::string NoBehaviorsLoggedText();           // "No behaviors logged"

// "YYYY-MM-DD" -> local noon of that day (avoids DST edges). -1 if invalid.
std::time_t ParseDate(const std::string &date);
