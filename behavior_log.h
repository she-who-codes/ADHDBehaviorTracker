#pragma once

#include <ctime>
#include <string>
#include <vector>

// One logged behavior (a single button click).
struct BehaviorEntry
{
    std::time_t timestamp = 0; // When the behavior was logged
    std::string label;         // e.g. "Completed Morning Routine"
    int delta = 0;             // Points added (+) or removed (-)
    int pointsAfter = 0;       // Running total right after this behavior

    std::string dateString() const; // "YYYY-MM-DD"
    std::string timeString() const; // "HH:MM:SS"
};

// One calendar day inside the history window. Days with no logged
// behaviors are included too (hasData == false).
struct HistoryDay
{
    std::string date;          // "YYYY-MM-DD"
    int year = 0;              // e.g. 2025
    int month = 0;             // 1-12
    int day = 0;               // 1-31
    int weekday = 0;           // 0 = Sunday ... 6 = Saturday
    std::time_t midnight = 0;  // Local midnight at the start of this day
    bool hasData = false;      // true if at least one behavior was logged
    int endingPoints = 0;      // Points total after the day's last behavior
    std::vector<BehaviorEntry> entries; // In chronological order
};

// Stores the current points plus a timestamped log of every behavior.
//
// File format (behavior_data.txt):
//   Line 1:  current points (integer) - same as the original file format
//   Line 2+: YYYY-MM-DD HH:MM:SS|delta|pointsAfter|label
//
// History covers today plus the kHistoryDaysBeforeToday previous calendar
// days. Entries dated before that window are removed by date (at local
// midnight), never by a rolling 24-hour clock. Pruning happens whenever the
// log is loaded or saved.
class BehaviorLog
{
public:
    static constexpr int kHistoryDaysBeforeToday = 30;
    static constexpr int kHistoryWindowDays = kHistoryDaysBeforeToday + 1; // incl. today

    explicit BehaviorLog(std::string filePath = "behavior_data.txt");

    // Load from disk. Returns false if the file could not be opened
    // (the log is left empty with 0 points in that case).
    bool load(std::time_t now = std::time(nullptr));

    // Write the current points and all retained entries to disk.
    bool save(std::time_t now = std::time(nullptr));

    // Record a behavior click, update the points total, and persist.
    void record(const std::string &label, int delta,
                std::time_t now = std::time(nullptr));

    // Remove entries dated before the start of the history window.
    void prune(std::time_t now = std::time(nullptr));

    // Local midnight at the start of the oldest day in the history window.
    static std::time_t windowStart(std::time_t now = std::time(nullptr));

    // Every day in the history window, oldest first (always
    // kHistoryWindowDays entries, including days with no data).
    std::vector<HistoryDay> historyWindow(std::time_t now = std::time(nullptr)) const;

    int currentPoints() const { return m_points; }
    void setCurrentPoints(int points) { m_points = points; }

    const std::vector<BehaviorEntry> &entries() const { return m_entries; }
    const std::string &filePath() const { return m_filePath; }

    // Helpers exposed for formatting in the UI.
    static std::tm toLocalTime(std::time_t t);
    static std::time_t localMidnight(std::time_t t, int dayOffset = 0); // midnight of (day of t + offset)
    static std::string formatDate(std::time_t t);                 // "YYYY-MM-DD"
    static std::string formatTime(std::time_t t);                 // "HH:MM:SS"
    static std::string formatFriendlyDate(std::time_t t);         // "Wednesday, January 5, 2025"
    static std::string formatLongDate(std::time_t t);             // "January 5, 2025"
    static std::string formatFriendlyTime(std::time_t t);         // "8:32 AM"
    static bool parseEntry(const std::string &line, BehaviorEntry &out);
    static std::string serializeEntry(const BehaviorEntry &entry);

private:
    std::string m_filePath;
    int m_points = 0;
    std::vector<BehaviorEntry> m_entries; // Kept sorted by timestamp
};
