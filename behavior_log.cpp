#include "behavior_log.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

// ---------------------------------------------------------------------------
// Time helpers
// ---------------------------------------------------------------------------

std::tm BehaviorLog::toLocalTime(std::time_t t)
{
    std::tm result{};
#if defined(_MSC_VER)
    localtime_s(&result, &t);
#elif defined(__unix__) || defined(__APPLE__)
    localtime_r(&t, &result);
#else
    // MinGW / other: std::localtime is fine for this single-threaded app
    if (const std::tm *tm = std::localtime(&t))
        result = *tm;
#endif
    return result;
}

static std::string formatWith(std::time_t t, const char *fmt)
{
    std::tm tm = BehaviorLog::toLocalTime(t);
    char buffer[128];
    if (std::strftime(buffer, sizeof(buffer), fmt, &tm) == 0)
        return "";
    return buffer;
}

std::string BehaviorLog::formatDate(std::time_t t)
{
    return formatWith(t, "%Y-%m-%d");
}

std::string BehaviorLog::formatTime(std::time_t t)
{
    return formatWith(t, "%H:%M:%S");
}

// Day-of-month without a leading zero (strftime's %d always pads).
static std::string dayOfMonth(std::time_t t)
{
    return std::to_string(BehaviorLog::toLocalTime(t).tm_mday);
}

std::string BehaviorLog::formatFriendlyDate(std::time_t t)
{
    return formatWith(t, "%A, %B ") + dayOfMonth(t) + formatWith(t, ", %Y");
}

std::string BehaviorLog::formatLongDate(std::time_t t)
{
    return formatWith(t, "%B ") + dayOfMonth(t) + formatWith(t, ", %Y");
}

std::string BehaviorLog::formatFriendlyTime(std::time_t t)
{
    std::string s = formatWith(t, "%I:%M %p");
    if (s.size() > 1 && s[0] == '0')
        s.erase(0, 1); // "07:23 AM" -> "7:23 AM"
    return s;
}

std::string BehaviorEntry::dateString() const
{
    return BehaviorLog::formatDate(timestamp);
}

std::string BehaviorEntry::timeString() const
{
    return BehaviorLog::formatTime(timestamp);
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

std::string BehaviorLog::serializeEntry(const BehaviorEntry &entry)
{
    std::ostringstream out;
    out << formatDate(entry.timestamp) << ' ' << formatTime(entry.timestamp)
        << '|' << entry.delta
        << '|' << entry.pointsAfter
        << '|' << entry.label;
    return out.str();
}

bool BehaviorLog::parseEntry(const std::string &line, BehaviorEntry &out)
{
    // Expected: YYYY-MM-DD HH:MM:SS|delta|pointsAfter|label
    // The label is last so it may contain any character except newline.
    size_t p1 = line.find('|');
    if (p1 == std::string::npos) return false;
    size_t p2 = line.find('|', p1 + 1);
    if (p2 == std::string::npos) return false;
    size_t p3 = line.find('|', p2 + 1);
    if (p3 == std::string::npos) return false;

    std::string stamp = line.substr(0, p1);
    std::string deltaStr = line.substr(p1 + 1, p2 - p1 - 1);
    std::string afterStr = line.substr(p2 + 1, p3 - p2 - 1);
    std::string label = line.substr(p3 + 1);

    std::tm tm{};
    int year, month, day, hour, minute, second;
    if (std::sscanf(stamp.c_str(), "%d-%d-%d %d:%d:%d",
                    &year, &month, &day, &hour, &minute, &second) != 6)
        return false;

    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1; // Let the C library figure out DST

    std::time_t ts = std::mktime(&tm);
    if (ts == static_cast<std::time_t>(-1)) return false;

    try
    {
        out.delta = std::stoi(deltaStr);
        out.pointsAfter = std::stoi(afterStr);
    }
    catch (...)
    {
        return false;
    }

    out.timestamp = ts;
    out.label = label;
    return true;
}

// ---------------------------------------------------------------------------
// BehaviorLog
// ---------------------------------------------------------------------------

BehaviorLog::BehaviorLog(std::string filePath)
    : m_filePath(std::move(filePath))
{
}

bool BehaviorLog::load(std::time_t now)
{
    m_points = 0;
    m_entries.clear();

    std::ifstream in(m_filePath);
    if (!in.is_open())
        return false;

    std::string line;
    bool readPoints = false;

    while (std::getline(in, line))
    {
        // Strip a trailing carriage return (Windows line endings)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        if (!readPoints)
        {
            // First non-empty line is the points total (original format)
            try
            {
                m_points = std::stoi(line);
            }
            catch (...)
            {
                m_points = 0;
            }
            readPoints = true;
            continue;
        }

        BehaviorEntry entry;
        if (parseEntry(line, entry))
            m_entries.push_back(entry);
        // Malformed lines are skipped rather than aborting the load
    }

    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [](const BehaviorEntry &a, const BehaviorEntry &b)
                     { return a.timestamp < b.timestamp; });

    prune(now);
    return true;
}

bool BehaviorLog::save(std::time_t now)
{
    prune(now);

    std::ofstream out(m_filePath, std::ios::trunc);
    if (!out.is_open())
        return false;

    out << m_points << '\n';
    for (const BehaviorEntry &entry : m_entries)
        out << serializeEntry(entry) << '\n';

    return out.good();
}

void BehaviorLog::record(const std::string &label, int delta, std::time_t now)
{
    m_points += delta;

    BehaviorEntry entry;
    entry.timestamp = now;
    entry.label = label;
    entry.delta = delta;
    entry.pointsAfter = m_points;

    // Entries are normally appended in time order; insert sorted just in
    // case the system clock moved backwards.
    auto pos = std::upper_bound(m_entries.begin(), m_entries.end(), entry,
                                [](const BehaviorEntry &a, const BehaviorEntry &b)
                                { return a.timestamp < b.timestamp; });
    m_entries.insert(pos, entry);

    save(now);
}

std::time_t BehaviorLog::localMidnight(std::time_t t, int dayOffset)
{
    std::tm tm = toLocalTime(t);
    tm.tm_mday += dayOffset; // mktime normalizes across month/year boundaries
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1; // Let the C library resolve DST for the target day
    return std::mktime(&tm);
}

std::time_t BehaviorLog::windowStart(std::time_t now)
{
    return localMidnight(now, -kHistoryDaysBeforeToday);
}

void BehaviorLog::prune(std::time_t now)
{
    const std::time_t cutoff = windowStart(now);

    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
                       [cutoff](const BehaviorEntry &e)
                       { return e.timestamp < cutoff; }),
        m_entries.end());
}

std::vector<HistoryDay> BehaviorLog::historyWindow(std::time_t now) const
{
    std::vector<HistoryDay> days;
    days.reserve(kHistoryWindowDays);
    std::map<std::string, size_t> indexByDate;

    for (int offset = -kHistoryDaysBeforeToday; offset <= 0; ++offset)
    {
        HistoryDay day;
        day.midnight = localMidnight(now, offset);

        const std::tm tm = toLocalTime(day.midnight);
        day.year = tm.tm_year + 1900;
        day.month = tm.tm_mon + 1;
        day.day = tm.tm_mday;
        day.weekday = tm.tm_wday;
        day.date = formatDate(day.midnight);

        indexByDate[day.date] = days.size();
        days.push_back(day);
    }

    for (const BehaviorEntry &entry : m_entries) // already chronological
    {
        auto it = indexByDate.find(entry.dateString());
        if (it == indexByDate.end())
            continue; // outside the window (e.g. not yet pruned, or future-dated)

        HistoryDay &day = days[it->second];
        day.hasData = true;
        day.entries.push_back(entry);
        day.endingPoints = entry.pointsAfter; // last entry of the day wins
    }

    return days;
}
