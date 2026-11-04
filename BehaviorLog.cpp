#include "BehaviorLog.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <system_error>
#include <utility>

namespace
{
    std::tm toLocalTime(std::time_t when)
    {
        std::tm result{};
#if defined(_MSC_VER)
        localtime_s(&result, &when);
#else
        if (const std::tm *local = std::localtime(&when))
        {
            result = *local;
        }
#endif
        return result;
    }

    std::string formatDate(const std::tm &tm)
    {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        return buffer;
    }

    std::string formatTime(const std::tm &tm)
    {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        return buffer;
    }

    std::string trim(const std::string &text)
    {
        const char *whitespace = " \t\r\n";
        const size_t first = text.find_first_not_of(whitespace);
        if (first == std::string::npos)
        {
            return "";
        }
        const size_t last = text.find_last_not_of(whitespace);
        return text.substr(first, last - first + 1);
    }

    bool parseInt(const std::string &rawText, int &value)
    {
        const std::string text = trim(rawText);
        if (text.empty())
        {
            return false;
        }

        errno = 0;
        char *end = nullptr;
        const long parsed = std::strtol(text.c_str(), &end, 10);
        if (*end != '\0' || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX)
        {
            return false;
        }

        value = static_cast<int>(parsed);
        return true;
    }

    bool allDigits(const std::string &text, size_t start, size_t count)
    {
        for (size_t i = start; i < start + count; ++i)
        {
            if (text[i] < '0' || text[i] > '9')
            {
                return false;
            }
        }
        return true;
    }

    // Accepts only "YYYY-MM-DD" with a sensible month and day.
    bool parseDate(const std::string &date, int &year, int &month, int &day)
    {
        if (date.size() != 10 || date[4] != '-' || date[7] != '-' ||
            !allDigits(date, 0, 4) || !allDigits(date, 5, 2) || !allDigits(date, 8, 2))
        {
            return false;
        }

        year = std::atoi(date.substr(0, 4).c_str());
        month = std::atoi(date.substr(5, 2).c_str());
        day = std::atoi(date.substr(8, 2).c_str());
        return month >= 1 && month <= 12 && day >= 1 && day <= 31;
    }

    // Accepts only "HH:MM:SS" with a sensible hour, minute and second.
    bool parseTime(const std::string &time, int &hour, int &minute, int &second)
    {
        if (time.size() != 8 || time[2] != ':' || time[5] != ':' ||
            !allDigits(time, 0, 2) || !allDigits(time, 3, 2) || !allDigits(time, 6, 2))
        {
            return false;
        }

        hour = std::atoi(time.substr(0, 2).c_str());
        minute = std::atoi(time.substr(3, 2).c_str());
        second = std::atoi(time.substr(6, 2).c_str());
        return hour <= 23 && minute <= 59 && second <= 60;
    }

    // Parses "YYYY-MM-DD|HH:MM:SS|points|totalAfter|behavior name".
    bool parseEntry(const std::string &line, BehaviorEntry &entry)
    {
        size_t separators[4];
        size_t searchFrom = 0;
        for (size_t &separator : separators)
        {
            separator = line.find('|', searchFrom);
            if (separator == std::string::npos)
            {
                return false;
            }
            searchFrom = separator + 1;
        }

        entry.date = trim(line.substr(0, separators[0]));
        entry.time = trim(line.substr(separators[0] + 1, separators[1] - separators[0] - 1));
        const std::string pointsText = line.substr(separators[1] + 1, separators[2] - separators[1] - 1);
        const std::string totalText = line.substr(separators[2] + 1, separators[3] - separators[2] - 1);
        entry.behavior = trim(line.substr(separators[3] + 1));

        int year, month, day, hour, minute, second;
        if (!parseDate(entry.date, year, month, day) ||
            !parseTime(entry.time, hour, minute, second) ||
            !parseInt(pointsText, entry.points) ||
            !parseInt(totalText, entry.totalAfter))
        {
            return false;
        }

        if (entry.behavior.empty())
        {
            entry.behavior = "(unnamed behavior)";
        }
        return true;
    }

    // Writes the whole file in one go. The stream is closed before
    // returning so the file can be renamed afterwards (needed on Windows).
    bool writeDataFile(const std::string &path, int currentPoints,
                       const std::vector<BehaviorEntry> &entries)
    {
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open())
        {
            return false;
        }

        out << "# ADHD Behavior Tracker data\n";
        out << "# POINTS = current point balance\n";
        out << "# Behavior lines: date|time|points|points after|behavior\n";
        out << "POINTS " << currentPoints << "\n";

        for (const BehaviorEntry &entry : entries)
        {
            out << entry.date << '|'
                << entry.time << '|'
                << (entry.points > 0 ? "+" : "") << entry.points << '|'
                << entry.totalAfter << '|'
                << entry.behavior << "\n";
        }

        out.close();
        return !out.fail();
    }
}

BehaviorLog::BehaviorLog(std::string filePath)
    : filePath_(std::move(filePath))
{
}

bool BehaviorLog::load()
{
    std::ifstream in(filePath_);
    if (!in.is_open())
    {
        return false;
    }

    int points = 0;
    bool foundPoints = false;
    std::vector<BehaviorEntry> loaded;

    std::string line;
    while (std::getline(in, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        int value = 0;
        if (line.rfind("POINTS", 0) == 0)
        {
            if (parseInt(line.substr(6), value))
            {
                points = value;
                foundPoints = true;
            }
            continue;
        }

        // Older versions of the app saved only the point balance.
        if (parseInt(line, value))
        {
            points = value;
            foundPoints = true;
            continue;
        }

        BehaviorEntry entry;
        if (parseEntry(line, entry))
        {
            loaded.push_back(entry);
        }
    }

    if (!foundPoints && !loaded.empty())
    {
        points = loaded.back().totalAfter;
    }

    currentPoints_ = points;
    entries_ = std::move(loaded);
    return true;
}

bool BehaviorLog::save()
{
    // Write to a temporary file first and then swap it into place, so a
    // crash or power loss mid-write can't leave a half-written data file.
    const std::string tempPath = filePath_ + ".tmp";
    bool saved = false;

    if (writeDataFile(tempPath, currentPoints_, entries_))
    {
        std::error_code error;
        std::filesystem::rename(tempPath, filePath_, error);
        saved = !error;
        if (!saved)
        {
            std::filesystem::remove(tempPath, error);
        }
    }

    if (!saved)
    {
        // Fall back to writing the data file directly.
        saved = writeDataFile(filePath_, currentPoints_, entries_);
    }

    lastSaveSucceeded_ = saved;
    return saved;
}

bool BehaviorLog::logBehavior(const std::string &behavior, int points, std::time_t now)
{
    const std::tm local = toLocalTime(now);

    BehaviorEntry entry;
    entry.date = formatDate(local);
    entry.time = formatTime(local);
    entry.points = points;
    entry.behavior = behavior;

    // Keep each behavior on a single line in the file.
    std::replace(entry.behavior.begin(), entry.behavior.end(), '\n', ' ');
    std::replace(entry.behavior.begin(), entry.behavior.end(), '\r', ' ');

    currentPoints_ += points;
    entry.totalAfter = currentPoints_;
    entries_.push_back(entry);

    pruneOldEntries(now);
    return save();
}

bool BehaviorLog::pruneOldEntries(std::time_t now)
{
    // Dates are stored as "YYYY-MM-DD", so comparing the strings compares
    // the dates.
    const std::string oldestDate = oldestKeptDate(now);
    const size_t before = entries_.size();

    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&oldestDate](const BehaviorEntry &entry)
                       { return entry.date < oldestDate; }),
        entries_.end());

    return entries_.size() != before;
}

std::vector<DaySummary> BehaviorLog::dailySummaries() const
{
    std::map<std::string, DaySummary> byDate;

    for (const BehaviorEntry &entry : entries_)
    {
        DaySummary &day = byDate[entry.date];
        day.date = entry.date;
        day.behaviorCount++;
        day.netPoints += entry.points;
        day.endingPoints = entry.totalAfter; // The last entry of the day wins
        if (entry.points > 0)
        {
            day.goodCount++;
        }
        else if (entry.points < 0)
        {
            day.wrongCount++;
        }
    }

    std::vector<DaySummary> summaries;
    summaries.reserve(byDate.size());
    for (auto it = byDate.rbegin(); it != byDate.rend(); ++it)
    {
        summaries.push_back(it->second);
    }
    return summaries;
}

std::vector<BehaviorEntry> BehaviorLog::entriesForDate(const std::string &date) const
{
    std::vector<BehaviorEntry> result;
    for (const BehaviorEntry &entry : entries_)
    {
        if (entry.date == date)
        {
            result.push_back(entry);
        }
    }
    return result;
}

std::string BehaviorLog::dateString(std::time_t when)
{
    return formatDate(toLocalTime(when));
}

std::string BehaviorLog::oldestKeptDate(std::time_t now)
{
    std::tm day = toLocalTime(now);
    day.tm_mday -= kHistoryDays - 1; // mktime() handles month/year rollover
    day.tm_hour = 12;                // Midday avoids daylight-saving edge cases
    day.tm_min = 0;
    day.tm_sec = 0;
    day.tm_isdst = -1;
    std::mktime(&day);
    return formatDate(day);
}

std::string BehaviorLog::friendlyDate(const std::string &date, bool longForm)
{
    static const char *shortDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *longDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                     "Thursday", "Friday", "Saturday"};
    static const char *shortMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char *longMonths[] = {"January", "February", "March", "April",
                                       "May", "June", "July", "August",
                                       "September", "October", "November", "December"};

    int year, month, day;
    if (!parseDate(date, year, month, day))
    {
        return date;
    }

    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 12;
    tm.tm_isdst = -1;
    if (std::mktime(&tm) == static_cast<std::time_t>(-1) || tm.tm_mday != day)
    {
        return date; // Not a real calendar date (e.g. Feb 31)
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s, %s %d, %d",
                  longForm ? longDays[tm.tm_wday] : shortDays[tm.tm_wday],
                  longForm ? longMonths[month - 1] : shortMonths[month - 1],
                  day, year);
    return buffer;
}

std::string BehaviorLog::friendlyTime(const std::string &time)
{
    int hour, minute, second;
    if (!parseTime(time, hour, minute, second))
    {
        return time;
    }

    const int hour12 = (hour % 12 == 0) ? 12 : hour % 12;
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d %s",
                  hour12, minute, hour < 12 ? "AM" : "PM");
    return buffer;
}
