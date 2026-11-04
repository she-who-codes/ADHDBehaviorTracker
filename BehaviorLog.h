#pragma once

#include <ctime>
#include <string>
#include <vector>

// One behavior that a parent logged by clicking a button.
struct BehaviorEntry
{
    std::string date;     // Local date, "YYYY-MM-DD"
    std::string time;     // Local time, "HH:MM:SS"
    int points = 0;       // Points added (+) or removed (-) by this behavior
    int totalAfter = 0;   // Point balance right after this behavior was logged
    std::string behavior; // Behavior name, e.g. "Completed Schoolwork"
};

// Summary of everything logged on a single day.
struct DaySummary
{
    std::string date;     // "YYYY-MM-DD"
    int behaviorCount = 0;
    int goodCount = 0;    // Behaviors worth positive points
    int wrongCount = 0;   // Behaviors worth negative points
    int netPoints = 0;    // Total change in points during the day
    int endingPoints = 0; // Point balance at the end of the day
};

// Stores the current point balance and the behavior history, and keeps
// both saved in a plain-text file (behavior_data.txt).
//
// File format (one item per line):
//   # comment lines are ignored
//   POINTS <current balance>
//   YYYY-MM-DD|HH:MM:SS|<points>|<total after>|<behavior name>
//
// Older files that contain only a single number (the point balance) are
// still understood and are upgraded the next time the file is saved.
class BehaviorLog
{
public:
    // Number of days of history to keep, including today.
    static constexpr int kHistoryDays = 30;

    explicit BehaviorLog(std::string filePath);

    // Reads the data file. Returns false if the file could not be opened
    // (for example on the very first run, before it exists).
    bool load();

    // Writes everything to the data file. Returns true on success.
    bool save();

    // Records a behavior clicked by a parent, updates the point balance,
    // removes history older than kHistoryDays, and saves to the file.
    // Returns true if the file was saved successfully.
    bool logBehavior(const std::string &behavior, int points,
                     std::time_t now = std::time(nullptr));

    // Removes entries dated before the oldest day in the history window.
    // Returns true if anything was removed.
    bool pruneOldEntries(std::time_t now = std::time(nullptr));

    int currentPoints() const { return currentPoints_; }
    bool lastSaveSucceeded() const { return lastSaveSucceeded_; }
    const std::vector<BehaviorEntry> &entries() const { return entries_; }

    // One summary per day that has logged behaviors, newest day first.
    std::vector<DaySummary> dailySummaries() const;

    // Every behavior logged on the given "YYYY-MM-DD" date, in the order
    // they were logged.
    std::vector<BehaviorEntry> entriesForDate(const std::string &date) const;

    // ---- Date/time helpers ----

    // Local "YYYY-MM-DD" date for a point in time.
    static std::string dateString(std::time_t when);

    // Oldest date ("YYYY-MM-DD") that is still inside the history window.
    static std::string oldestKeptDate(std::time_t now = std::time(nullptr));

    // "2026-05-04" -> "Mon, May 4, 2026" (or "Monday, May 4, 2026" if
    // longForm is true). Returns the input unchanged if it is not a date.
    static std::string friendlyDate(const std::string &date, bool longForm = false);

    // "14:05:09" -> "2:05 PM". Returns the input unchanged if invalid.
    static std::string friendlyTime(const std::string &time);

private:
    std::string filePath_;
    int currentPoints_ = 0;
    bool lastSaveSucceeded_ = true;
    std::vector<BehaviorEntry> entries_;
};
