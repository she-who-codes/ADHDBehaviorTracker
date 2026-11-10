#pragma once

#include <array>
#include <ctime>
#include <string>
#include <vector>

#include "behavior_choice.h"
#include "behavior_log.h"

// ---------------------------------------------------------------------------
// Trends: counts of logged behaviors, used by the Trends screen and its PDF.
// Nothing in here draws anything, so it can be reused and tested on its own.
// ---------------------------------------------------------------------------

// A logged behavior is a good choice if it added points and a wrong choice if
// it removed points. (The app never logs 0 points; if a hand-edited file has
// one, it is treated as good, the same way the History screen colours it.)
bool IsGoodChoice(const BehaviorEntry &entry);

// Which kinds of choices to include.
enum class ChoiceKind
{
    All,
    Good,
    Wrong,
};

// ---------------------------------------------------------------------------
// Filters shared by both Trends tabs.
//
// To add a new filter later (e.g. "only days medication was taken late"):
//   1. Add a field below with a default value that lets everything through.
//   2. Check it in MatchesDay() (whole-day filters) or MatchesEntry()
//      (per-behavior filters) in trends.cpp.
//   3. Describe it in Describe() so it is printed on the exported PDF.
//   4. Add a control for it in DrawFilterPanel() in trends_view.cpp.
// Every count on both tabs, and the PDF export, then respects it.
// ---------------------------------------------------------------------------
struct TrendFilter
{
    // Date range, inclusive, as "YYYY-MM-DD". Empty means "no limit" (the
    // whole history window). The Trends screen keeps these inside the window.
    std::string fromDate;
    std::string toDate;

    // Days of the week to include, indexed 0 = Sunday ... 6 = Saturday.
    std::array<bool, 7> weekdays{true, true, true, true, true, true, true};

    bool MatchesDay(const HistoryDay &day) const;
    bool MatchesEntry(const BehaviorEntry &entry) const;

    bool AllWeekdays() const;
    bool NoWeekdays() const;

    // Human-readable lines such as {"Date range:", "October 10, 2026 to ..."}
    // for the PDF header.
    std::vector<std::pair<std::string, std::string>> Describe(
        const BehaviorLog &log, std::time_t now = std::time(nullptr)) const;
};

// "All days", "Weekdays (Mon-Fri)", "Weekends (Sat-Sun)", or e.g. "Mon, Wed, Fri".
std::string DescribeWeekdays(const std::array<bool, 7> &weekdays);

// The behaviors that pass the filter, plus how many calendar days they were
// drawn from (e.g. 5 Mondays) so counts can be read in context.
struct FilteredBehaviors
{
    std::vector<BehaviorEntry> entries; // chronological
    int daysCovered = 0;                // days in range that match the day filters
};
FilteredBehaviors ApplyTrendFilter(const BehaviorLog &log, const TrendFilter &filter,
                                   std::time_t now = std::time(nullptr));

// ---------------------------------------------------------------------------
// Tab 1: Most / least logged good and wrong choices
// ---------------------------------------------------------------------------

enum class RankOrder
{
    MostFirst,
    LeastFirst,
};

struct BehaviorCount
{
    std::string name;
    bool isGood = true;
    int count = 0;
};

// Every behavior of one kind (good or wrong) with how many times it was
// logged, combined by name regardless of its point value. Buttons that exist
// in 'choices' but were never logged are included with a count of 0.
// Ties are broken alphabetically (A-Z), whichever order is chosen.
std::vector<BehaviorCount> RankBehaviors(const std::vector<BehaviorEntry> &entries,
                                         const std::vector<BehaviorChoice> &choices,
                                         bool goodChoices, RankOrder order);

// ---------------------------------------------------------------------------
// Tab 2: Time of day, grouped into one-hour blocks
// ---------------------------------------------------------------------------

enum class TimeOfDaySort
{
    ByHour,  // 12am-1am first ... 11pm-12am last; most logged first within an hour
    ByCount, // most logged first across all hours
};

// One line of the time-of-day table, e.g. "7am-8am | Being Destructive | 15 times".
// Hours with nothing logged get a single row with an empty name and count 0.
struct HourRow
{
    int hour = 0; // 0-23, the start of the hour block
    std::string name;
    bool isGood = true;
    int count = 0;

    bool IsEmptyHour() const { return count == 0; }
};

// Every hour 0-23 appears at least once. With ByCount, hours that have
// nothing logged are listed after all the others, earliest hour first.
std::vector<HourRow> TimeOfDayRows(const std::vector<BehaviorEntry> &entries,
                                   ChoiceKind kind, TimeOfDaySort sort);

// ---------------------------------------------------------------------------
// Text helpers shared by the screen and the PDF
// ---------------------------------------------------------------------------

// "12am-1am", "7am-8am", "11am-12pm", "12pm-1pm", "11pm-12am"
std::string HourBlockLabel(int hour);

// "1 time", "15 times"
std::string TimesLabel(int count);

// "No behaviors logged", "No good choices logged", "No wrong choices logged"
std::string EmptyHourLabel(ChoiceKind kind);
