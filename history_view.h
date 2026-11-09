#pragma once

#include <string>

#include "behavior_log.h"

// Which screen is showing.
enum class Screen
{
    Main,
    History,
    DayDetail,
    Trends,
};

// Week rows (0-based, Sunday-first) of a month's calendar grid that contain
// at least one day in [firstDay, lastDay]. Weeks outside this range have no
// days in the history window and are hidden.
struct WeekRowRange
{
    int firstRow = 0;
    int lastRow = 0;
    int count() const { return lastRow - firstRow + 1; }
};
WeekRowRange VisibleWeekRows(int year, int month, int firstDay, int lastDay);

// Calendar view of the history window: one Sun-Sat grid per month (newest
// month first) plus a legend. Clicking a day switches to Screen::DayDetail
// and stores its "YYYY-MM-DD" in selectedDate.
void DrawHistoryScreen(BehaviorLog &log, Screen &screen, std::string &selectedDate);

// Full-screen list of every behavior logged on selectedDate.
void DrawDayDetailScreen(BehaviorLog &log, Screen &screen, const std::string &selectedDate);
