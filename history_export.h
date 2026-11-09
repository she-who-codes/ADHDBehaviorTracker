#pragma once

#include <ctime>
#include <filesystem>
#include <string>

#include "behavior_log.h"

// Result of exporting the history to a PDF.
struct ExportResult
{
    bool success = false;
    std::filesystem::path path; // where the PDF was written
    std::string error;          // reason, if success == false
};

// Builds the complete PDF report (bytes) for the current history window:
// header, "Behavior Summary" (most to least logged), then every day,
// newest first, including days with no data.
std::string BuildHistoryPdf(const BehaviorLog &log, std::time_t now = std::time(nullptr));

// "Behavior_History_2026-10-08_to_2026-11-07.pdf"
std::string HistoryPdfFileName(const BehaviorLog &log, std::time_t now = std::time(nullptr));

// The user's Downloads folder.
std::filesystem::path DownloadsFolder();

// dir/name, or dir/"name (2).ext", "name (3).ext"... if it already exists,
// so an earlier export is never overwritten.
std::filesystem::path UniquePath(const std::filesystem::path &dir, const std::string &fileName);

// Writes the report to the Downloads folder.
ExportResult ExportHistoryPdf(const BehaviorLog &log, std::time_t now = std::time(nullptr));

// Opens a file in the system's default application (e.g. the PDF viewer).
bool OpenWithDefaultApp(const std::filesystem::path &path);
