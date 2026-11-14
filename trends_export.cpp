#include "trends_export.h"

#include "localization/strings.h"
#include "report_writer.h"

#include <utility>

namespace
{
using namespace report;

// Time-of-day table columns
constexpr float kColHour = kMargin + 8;
constexpr float kColName = kMargin + 100;
constexpr float kColType = kMargin + 350;
constexpr float kColTimesRight = kMargin + kContentW - 8;

const char *TabTitle(TrendsTab tab)
{
    return Tr(tab == TrendsTab::MostLeast ? TextId::TrendsTabMostLeast
                                          : TextId::TrendsTabTimeOfDay);
}

const char *OrderText(RankOrder order)
{
    return Tr(order == RankOrder::MostFirst ? TextId::PdfTrendsOrderMostFirst
                                            : TextId::PdfTrendsOrderLeastFirst);
}

const char *KindText(ChoiceKind kind)
{
    switch (kind)
    {
    case ChoiceKind::Good: return Tr(TextId::PdfTrendsKindGood);
    case ChoiceKind::Wrong: return Tr(TextId::PdfTrendsKindWrong);
    default: return Tr(TextId::PdfTrendsKindAll);
    }
}

const char *SortText(TimeOfDaySort sort)
{
    return Tr(sort == TimeOfDaySort::ByHour ? TextId::PdfTrendsSortByHour
                                            : TextId::PdfTrendsSortByCount);
}

// First and last "YYYY-MM-DD" of the history window that fall inside the
// filter's date range (used for the file name).
std::pair<std::string, std::string> ResolvedRange(const BehaviorLog &log,
                                                  const TrendFilter &filter, std::time_t now)
{
    std::string first, last;
    for (const HistoryDay &day : log.historyWindow(now))
    {
        if ((!filter.fromDate.empty() && day.date < filter.fromDate) ||
            (!filter.toDate.empty() && day.date > filter.toDate))
            continue;
        if (first.empty()) first = day.date;
        last = day.date;
    }
    return {first, last};
}

// One ranking table ("Good Choices" or "Wrong Choices").
void RankingSection(ReportWriter &w, const std::string &title, const std::vector<BehaviorCount> &rows,
                    RankOrder order, bool good)
{
    w.sectionTitle(title, Tr(order == RankOrder::MostFirst
                                 ? TextId::PdfTrendsRankingCaptionMostFirst
                                 : TextId::PdfTrendsRankingCaptionLeastFirst));

    const float timesRight = kMargin + kContentW - 8;
    const auto header = [&]
    {
        w.tableHeader({{Tr(TextId::ColumnBehavior), kMargin + 8}},
                      {{Tr(TextId::ColumnTimesLogged), timesRight}});
    };
    header();

    if (rows.empty())
    {
        w.doc.text(kMargin + 8, w.y + 12.5f,
                   Tr(good ? TextId::TrendsNoGoodChoicesYet : TextId::TrendsNoWrongChoicesYet),
                   Font::Regular, kBodySize, kMuted);
        w.y += kRowH;
    }

    int index = 0;
    for (const BehaviorCount &row : rows)
    {
        if (w.ensureSpace(kRowH))
            header();
        w.zebra(index++);
        const float baseline = w.y + 12.5f;
        w.doc.text(kMargin + 8, baseline,
                   FitText(row.name, Font::Regular, kBodySize, timesRight - kMargin - 100),
                   Font::Regular, kBodySize, kText);
        w.doc.textRight(timesRight, baseline, TimesLabel(row.count), Font::Bold, kBodySize,
                        row.count == 0 ? kMuted : (good ? kGreen : kRed));
        w.y += kRowH;
    }
    w.y += 14;
}

void TimeOfDaySection(ReportWriter &w, const std::vector<HourRow> &rows,
                      const TrendsReportOptions &options)
{
    const bool showType = options.timeKind == ChoiceKind::All;
    w.sectionTitle(Tr(TextId::PdfTrendsTimeOfDayTitle), Tr(TextId::PdfTrendsTimeOfDayCaption));

    const auto header = [&]
    {
        std::vector<std::pair<std::string, float>> left = {{Tr(TextId::ColumnTime), kColHour},
                                                           {Tr(TextId::ColumnBehavior), kColName}};
        if (showType)
            left.push_back({Tr(TextId::ColumnType), kColType});
        w.tableHeader(left, {{Tr(TextId::ColumnTimesLogged), kColTimesRight}});
    };
    header();

    const float nameWidth = (showType ? kColType : kColTimesRight - 70) - kColName - 8;

    // Sorted by hour: shade alternate hours so each hour reads as a group.
    // Sorted by count: shade alternate rows.
    int shade = 0;
    int previousHour = -1;
    for (const HourRow &row : rows)
    {
        if (options.timeSort == TimeOfDaySort::ByHour)
        {
            if (row.hour != previousHour && previousHour != -1)
                ++shade;
            previousHour = row.hour;
        }

        if (w.ensureSpace(kRowH))
            header();
        w.zebra(shade);
        if (options.timeSort == TimeOfDaySort::ByCount)
            ++shade;

        const float baseline = w.y + 12.5f;
        w.doc.text(kColHour, baseline, HourBlockLabel(row.hour), Font::Regular, kBodySize, kText);

        if (row.IsEmptyHour())
        {
            w.doc.text(kColName, baseline, EmptyHourLabel(options.timeKind), Font::Regular,
                       kBodySize, kMuted);
        }
        else
        {
            const Color color = row.isGood ? kGreen : kRed;
            w.doc.text(kColName, baseline, FitText(row.name, Font::Regular, kBodySize, nameWidth),
                       Font::Regular, kBodySize, kText);
            if (showType)
                w.doc.text(kColType, baseline,
                           Tr(row.isGood ? TextId::PdfTrendsTypeGood : TextId::PdfTrendsTypeWrong),
                           Font::Regular, kBodySize, color);
            w.doc.textRight(kColTimesRight, baseline, TimesLabel(row.count), Font::Bold,
                            kBodySize, color);
        }
        w.y += kRowH;
    }
    w.y += 14;
}
} // namespace

std::string TrendsPdfFileName(const BehaviorLog &log, const TrendsReportOptions &options,
                              std::time_t now)
{
    const auto range = ResolvedRange(log, options.filter, now);
    const std::string view =
        options.tab == TrendsTab::MostLeast ? "Most_Least" : "Time_of_Day";
    return "Behavior_Trends_" + view + "_" + range.first + "_to_" + range.second + ".pdf";
}

std::string BuildTrendsPdf(const BehaviorLog &log, const std::vector<BehaviorChoice> &choices,
                           const TrendsReportOptions &options, std::time_t now)
{
    const FilteredBehaviors data = ApplyTrendFilter(log, options.filter, now);

    const char *reportTitle = Tr(TextId::PdfTrendsTitle);
    ReportWriter w(Format(Tr(TextId::PdfFooter), {reportTitle}));
    w.doc.setTitle(reportTitle);

    // ---- Report header: exactly which view, filters and sort were used -------
    w.doc.text(kMargin, w.y + 20, reportTitle, Font::Bold, 22, kText);
    w.y += 36;

    std::vector<std::pair<std::string, std::string>> headerLines;
    headerLines.push_back({Tr(TextId::PdfTrendsViewLabel), TabTitle(options.tab)});
    for (const auto &line : options.filter.Describe(log, now))
        headerLines.push_back(line);
    headerLines.push_back({Tr(TextId::PdfTrendsDaysIncludedLabel), std::to_string(data.daysCovered)});
    if (options.tab == TrendsTab::TimeOfDay)
    {
        headerLines.push_back({Tr(TextId::PdfTrendsShowingLabel), KindText(options.timeKind)});
        headerLines.push_back({Tr(TextId::PdfTrendsSortedLabel), SortText(options.timeSort)});
    }
    else
    {
        headerLines.push_back({Tr(TextId::PdfTrendsGoodChoicesLabel), OrderText(options.goodOrder)});
        headerLines.push_back({Tr(TextId::PdfTrendsWrongChoicesLabel), OrderText(options.wrongOrder)});
    }
    headerLines.push_back({Tr(TextId::PdfCreatedLabel),
                           Format(Tr(TextId::DateAtTime), {BehaviorLog::formatFriendlyDate(now),
                                                           BehaviorLog::formatFriendlyTime(now)})});

    for (const auto &line : headerLines)
    {
        w.doc.text(kMargin, w.y + 11, line.first, Font::Bold, kBodySize, kText);
        w.doc.text(kMargin + 95, w.y + 11, line.second, Font::Regular, kBodySize, kText);
        w.y += 17;
    }
    w.y += 8;
    w.rule();

    // ---- Body ----------------------------------------------------------------
    if (options.tab == TrendsTab::MostLeast)
    {
        RankingSection(w, Tr(TextId::CommonGoodChoices),
                       RankBehaviors(data.entries, choices, true, options.goodOrder),
                       options.goodOrder, true);
        RankingSection(w, Tr(TextId::CommonWrongChoices),
                       RankBehaviors(data.entries, choices, false, options.wrongOrder),
                       options.wrongOrder, false);
    }
    else
    {
        TimeOfDaySection(w, TimeOfDayRows(data.entries, options.timeKind, options.timeSort),
                         options);
    }

    w.footers();
    return w.doc.serialize();
}

ExportResult ExportTrendsPdf(const BehaviorLog &log, const std::vector<BehaviorChoice> &choices,
                             const TrendsReportOptions &options, std::time_t now)
{
    return SavePdfToDownloads(TrendsPdfFileName(log, options, now),
                              BuildTrendsPdf(log, choices, options, now));
}
