#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "behavior_log.h"
#include "behavior_trends.h"
#include "history_export.h" // ExportResult, ExportPdfToDownloads, OpenWithDefaultApp

// Builds a PDF (bytes) of exactly what the Trends screen is showing: the
// active tab, with its filters and sort order.
std::string BuildTrendsPdf(const BehaviorLog &log, const std::vector<ChoiceInfo> &knownChoices,
                           const TrendsSettings &settings, std::time_t now = std::time(nullptr));

// "Behavior_Trends_Rankings_2026-10-10_to_2026-11-09.pdf" or
// "Behavior_Trends_Time_of_Day_2026-10-10_to_2026-11-09.pdf"
std::string TrendsPdfFileName(const TrendsSettings &settings);

// Writes the Trends PDF to the Downloads folder (never overwrites).
ExportResult ExportTrendsPdf(const BehaviorLog &log, const std::vector<ChoiceInfo> &knownChoices,
                             const TrendsSettings &settings, std::time_t now = std::time(nullptr));
