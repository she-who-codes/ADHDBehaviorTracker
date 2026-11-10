#include "trends.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <tuple>
#include <utility>

bool IsGoodChoice(const BehaviorEntry &entry)
{
    return entry.delta >= 0;
}

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------

bool TrendFilter::MatchesDay(const HistoryDay &day) const
{
    // "YYYY-MM-DD" strings sort the same way the dates do
    if (!fromDate.empty() && day.date < fromDate) return false;
    if (!toDate.empty() && day.date > toDate) return false;
    if (day.weekday < 0 || day.weekday > 6 || !weekdays[day.weekday]) return false;
    return true;
}

bool TrendFilter::MatchesEntry(const BehaviorEntry &) const
{
    // No per-behavior filters yet; this is where they go (see trends.h).
    return true;
}

bool TrendFilter::AllWeekdays() const
{
    return std::all_of(weekdays.begin(), weekdays.end(), [](bool on) { return on; });
}

bool TrendFilter::NoWeekdays() const
{
    return std::none_of(weekdays.begin(), weekdays.end(), [](bool on) { return on; });
}

std::string DescribeWeekdays(const std::array<bool, 7> &weekdays)
{
    static const char *kShortNames[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    const std::array<bool, 7> all{true, true, true, true, true, true, true};
    const std::array<bool, 7> schoolDays{false, true, true, true, true, true, false};
    const std::array<bool, 7> weekend{true, false, false, false, false, false, true};
    if (weekdays == all) return "All days";
    if (weekdays == schoolDays) return "Weekdays (Mon-Fri)";
    if (weekdays == weekend) return "Weekends (Sat-Sun)";

    std::string text;
    for (int d = 0; d < 7; ++d)
    {
        if (!weekdays[d]) continue;
        if (!text.empty()) text += ", ";
        text += kShortNames[d];
    }
    return text.empty() ? "None selected" : text;
}

FilteredBehaviors ApplyTrendFilter(const BehaviorLog &log, const TrendFilter &filter,
                                   std::time_t now)
{
    FilteredBehaviors result;
    for (const HistoryDay &day : log.historyWindow(now))
    {
        if (!filter.MatchesDay(day))
            continue;
        ++result.daysCovered;
        for (const BehaviorEntry &entry : day.entries)
            if (filter.MatchesEntry(entry))
                result.entries.push_back(entry);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> TrendFilter::Describe(
    const BehaviorLog &log, std::time_t now) const
{
    const std::vector<HistoryDay> days = log.historyWindow(now);

    // First and last day of the window that fall inside the chosen range
    const HistoryDay *first = nullptr;
    const HistoryDay *last = nullptr;
    for (const HistoryDay &day : days)
    {
        if ((!fromDate.empty() && day.date < fromDate) || (!toDate.empty() && day.date > toDate))
            continue;
        if (!first) first = &day;
        last = &day;
    }

    std::string range = "None";
    if (first && last)
    {
        const int length = static_cast<int>(last - first) + 1;
        range = BehaviorLog::formatLongDate(first->midnight) + " to " +
                BehaviorLog::formatLongDate(last->midnight) + " (" +
                std::to_string(length) + (length == 1 ? " day)" : " days)");
    }

    return {
        {"Date range:", range},
        {"Days of week:", DescribeWeekdays(weekdays)},
    };
}

// ---------------------------------------------------------------------------
// Sorting helpers
// ---------------------------------------------------------------------------

// Alphabetical, ignoring upper/lower case ("apple" before "Banana").
// Falls back to an exact comparison so the order is always the same.
static bool AlphabeticalLess(const std::string &a, const std::string &b)
{
    const bool lessIgnoringCase = std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(),
        [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
    if (lessIgnoringCase) return true;

    const bool greaterIgnoringCase = std::lexicographical_compare(
        b.begin(), b.end(), a.begin(), a.end(),
        [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
    if (greaterIgnoringCase) return false;

    return a < b;
}

// ---------------------------------------------------------------------------
// Tab 1: rankings
// ---------------------------------------------------------------------------

std::vector<BehaviorCount> RankBehaviors(const std::vector<BehaviorEntry> &entries,
                                         const std::vector<BehaviorChoice> &choices,
                                         bool goodChoices, RankOrder order)
{
    std::map<std::string, int> counts; // combined by name, whatever the points

    for (const BehaviorEntry &entry : entries)
        if (IsGoodChoice(entry) == goodChoices)
            ++counts[entry.label];

    // Buttons of this kind that were never logged (in this filter) show as 0
    for (const BehaviorChoice &choice : choices)
        if (choice.isGood == goodChoices)
            counts.emplace(choice.name, 0); // no-op if already counted

    std::vector<BehaviorCount> ranked;
    ranked.reserve(counts.size());
    for (const auto &item : counts)
        ranked.push_back({item.first, goodChoices, item.second});

    std::sort(ranked.begin(), ranked.end(),
              [order](const BehaviorCount &a, const BehaviorCount &b)
              {
                  if (a.count != b.count)
                      return order == RankOrder::MostFirst ? a.count > b.count
                                                           : a.count < b.count;
                  return AlphabeticalLess(a.name, b.name);
              });
    return ranked;
}

// ---------------------------------------------------------------------------
// Tab 2: time of day
// ---------------------------------------------------------------------------

std::vector<HourRow> TimeOfDayRows(const std::vector<BehaviorEntry> &entries,
                                   ChoiceKind kind, TimeOfDaySort sort)
{
    // (hour, name, isGood) -> count. isGood is part of the key only so that
    // a name that was somehow logged both ways is not shown in one colour.
    std::map<std::tuple<int, std::string, bool>, int> counts;

    for (const BehaviorEntry &entry : entries)
    {
        const bool good = IsGoodChoice(entry);
        if ((kind == ChoiceKind::Good && !good) || (kind == ChoiceKind::Wrong && good))
            continue;
        const int hour = BehaviorLog::toLocalTime(entry.timestamp).tm_hour;
        ++counts[std::make_tuple(hour, entry.label, good)];
    }

    std::vector<HourRow> rows;
    std::set<int> hoursWithData;
    for (const auto &item : counts)
    {
        HourRow row;
        row.hour = std::get<0>(item.first);
        row.name = std::get<1>(item.first);
        row.isGood = std::get<2>(item.first);
        row.count = item.second;
        rows.push_back(row);
        hoursWithData.insert(row.hour);
    }

    for (int hour = 0; hour < 24; ++hour)
    {
        if (hoursWithData.count(hour) == 0)
        {
            HourRow empty;
            empty.hour = hour;
            rows.push_back(empty);
        }
    }

    std::sort(rows.begin(), rows.end(),
              [sort](const HourRow &a, const HourRow &b)
              {
                  if (sort == TimeOfDaySort::ByCount)
                  {
                      // Empty hours go last (count 0 sorts after everything)
                      if (a.count != b.count) return a.count > b.count;
                      if (a.hour != b.hour) return a.hour < b.hour;
                  }
                  else
                  {
                      if (a.hour != b.hour) return a.hour < b.hour;
                      if (a.count != b.count) return a.count > b.count;
                  }
                  if (a.name != b.name) return AlphabeticalLess(a.name, b.name);
                  return a.isGood && !b.isGood; // same name logged both ways
              });
    return rows;
}

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------

static std::string TwelveHourLabel(int hour)
{
    hour = ((hour % 24) + 24) % 24;
    const int shown = (hour % 12 == 0) ? 12 : hour % 12;
    return std::to_string(shown) + (hour < 12 ? "am" : "pm");
}

std::string HourBlockLabel(int hour)
{
    return TwelveHourLabel(hour) + "-" + TwelveHourLabel(hour + 1);
}

std::string TimesLabel(int count)
{
    return std::to_string(count) + (count == 1 ? " time" : " times");
}

std::string EmptyHourLabel(ChoiceKind kind)
{
    switch (kind)
    {
    case ChoiceKind::Good: return "No good choices logged";
    case ChoiceKind::Wrong: return "No wrong choices logged";
    default: return "No behaviors logged";
    }
}
