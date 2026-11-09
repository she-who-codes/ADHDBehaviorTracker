#include "trends_export.h"

#include "pdf_report_writer.h"

#include <utility>

namespace
{
using namespace pdf_report;

constexpr float kHeaderValueX = kMargin + 125;

// Choice Rankings columns
constexpr float kColRankBehavior = kMargin + 8;
constexpr float kColRankTimesRight = kMargin + kContentW - 8;

// Time of Day columns
constexpr float kColHour = kMargin + 8;
constexpr float kColHourBehavior = kMargin + 110;
constexpr float kColHourTimesRight = kMargin + kContentW - 8;

// Room for a behavior name before the right-aligned "Times Logged" column.
constexpr float kRankNameMaxW = kColRankTimesRight - 80 - kColRankBehavior;
constexpr float kHourNameMaxW = kColHourTimesRight - 80 - kColHourBehavior;

// Shortens text with "..." so it fits in maxWidth (long behavior names would
// otherwise run into the next column).
std::string FitText(const std::string &s, Font font, float size, float maxWidth)
{
    if (PdfDocument::textWidth(s, font, size) <= maxWidth)
        return s;
    std::string cut = s;
    while (!cut.empty() && PdfDocument::textWidth(cut + "...", font, size) > maxWidth)
        cut.pop_back();
    while (!cut.empty() && cut.back() == ' ')
        cut.pop_back();
    return cut + "...";
}

const char *TabTitle(TrendsTab tab)
{
    return tab == TrendsTab::Rankings ? "Choice Rankings" : "Time of Day";
}

void HeaderLine(ReportWriter &w, const std::string &label, const std::string &value)
{
    w.doc.text(kMargin, w.y + 11, label, Font::Bold, kBodySize, kText);
    w.doc.text(kHeaderValueX, w.y + 11, value, Font::Regular, kBodySize, kText);
    w.y += 17;
}

void RankingTableHeader(ReportWriter &w)
{
    w.tableHeader({{"Behavior", kColRankBehavior}}, {{"Times Logged", kColRankTimesRight}});
}

void RankingSection(ReportWriter &w, const std::string &title, RankOrder order,
                    const std::vector<BehaviorCount> &counts, Color nameColor)
{
    const std::string caption =
        std::string(order == RankOrder::MostFirst ? "Most logged first." : "Least logged first.") +
        " Ties are alphabetical. Choices that were not logged show 0 times.";
    w.sectionTitle(title, caption);

    if (counts.empty())
    {
        w.doc.text(kMargin + 8, w.y + 12, "There are no choices of this type.",
                   Font::Regular, kBodySize, kMuted);
        w.y += kRowH;
        return;
    }

    RankingTableHeader(w);
    int row = 0;
    for (const BehaviorCount &item : counts)
    {
        if (w.ensureSpace(kRowH))
            RankingTableHeader(w);
        w.zebra(row++);
        const Color color = item.count == 0 ? kMuted : nameColor;
        w.doc.text(kColRankBehavior, w.y + 12.5f,
                   FitText(item.label, Font::Regular, kBodySize, kRankNameMaxW),
                   Font::Regular, kBodySize, color);
        w.doc.textRight(kColRankTimesRight, w.y + 12.5f, TimesLabel(item.count),
                        Font::Regular, kBodySize, item.count == 0 ? kMuted : kText);
        w.y += kRowH;
    }
}

void TimeOfDayTableHeader(ReportWriter &w)
{
    w.tableHeader({{"Time of Day", kColHour}, {"Behavior", kColHourBehavior}},
                  {{"Times Logged", kColHourTimesRight}});
}

void TimeOfDaySection(ReportWriter &w, const TrendsSettings &settings,
                      const std::vector<TimeOfDayRow> &rows)
{
    const std::string caption =
        settings.sort == TimeOfDaySort::ByHour
            ? "Each hour of the day, 12am to 11pm. Green = good choice, red = wrong choice."
            : "Most logged first; hours with nothing logged are last. Green = good, red = wrong.";
    w.sectionTitle("Time of Day", caption);

    TimeOfDayTableHeader(w);

    // When sorted by hour, shade by hour block so rows in the same hour group
    // together visually; otherwise shade every other row.
    int band = 0;
    int previousHour = -1;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const TimeOfDayRow &row = rows[i];
        if (w.ensureSpace(kRowH))
            TimeOfDayTableHeader(w);

        if (settings.sort == TimeOfDaySort::ByHour)
        {
            if (row.hour != previousHour && previousHour != -1)
                ++band;
            previousHour = row.hour;
            w.zebra(band);
        }
        else
        {
            w.zebra(static_cast<int>(i));
        }

        const float baseline = w.y + 12.5f;
        w.doc.text(kColHour, baseline, HourRangeLabel(row.hour), Font::Regular, kBodySize,
                   row.isEmptyHour() ? kMuted : kText);
        if (row.isEmptyHour())
        {
            w.doc.text(kColHourBehavior, baseline, NoBehaviorsLoggedText(), Font::Regular,
                       kBodySize, kMuted);
        }
        else
        {
            w.doc.text(kColHourBehavior, baseline,
                       FitText(row.label, Font::Regular, kBodySize, kHourNameMaxW),
                       Font::Regular, kBodySize, row.isGood ? kGreen : kRed);
            w.doc.textRight(kColHourTimesRight, baseline, TimesLabel(row.count),
                            Font::Regular, kBodySize, kText);
        }
        w.y += kRowH;
    }
}
} // namespace

std::string TrendsPdfFileName(const TrendsSettings &settings)
{
    const std::string view = settings.tab == TrendsTab::Rankings ? "Rankings" : "Time_of_Day";
    return "Behavior_Trends_" + view + "_" + settings.filter.fromDate + "_to_" +
           settings.filter.toDate + ".pdf";
}

std::string BuildTrendsPdf(const BehaviorLog &log, const std::vector<ChoiceInfo> &knownChoices,
                           const TrendsSettings &settings, std::time_t now)
{
    const TrendFilter &filter = settings.filter;
    const std::vector<TimedEntry> entries = PrepareTrendEntries(log.entries());
    const bool rankingsTab = (settings.tab == TrendsTab::Rankings);

    // Same builders the Trends screen uses, so the PDF matches the screen.
    ChoiceRankings rankings;
    std::vector<TimeOfDayRow> hourRows;
    if (rankingsTab)
        rankings = BuildChoiceRankings(entries, knownChoices, filter,
                                       settings.goodOrder, settings.wrongOrder);
    else
        hourRows = BuildTimeOfDay(entries, filter, settings.show, settings.sort);

    // Behaviors listed in this report (for Time of Day this respects the
    // good/wrong filter, so it always matches the table).
    const int behaviorsListed = rankingsTab ? CountEntriesIncluded(entries, filter)
                                            : TotalLogged(hourRows);

    ReportWriter w;
    w.doc.setTitle("Behavior Trends Report");

    // ---- Report header ------------------------------------------------------
    w.doc.text(kMargin, w.y + 20, "Behavior Trends Report", Font::Bold, 22, kText);
    w.y += 36;

    HeaderLine(w, "View:", TabTitle(settings.tab));
    for (const auto &line : filter.describe())
        HeaderLine(w, line.first, line.second);
    HeaderLine(w, "Days included:",
               std::to_string(CountDaysIncluded(log.historyWindow(now), filter)));
    if (!rankingsTab)
        HeaderLine(w, "Showing:", ChoiceTypeFilterLabel(settings.show));
    HeaderLine(w, "Behaviors included:", std::to_string(behaviorsListed));
    if (rankingsTab)
    {
        HeaderLine(w, "Good choices:", RankOrderLabel(settings.goodOrder));
        HeaderLine(w, "Wrong choices:", RankOrderLabel(settings.wrongOrder));
    }
    else
    {
        HeaderLine(w, "Sorted:", TimeOfDaySortLabel(settings.sort));
    }
    HeaderLine(w, "Created:", BehaviorLog::formatFriendlyDate(now) + " at " +
                                  BehaviorLog::formatFriendlyTime(now));
    w.y += 8;
    w.rule();

    // ---- Body -----------------------------------------------------------------
    if (rankingsTab)
    {
        RankingSection(w, "Good Choices", settings.goodOrder, rankings.good, kGreen);
        w.y += 14;
        RankingSection(w, "Wrong Choices", settings.wrongOrder, rankings.wrong, kRed);
    }
    else
    {
        TimeOfDaySection(w, settings, hourRows);
    }

    w.footers("Behavior Trends Report");
    return w.doc.serialize();
}

ExportResult ExportTrendsPdf(const BehaviorLog &log, const std::vector<ChoiceInfo> &knownChoices,
                             const TrendsSettings &settings, std::time_t now)
{
    return ExportPdfToDownloads(TrendsPdfFileName(settings),
                                BuildTrendsPdf(log, knownChoices, settings, now));
}
