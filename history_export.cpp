#include "history_export.h"

#include "report_writer.h"
#include "simple_pdf.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Windows 7+ (SHGetKnownFolderPath needs Vista+)
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace
{
using namespace report;

// Day table columns
constexpr float kColTime = kMargin + 8;
constexpr float kColBehavior = kMargin + 92;
constexpr float kColPointsRight = kMargin + 390;
constexpr float kColTotalRight = kMargin + kContentW - 8;

constexpr Color kDayBandFill{0.84f, 0.87f, 0.96f};

std::string signedPoints(int delta)
{
    return (delta > 0 ? "+" : "") + std::to_string(delta);
}

// Day heading band: date on the left, ending points (or nothing) on the right.
void dayHeading(ReportWriter &w, const HistoryDay &day, bool continued)
{
    const float bandH = 22.0f;
    w.doc.fillRect(kMargin, w.y, kContentW, bandH, kDayBandFill);

    std::string title = BehaviorLog::formatFriendlyDate(day.midnight);
    if (continued)
        title += " (continued)";
    w.doc.text(kMargin + 8, w.y + 15, title, Font::Bold, 11.5f, kText);

    if (day.hasData)
        w.doc.textRight(kMargin + kContentW - 8, w.y + 15,
                        "Ending points: " + std::to_string(day.endingPoints),
                        Font::Bold, 11, kText);
    w.y += bandH;
}

void dayTableHeader(ReportWriter &w)
{
    w.tableHeader({{"Time", kColTime}, {"Behavior", kColBehavior}},
                  {{"Points", kColPointsRight}, {"Total After", kColTotalRight}});
}
} // namespace

std::string HistoryPdfFileName(const BehaviorLog &log, std::time_t now)
{
    const std::vector<HistoryDay> days = log.historyWindow(now);
    return "Behavior_History_" + days.front().date + "_to_" + days.back().date + ".pdf";
}

std::string BuildHistoryPdf(const BehaviorLog &log, std::time_t now)
{
    const std::vector<HistoryDay> days = log.historyWindow(now); // oldest first

    ReportWriter w("Behavior Tracker - Behavior History Report");
    w.doc.setTitle("Behavior History Report");

    // ---- Report header ------------------------------------------------------
    w.doc.text(kMargin, w.y + 20, "Behavior History Report", Font::Bold, 22, kText);
    w.y += 36;

    const std::string range = BehaviorLog::formatLongDate(days.front().midnight) + " to " +
                              BehaviorLog::formatLongDate(days.back().midnight);
    const std::string created = BehaviorLog::formatFriendlyDate(now) + " at " +
                                BehaviorLog::formatFriendlyTime(now);

    const std::pair<const char *, std::string> headerLines[] = {
        {"Date range:", range},
        {"Created:", created},
        {"Current points:", std::to_string(log.currentPoints())},
    };
    for (const auto &line : headerLines)
    {
        w.doc.text(kMargin, w.y + 11, line.first, Font::Bold, kBodySize, kText);
        w.doc.text(kMargin + 95, w.y + 11, line.second, Font::Regular, kBodySize, kText);
        w.y += 17;
    }
    w.y += 8;
    w.rule();

    // ---- Behavior summary (most to least) -----------------------------------
    std::map<std::string, int> counts;
    for (const HistoryDay &day : days)
        for (const BehaviorEntry &e : day.entries)
            ++counts[e.label];

    std::vector<std::pair<std::string, int>> summary(counts.begin(), counts.end());
    std::stable_sort(summary.begin(), summary.end(),
                     [](const auto &a, const auto &b)
                     { return a.second > b.second; }); // ties stay alphabetical

    w.sectionTitle("Behavior Summary",
                   "Each behavior logged in this period and how many times it was logged (most to least).");

    if (summary.empty())
    {
        w.doc.text(kMargin + 8, w.y + 12, "No behaviors were logged in this period.",
                   Font::Regular, kBodySize, kMuted);
        w.y += kRowH;
    }
    else
    {
        const float timesRight = kMargin + kContentW - 8;
        w.tableHeader({{"Behavior", kMargin + 8}}, {{"Times Logged", timesRight}});
        int row = 0;
        for (const auto &item : summary)
        {
            if (w.ensureSpace(kRowH))
                w.tableHeader({{"Behavior", kMargin + 8}}, {{"Times Logged", timesRight}});
            w.zebra(row++);
            w.doc.text(kMargin + 8, w.y + 12.5f, item.first, Font::Regular, kBodySize, kText);
            w.doc.textRight(timesRight, w.y + 12.5f, std::to_string(item.second),
                            Font::Regular, kBodySize, kText);
            w.y += kRowH;
        }
    }
    w.y += 14;
    w.rule();

    // ---- Daily log, newest first --------------------------------------------
    w.sectionTitle("Daily Log",
                   "Newest day first. Points in green were earned; points in red were lost.");

    for (auto it = days.rbegin(); it != days.rend(); ++it)
    {
        const HistoryDay &day = *it;

        if (!day.hasData)
        {
            w.ensureSpace(22 + kRowH + 10);
            dayHeading(w, day, false);
            w.doc.text(kMargin + 8, w.y + 13, "No data logged", Font::Regular, kBodySize, kMuted);
            w.y += kRowH + 10;
            continue;
        }

        // Keep the heading together with the table header and first row
        w.ensureSpace(22 + kRowH * 2);
        dayHeading(w, day, false);
        dayTableHeader(w);

        int row = 0;
        for (const BehaviorEntry &e : day.entries)
        {
            if (w.ensureSpace(kRowH))
            {
                dayHeading(w, day, true);
                dayTableHeader(w);
            }
            w.zebra(row++);
            const float baseline = w.y + 12.5f;
            w.doc.text(kColTime, baseline, BehaviorLog::formatFriendlyTime(e.timestamp),
                       Font::Regular, kBodySize, kText);
            w.doc.text(kColBehavior, baseline, e.label, Font::Regular, kBodySize, kText);
            w.doc.textRight(kColPointsRight, baseline, signedPoints(e.delta), Font::Bold,
                            kBodySize, e.delta >= 0 ? kGreen : kRed);
            w.doc.textRight(kColTotalRight, baseline, std::to_string(e.pointsAfter),
                            Font::Regular, kBodySize, kText);
            w.y += kRowH;
        }
        w.y += 10;
    }

    w.footers();
    return w.doc.serialize();
}

std::filesystem::path DownloadsFolder()
{
#if defined(_WIN32)
    // FOLDERID_Downloads, defined here to avoid linking libuuid
    static const GUID kFolderIdDownloads = {
        0x374DE290, 0x123F, 0x4565, {0x91, 0x64, 0x39, 0xC4, 0x92, 0x5E, 0x46, 0x7B}};

    PWSTR widePath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(kFolderIdDownloads, 0, nullptr, &widePath)))
    {
        std::filesystem::path result(widePath);
        CoTaskMemFree(widePath);
        return result;
    }
    if (widePath)
        CoTaskMemFree(widePath);

    if (const wchar_t *profile = _wgetenv(L"USERPROFILE"))
        return std::filesystem::path(profile) / "Downloads";
    return std::filesystem::current_path();
#else
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Downloads";
    return std::filesystem::current_path();
#endif
}

std::filesystem::path UniquePath(const std::filesystem::path &dir, const std::string &fileName)
{
    std::filesystem::path candidate = dir / fileName;
    std::error_code ec;
    if (!std::filesystem::exists(candidate, ec))
        return candidate;

    const std::filesystem::path name(fileName);
    const std::string stem = name.stem().string();
    const std::string ext = name.extension().string();
    for (int n = 2;; ++n)
    {
        candidate = dir / (stem + " (" + std::to_string(n) + ")" + ext);
        if (!std::filesystem::exists(candidate, ec))
            return candidate;
    }
}

ExportResult SavePdfToDownloads(const std::string &fileName, const std::string &pdfBytes)
{
    ExportResult result;

    const std::filesystem::path dir = DownloadsFolder();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec); // no-op if it exists
    if (!std::filesystem::is_directory(dir, ec))
    {
        result.error = "The Downloads folder could not be found or created:\n" + dir.u8string();
        return result;
    }

    result.path = UniquePath(dir, fileName);

    std::ofstream out(result.path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        result.error = "Could not create the file:\n" + result.path.u8string();
        return result;
    }
    out.write(pdfBytes.data(), static_cast<std::streamsize>(pdfBytes.size()));
    out.close();
    if (!out)
    {
        result.error = "Could not finish writing the file:\n" + result.path.u8string();
        return result;
    }

    result.success = true;
    return result;
}

ExportResult ExportHistoryPdf(const BehaviorLog &log, std::time_t now)
{
    return SavePdfToDownloads(HistoryPdfFileName(log, now), BuildHistoryPdf(log, now));
}

bool OpenWithDefaultApp(const std::filesystem::path &path)
{
#if defined(_WIN32)
    const HINSTANCE h = ShellExecuteW(nullptr, L"open", path.wstring().c_str(),
                                      nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(h) > 32; // per ShellExecute docs
#elif defined(__APPLE__)
    return std::system(("open \"" + path.string() + "\"").c_str()) == 0;
#else
    // Launch in the background so the UI never waits on the viewer
    return std::system(("xdg-open \"" + path.string() + "\" </dev/null >/dev/null 2>&1 &").c_str()) == 0;
#endif
}
