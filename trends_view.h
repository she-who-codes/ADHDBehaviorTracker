#pragma once

#include <vector>

#include "behavior_choice.h"
#include "behavior_log.h"
#include "history_view.h"

// Switches to the Trends screen, remembering the current screen so that
// "< Back" returns there (the Main screen or the History calendar).
void OpenTrendsScreen(Screen &screen);

// Trends screen: filters (date range, days of the week, with "All days",
// "Clear days" and "Reset to Defaults" buttons) shared by two tabs:
//   "Most / Least Logged" - good and wrong choices ranked by how often they
//                           were logged, each with its own most/least toggle.
//   "Time of Day"         - behaviors counted per one-hour block, e.g.
//                           "7am-8am | Being Destructive | 15 times".
// "Export PDF" saves exactly the current tab, filters and sort.
// 'choices' (the behavior buttons) lets never-logged choices show as 0.
void DrawTrendsScreen(const BehaviorLog &log, const std::vector<BehaviorChoice> &choices,
                      Screen &screen);