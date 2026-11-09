#include "behavior_trends.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>

namespace
{
const char *const kShortWeekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *const kPluralWeekdays[7] = {"Sundays", "Mondays", "Tuesdays", "Wednesdays",
                                        "Thursdays", "Fridays", "Saturdays"};

// Case-insensitive A-to-Z; exact comparison breaks ties so order is stable.
bool LabelLess(const std::string &a, const std::string &b)
{
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
    {
        const int ca = std::tolower(static_cast<unsigned char>(a[i]));
        const int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb)
            return ca < cb;
    }
    if (a.size() != b.size())
        return a.size() < b.size();
    return a < b;
}
} // namespace

bool IsGoodChoice(const BehaviorEntry &entry)
{
    return entry.delta >= 0;
}

EntryTimeInfo GetEntryTimeInfo(const BehaviorEntry &entry)
{
    const std::tm tm = BehaviorLog::toLocalTime(entry.timestamp);
    char date[40];
    std::snprintf(date, sizeof(date), "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

    EntryTimeInfo info;
    info.date = date;
    info.weekday = tm.tm_wday;
    info.hour = tm.tm_hour;
    return info;
}

std::vector<TimedEntry> PrepareTrendEntries(const std::vector<BehaviorEntry> &entries)
{
    std::vector<TimedEntry> result;
    result.reserve(entries.size());
    for (const BehaviorEntry &entry : entries)
        result.push_back({entry, GetEntryTimeInfo(entry)});
    return result;
}

std::time_t ParseDate(const std::string &date)
{
    int year = 0, month = 0, day = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
        return static_cast<std::time_t>(-1);

    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 12; // noon avoids DST edge cases
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

// ---------------------------------------------------------------------------
// TrendFilter
// ---------------------------------------------------------------------------

bool TrendFilter::anyWeekdaySelected() const
{
    return std::find(weekdays.begin(), weekdays.end(), true) != weekdays.end();
}

bool TrendFilter::allWeekdaysSelected() const
{
    return std::find(weekdays.begin(), weekdays.end(), false) == weekdays.end();
}

bool TrendFilter::matchesDay(const std::string &date, int weekday) const
{
    // "YYYY-MM-DD" strings sort the same way as the dates they represent.
    if (!fromDate.empty() && date < fromDate)
        return false;
    if (!toDate.empty() && date > toDate)
        return false;
    if (weekday < 0 || weekday > 6 || !weekdays[weekday])
        return false;
    return true;
}

bool TrendFilter::matches(const TimedEntry &item) const
{
    if (!matchesDay(item.time.date, item.time.weekday))
        return false;
    // Filters that look at the behavior itself (item.entry) go here.
    return true;
}

std::vector<std::pair<std::string, std::string>> TrendFilter::describe() const
{
    std::vector<std::pair<std::string, std::string>> lines;

    const std::time_t from = fromDate.empty() ? static_cast<std::time_t>(-1) : ParseDate(fromDate);
    const std::time_t to = toDate.empty() ? static_cast<std::time_t>(-1) : ParseDate(toDate);
    std::string range;
    if (from != static_cast<std::time_t>(-1) && to != static_cast<std::time_t>(-1))
    {
        // Both are local noon, so rounding absorbs any DST hour.
        const int days = static_cast<int>(std::lround(std::difftime(to, from) / 86400.0)) + 1;
        range = BehaviorLog::formatLongDate(from) + " to " + BehaviorLog::formatLongDate(to) +
                " (" + std::to_string(days) + (days == 1 ? " day)" : " days)");
    }
    else
    {
        range = "All stored days";
    }
    lines.emplace_back("Date range:", range);
    lines.emplace_back("Days of week:", DescribeWeekdays(weekdays));
    return lines;
}

std::string DescribeWeekdays(const std::array<bool, 7> &weekdays)
{
    int selected = 0;
    int only = -1;
    for (int d = 0; d < 7; ++d)
        if (weekdays[d])
        {
            ++selected;
            only = d;
        }

    if (selected == 0)
        return "No days selected";
    if (selected == 7)
        return "All days";
    if (selected == 1)
        return std::string(kPluralWeekdays[only]) + " only";

    const std::array<bool, 7> weekdaysOnly{{false, true, true, true, true, true, false}};
    const std::array<bool, 7> weekendsOnly{{true, false, false, false, false, false, true}};
    if (weekdays == weekdaysOnly)
        return "Weekdays (Mon-Fri)";
    if (weekdays == weekendsOnly)
        return "Weekends (Sat-Sun)";

    std::string list;
    for (int d = 0; d < 7; ++d)
    {
        if (!weekdays[d])
            continue;
        if (!list.empty())
            list += ", ";
        list += kShortWeekdays[d];
    }
    return list;
}

int CountDaysIncluded(const std::vector<HistoryDay> &window, const TrendFilter &filter)
{
    int count = 0;
    for (const HistoryDay &day : window)
        if (filter.matchesDay(day.date, day.weekday))
            ++count;
    return count;
}

int CountEntriesIncluded(const std::vector<TimedEntry> &entries, const TrendFilter &filter)
{
    int count = 0;
    for (const TimedEntry &item : entries)
        if (filter.matches(item))
            ++count;
    return count;
}

// ---------------------------------------------------------------------------
// Choice Rankings
// ---------------------------------------------------------------------------

void SortBehaviorCounts(std::vector<BehaviorCount> &counts, RankOrder order)
{
    std::sort(counts.begin(), counts.end(),
              [order](const BehaviorCount &a, const BehaviorCount &b)
              {
                  if (a.count != b.count)
                      return order == RankOrder::MostFirst ? a.count > b.count
                                                           : a.count < b.count;
                  return LabelLess(a.label, b.label); // ties: A to Z
              });
}

ChoiceRankings BuildChoiceRankings(const std::vector<TimedEntry> &entries,
                                   const std::vector<ChoiceInfo> &knownChoices,
                                   const TrendFilter &filter,
                                   RankOrder goodOrder, RankOrder wrongOrder)
{
    // Start every known choice at 0 so never-logged choices are listed too.
    std::map<std::string, int> good;
    std::map<std::string, int> wrong;
    for (const ChoiceInfo &choice : knownChoices)
        (choice.isGood ? good : wrong).emplace(choice.name, 0);

    // Combined by name; point values are ignored.
    for (const TimedEntry &item : entries)
        if (filter.matches(item))
            ++(IsGoodChoice(item.entry) ? good : wrong)[item.entry.label];

    ChoiceRankings result;
    for (const auto &item : good)
        result.good.push_back({item.first, item.second});
    for (const auto &item : wrong)
        result.wrong.push_back({item.first, item.second});

    SortBehaviorCounts(result.good, goodOrder);
    SortBehaviorCounts(result.wrong, wrongOrder);
    return result;
}

// ---------------------------------------------------------------------------
// Time of Day
// ---------------------------------------------------------------------------

std::vector<TimeOfDayRow> BuildTimeOfDay(const std::vector<TimedEntry> &entries,
                                         const TrendFilter &filter,
                                         ChoiceTypeFilter show, TimeOfDaySort sort)
{
    // Key: (hour, label, isGood). A name is combined within its hour; the
    // good/wrong split only matters in the rare case the same name was logged
    // both as a good and a wrong choice.
    std::map<std::tuple<int, std::string, bool>, int> counts;

    for (const TimedEntry &item : entries)
    {
        if (!filter.matches(item))
            continue;

        const bool good = IsGoodChoice(item.entry);
        if ((show == ChoiceTypeFilter::GoodOnly && !good) ||
            (show == ChoiceTypeFilter::WrongOnly && good))
            continue;

        ++counts[std::make_tuple(item.time.hour, item.entry.label, good)];
    }

    std::vector<TimeOfDayRow> rows;
    std::array<bool, 24> hourHasData{};
    for (const auto &item : counts)
    {
        TimeOfDayRow row;
        row.hour = std::get<0>(item.first);
        row.label = std::get<1>(item.first);
        row.isGood = std::get<2>(item.first);
        row.count = item.second;
        rows.push_back(row);
        hourHasData[row.hour] = true;
    }

    if (sort == TimeOfDaySort::ByHour)
    {
        std::sort(rows.begin(), rows.end(),
                  [](const TimeOfDayRow &a, const TimeOfDayRow &b)
                  {
                      if (a.hour != b.hour)
                          return a.hour < b.hour;
                      if (a.count != b.count)
                          return a.count > b.count;
                      return LabelLess(a.label, b.label);
                  });
    }
    else
    {
        std::sort(rows.begin(), rows.end(),
                  [](const TimeOfDayRow &a, const TimeOfDayRow &b)
                  {
                      if (a.count != b.count)
                          return a.count > b.count;
                      if (a.hour != b.hour)
                          return a.hour < b.hour;
                      return LabelLess(a.label, b.label);
                  });
    }

    // Empty hours: "No behaviors logged". In hour order they slot into place;
    // when sorted by most logged they go last (0 times), earliest hour first.
    std::vector<TimeOfDayRow> result;
    result.reserve(rows.size() + 24);
    if (sort == TimeOfDaySort::ByHour)
    {
        size_t next = 0;
        for (int hour = 0; hour < 24; ++hour)
        {
            if (!hourHasData[hour])
            {
                TimeOfDayRow empty;
                empty.hour = hour;
                result.push_back(empty);
                continue;
            }
            while (next < rows.size() && rows[next].hour == hour)
                result.push_back(rows[next++]);
        }
    }
    else
    {
        result = rows;
        for (int hour = 0; hour < 24; ++hour)
        {
            if (hourHasData[hour])
                continue;
            TimeOfDayRow empty;
            empty.hour = hour;
            result.push_back(empty);
        }
    }
    return result;
}

int TotalLogged(const std::vector<TimeOfDayRow> &rows)
{
    int total = 0;
    for (const TimeOfDayRow &row : rows)
        total += row.count;
    return total;
}

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------

static std::string HourLabel(int hour)
{
    hour = ((hour % 24) + 24) % 24;
    const int twelve = (hour % 12 == 0) ? 12 : hour % 12;
    return std::to_string(twelve) + (hour < 12 ? "am" : "pm");
}

std::string HourRangeLabel(int hour)
{
    return HourLabel(hour) + "-" + HourLabel(hour + 1);
}

std::string TimesLabel(int count)
{
    return std::to_string(count) + (count == 1 ? " time" : " times");
}

std::string NoBehaviorsLoggedText()
{
    return "No behaviors logged";
}

std::string FormatTimeOfDayRow(const TimeOfDayRow &row)
{
    if (row.isEmptyHour())
        return HourRangeLabel(row.hour) + " | " + NoBehaviorsLoggedText();
    return HourRangeLabel(row.hour) + " | " + row.label + " | " + TimesLabel(row.count);
}

std::string RankOrderLabel(RankOrder order)
{
    return order == RankOrder::MostFirst ? "Most to least" : "Least to most";
}

std::string ChoiceTypeFilterLabel(ChoiceTypeFilter show)
{
    switch (show)
    {
    case ChoiceTypeFilter::GoodOnly:
        return "Good choices only";
    case ChoiceTypeFilter::WrongOnly:
        return "Wrong choices only";
    case ChoiceTypeFilter::All:
    default:
        return "All choices (good and wrong)";
    }
}

std::string TimeOfDaySortLabel(TimeOfDaySort sort)
{
    return sort == TimeOfDaySort::ByHour ? "By hour of day (12am to 11pm)"
                                         : "Most logged first";
}
