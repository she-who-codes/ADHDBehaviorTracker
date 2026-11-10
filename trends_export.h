#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "behavior_choice.h"
#include "behavior_log.h"
#include "history_export.h"
#include "trends.h"

// Which Trends tab is showing (and therefore what gets exported).
enum class TrendsTab
{
    MostLeast,
    TimeOfDay,
};

// Everything the parent has chosen on the Trends screen. The screen keeps
// one of these, and the PDF export receives the same object, so the PDF
// always matches what is on screen.
struct TrendsReportOptions
{
    TrendFilter filter; // shared by both tabs

    TrendsTab tab = TrendsTab::MostLeast;

    // "Most / Least" tab
    RankOrder goodOrder = RankOrder::MostFirst;
    RankOrder wrongOrder = RankOrder::MostFirst;

    // "Time of Day" tab
    ChoiceKind timeKind = ChoiceKind::All;
    TimeOfDaySort timeSort = TimeOfDaySort::ByHour;
};

// e.g. "Behavior_Trends_Time_of_Day_2026-10-10_to_2026-11-09.pdf"
std::string TrendsPdfFileName(const BehaviorLog &log, const TrendsReportOptions &options,
                              std::time_t now = std::time(nullptr));

// The complete PDF (bytes) for the current tab, filters and sort.
std::string BuildTrendsPdf(const BehaviorLog &log, const std::vector<BehaviorChoice> &choices,
                           const TrendsReportOptions &options,
                           std::time_t now = std::time(nullptr));

// Builds the PDF and writes it to the Downloads folder.
ExportResult ExportTrendsPdf(const BehaviorLog &log, const std::vector<BehaviorChoice> &choices,
                             const TrendsReportOptions &options,
                             std::time_t now = std::time(nullptr));
