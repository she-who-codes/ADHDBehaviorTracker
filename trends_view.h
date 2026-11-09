#pragma once

#include <vector>

#include "behavior_log.h"
#include "behavior_trends.h"
#include "history_view.h" // Screen

// Switches to the Trends screen. "< Back" on the Trends screen returns to
// `returnTo` (the Main screen or the History screen).
void OpenTrendsScreen(Screen &screen, Screen returnTo);

// Trends screen: shared filters (date range, days of week) and two tabs,
// "Choice Rankings" and "Time of Day", plus an "Export PDF" button that
// exports exactly what is showing. `knownChoices` are the behavior buttons,
// so choices that were never logged can be listed with 0.
void DrawTrendsScreen(const BehaviorLog &log, const std::vector<ChoiceInfo> &knownChoices,
                      Screen &screen);
