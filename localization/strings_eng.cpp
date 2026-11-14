#include "strings.h"

#include <iterator>

// ---------------------------------------------------------------------------
// English text. This is also the fallback for any text another language
// has not translated yet, so every ID must have an entry here.
//
// {0}, {1}, ... are placeholders filled in by the app (see strings.h).
// ---------------------------------------------------------------------------

namespace
{
const TextEntry kText[] = {
    // ---- Shared names ------------------------------------------------------
    {TextId::WeekdayShortSun, "Sun"},
    {TextId::WeekdayShortMon, "Mon"},
    {TextId::WeekdayShortTue, "Tue"},
    {TextId::WeekdayShortWed, "Wed"},
    {TextId::WeekdayShortThu, "Thu"},
    {TextId::WeekdayShortFri, "Fri"},
    {TextId::WeekdayShortSat, "Sat"},

    {TextId::MonthJanuary, "January"},
    {TextId::MonthFebruary, "February"},
    {TextId::MonthMarch, "March"},
    {TextId::MonthApril, "April"},
    {TextId::MonthMay, "May"},
    {TextId::MonthJune, "June"},
    {TextId::MonthJuly, "July"},
    {TextId::MonthAugust, "August"},
    {TextId::MonthSeptember, "September"},
    {TextId::MonthOctober, "October"},
    {TextId::MonthNovember, "November"},
    {TextId::MonthDecember, "December"},

    {TextId::MonthShortJan, "Jan"},
    {TextId::MonthShortFeb, "Feb"},
    {TextId::MonthShortMar, "Mar"},
    {TextId::MonthShortApr, "Apr"},
    {TextId::MonthShortMay, "May"},
    {TextId::MonthShortJun, "Jun"},
    {TextId::MonthShortJul, "Jul"},
    {TextId::MonthShortAug, "Aug"},
    {TextId::MonthShortSep, "Sep"},
    {TextId::MonthShortOct, "Oct"},
    {TextId::MonthShortNov, "Nov"},
    {TextId::MonthShortDec, "Dec"},

    // ---- Shared formats ----------------------------------------------------
    {TextId::MonthYearTitle, "{0} {1}"},
    {TextId::ShortDate, "{0}, {1} {2}"},
    {TextId::ListSeparator, ", "},
    {TextId::DateRangeFromTo, "{0} to {1}"},
    {TextId::DateAtTime, "{0} at {1}"},
    {TextId::HourAm, "{0}am"},
    {TextId::HourPm, "{0}pm"},
    {TextId::HourRange, "{0}-{1}"},

    // ---- Shared across screens (and PDFs) ----------------------------------
    {TextId::CommonBackButton, "< Back"},
    {TextId::CommonTrendsButton, "Trends"},
    {TextId::CommonExportPdfButton, "Export PDF"},
    {TextId::CommonCurrentPoints, "Current Points: {0}"},
    {TextId::CommonGoodChoices, "Good Choices"},
    {TextId::CommonWrongChoices, "Wrong Choices"},
    {TextId::CommonNoDataLogged, "No data logged"},
    {TextId::CommonGoldOutlineToday, "Gold outline: today."},

    {TextId::ColumnTime, "Time"},
    {TextId::ColumnBehavior, "Behavior"},
    {TextId::ColumnPoints, "Points"},
    {TextId::ColumnTotalAfter, "Total After"},
    {TextId::ColumnTimesLogged, "Times Logged"},
    {TextId::ColumnType, "Type"},

    // ---- Main screen -------------------------------------------------------
    {TextId::MainWelcome, "Welcome to the Behavior Tracker!"},
    {TextId::MainHistoryButton, "History"},

    {TextId::DefaultChoiceMorningRoutine, "Completed Morning Routine"},
    {TextId::DefaultChoiceFollowedDirections, "Followed Directions"},
    {TextId::DefaultChoiceSchoolwork, "Completed Schoolwork"},
    {TextId::DefaultChoiceNegativeBehavior, "Negative Behavior"},
    {TextId::DefaultChoiceIncompleteSchoolwork, "Incomplete Schoolwork"},

    // ---- "Create a choice" popup -------------------------------------------
    {TextId::AddChoiceGoodButton, "+  CREATE A GOOD CHOICE"},
    {TextId::AddChoiceWrongButton, "+  CREATE A WRONG CHOICE"},
    {TextId::AddChoiceGoodTitle, "New Good Choice"},
    {TextId::AddChoiceWrongTitle, "New Wrong Choice"},
    {TextId::AddChoiceNameLabel, "Choice name:"},
    {TextId::AddChoiceGoodNameHint, "e.g. Cleaned Room"},
    {TextId::AddChoiceWrongNameHint, "e.g. Lying"},
    {TextId::AddChoicePointsToAdd, "Points to add:"},
    {TextId::AddChoicePointsToSubtract, "Points to subtract:"},
    {TextId::AddChoiceGoodPointsHelp, "+ rewards more, - rewards less (+1 to +10)"},
    {TextId::AddChoiceWrongPointsHelp, "+ penalises more, - penalises less (-1 to -10)"},
    {TextId::AddChoiceAddButton, "Add"},
    {TextId::AddChoiceCancelButton, "Cancel"},

    // ---- History calendar --------------------------------------------------
    {TextId::HistoryTitle, "History - Last 30 Days"},
    {TextId::HistoryLegendTitle, "Legend"},
    {TextId::HistoryLegendLogged, "Behaviors logged. Shows the points total at the end of that day."},
    {TextId::HistoryLegendNoData, "Grayed out: no behaviors were logged that day (\"{0}\")."},
    {TextId::HistoryLegendBlank, "Blank square: date is outside the 30-day history."},
    {TextId::HistoryLegendDetailIntro, "On a day's detail screen:"},
    {TextId::HistoryLegendPointsEarned, "points earned"},
    {TextId::HistoryLegendPointsLost, "points lost"},
    {TextId::HistoryLegendClickDay, "Click any day to see every behavior logged on it."},
    {TextId::HistoryLegendRetentionNote,
     "Note: Data is only stored for 30 days. At midnight each night, "
     "the oldest day is permanently removed."},

    // ---- Day detail --------------------------------------------------------
    {TextId::DayDetailBackButton, "< Back to Calendar"},
    {TextId::DayDetailRemoved,
     "This day is no longer within the 30-day history, so its data has been removed."},
    {TextId::DayDetailTitleToday, "{0}  (Today)"},
    {TextId::DayDetailNoBehaviors, "No behaviors were logged on this day."},
    {TextId::DayDetailEndingPoints, "Ending Points: {0}"},

    // ---- PDF export popup --------------------------------------------------
    {TextId::ExportPopupTitle, "PDF Export"},
    {TextId::ExportComplete, "Export complete"},
    {TextId::ExportSavedHistory, "Your behavior history was saved to your Downloads folder:"},
    {TextId::ExportSavedTrends, "Your trends report was saved to your Downloads folder:"},
    {TextId::ExportOpenedInViewer, "It has been opened in your default PDF viewer."},
    {TextId::ExportNotOpened,
     "It could not be opened automatically. You can open it from your Downloads folder."},
    {TextId::ExportFailed, "Export failed"},
    {TextId::ExportOkButton, "OK"},

    {TextId::ExportErrorNoDownloadsFolder, "The Downloads folder could not be found or created:\n{0}"},
    {TextId::ExportErrorCreateFile, "Could not create the file:\n{0}"},
    {TextId::ExportErrorWriteFile, "Could not finish writing the file:\n{0}"},

    // ---- Trends screen -----------------------------------------------------
    {TextId::TrendsTitle, "Trends"},
    {TextId::TrendsSubtitle, "Spot patterns in logged behaviors"},
    {TextId::TrendsBackHintHistory, "Go back to the History calendar."},
    {TextId::TrendsBackHintMain, "Go back to the main screen."},
    {TextId::TrendsExportHint,
     "Save exactly what is on screen (this tab, your dates, days and sorting) "
     "as a PDF in your Downloads folder, then open it."},
    {TextId::TrendsExportHintNoDays, "Check at least one day of the week before exporting."},
    {TextId::TrendsTabMostLeast, "Most / Least Logged"},
    {TextId::TrendsTabMostLeastHint,
     "See which good and wrong choices were logged the most and the least."},
    {TextId::TrendsTabTimeOfDay, "Time of Day"},
    {TextId::TrendsTabTimeOfDayHint,
     "See how many times each behavior was logged in each hour of the day."},

    {TextId::TrendsDatesLabel, "Dates:"},
    {TextId::TrendsDateRangeTo, "to"},
    {TextId::TrendsFromDateHint, "Pick the first day to include. Opens a calendar."},
    {TextId::TrendsToDateHint, "Pick the last day to include. Opens a calendar."},
    {TextId::TrendsLast7Days, "Last 7 days"},
    {TextId::TrendsLast7DaysHint, "Count only the last 7 days, including today."},
    {TextId::TrendsLast14Days, "Last 14 days"},
    {TextId::TrendsLast14DaysHint, "Count only the last 14 days, including today."},
    {TextId::TrendsResetButton, "Reset to Defaults"},
    {TextId::TrendsResetHint,
     "Undo every choice on this screen: all stored days, every day of the week, "
     "both lists most first, and Time of Day showing all choices by hour."},

    {TextId::TrendsDaysLabel, "Days:"},
    {TextId::TrendsIncludeSundaysHint, "Include Sundays. Uncheck to leave them out."},
    {TextId::TrendsIncludeMondaysHint, "Include Mondays. Uncheck to leave them out."},
    {TextId::TrendsIncludeTuesdaysHint, "Include Tuesdays. Uncheck to leave them out."},
    {TextId::TrendsIncludeWednesdaysHint, "Include Wednesdays. Uncheck to leave them out."},
    {TextId::TrendsIncludeThursdaysHint, "Include Thursdays. Uncheck to leave them out."},
    {TextId::TrendsIncludeFridaysHint, "Include Fridays. Uncheck to leave them out."},
    {TextId::TrendsIncludeSaturdaysHint, "Include Saturdays. Uncheck to leave them out."},
    {TextId::TrendsAllDaysButton, "All days"},
    {TextId::TrendsAllDaysHint, "Check all seven days of the week."},
    {TextId::TrendsAllDaysHintAlready, "All seven days are already checked."},
    {TextId::TrendsClearDaysButton, "Clear days"},
    {TextId::TrendsClearDaysHint,
     "Uncheck every day, so you can check just the one or two you want. "
     "At least one day must be checked to see trends."},
    {TextId::TrendsClearDaysHintNone, "No days are checked. Check the days you want to see."},
    {TextId::TrendsPickAtLeastOneDay, "Pick at least one day of the week to see trends."},
    {TextId::TrendsCountingSummary, "Counting {0} from {1} ({2})."},

    {TextId::TrendsPickerRangeNote, "Bright: first and last chosen day. Shaded: days between."},

    {TextId::TrendsMostFirst, "Most first"},
    {TextId::TrendsMostFirstGoodHint, "List good choices from most logged to least logged."},
    {TextId::TrendsMostFirstWrongHint, "List wrong choices from most logged to least logged."},
    {TextId::TrendsLeastFirst, "Least first"},
    {TextId::TrendsLeastFirstGoodHint,
     "List good choices from least logged to most logged. "
     "Good choices never logged (0 times) come first."},
    {TextId::TrendsLeastFirstWrongHint,
     "List wrong choices from least logged to most logged. "
     "Wrong choices never logged (0 times) come first."},
    {TextId::TrendsNoGoodChoicesYet, "There are no good choices yet."},
    {TextId::TrendsNoWrongChoicesYet, "There are no wrong choices yet."},

    {TextId::TrendsShowLabel, "Show:"},
    {TextId::TrendsSortLabel, "Sort:"},
    {TextId::TrendsKindAll, "All choices"},
    {TextId::TrendsKindAllHint, "Show good and wrong choices together."},
    {TextId::TrendsKindGood, "Good only"},
    {TextId::TrendsKindGoodHint, "Show only good choices."},
    {TextId::TrendsKindWrong, "Wrong only"},
    {TextId::TrendsKindWrongHint, "Show only wrong choices."},
    {TextId::TrendsSortByHour, "By time of day"},
    {TextId::TrendsSortByHourHint,
     "List the hours in order, 12am to 11pm, most logged first within each hour."},
    {TextId::TrendsSortByCount, "By most logged"},
    {TextId::TrendsSortByCountHint,
     "List the most logged behavior and hour first, across the whole day. "
     "Hours with nothing logged go last."},
    {TextId::TrendsTimeOfDayNote,
     "Each row is one hour of the day, added up across every included day."},
    {TextId::TrendsGreenNote, "Green = good choice."},
    {TextId::TrendsRedNote, "Red = wrong choice."},

    // ---- Trends text shared by the screen and the PDF ----------------------
    {TextId::WeekdaysAll, "All days"},
    {TextId::WeekdaysSchoolDays, "Weekdays (Mon-Fri)"},
    {TextId::WeekdaysWeekend, "Weekends (Sat-Sun)"},
    {TextId::WeekdaysNone, "None selected"},
    {TextId::DescribeDateRangeNone, "None"},
    {TextId::DescribeDaysOfWeekLabel, "Days of week:"},
    {TextId::EmptyHourAll, "No behaviors logged"},
    {TextId::EmptyHourGood, "No good choices logged"},
    {TextId::EmptyHourWrong, "No wrong choices logged"},

    // ---- PDF reports (shared) ----------------------------------------------
    {TextId::PdfFooter, "Behavior Tracker - {0}"},
    {TextId::PdfPageNumber, "Page {0} of {1}"},
    {TextId::PdfDateRangeLabel, "Date range:"},
    {TextId::PdfCreatedLabel, "Created:"},

    // ---- History PDF -------------------------------------------------------
    {TextId::PdfHistoryTitle, "Behavior History Report"},
    {TextId::PdfHistoryCurrentPoints, "Current points:"},
    {TextId::PdfHistorySummaryTitle, "Behavior Summary"},
    {TextId::PdfHistorySummaryCaption,
     "Each behavior logged in this period and how many times it was logged (most to least)."},
    {TextId::PdfHistoryNoBehaviors, "No behaviors were logged in this period."},
    {TextId::PdfHistoryDailyLogTitle, "Daily Log"},
    {TextId::PdfHistoryDailyLogCaption,
     "Newest day first. Points in green were earned; points in red were lost."},
    {TextId::PdfHistoryDayContinued, "{0} (continued)"},
    {TextId::PdfHistoryEndingPoints, "Ending points: {0}"},

    // ---- Trends PDF --------------------------------------------------------
    {TextId::PdfTrendsTitle, "Behavior Trends Report"},
    {TextId::PdfTrendsViewLabel, "View:"},
    {TextId::PdfTrendsDaysIncludedLabel, "Days included:"},
    {TextId::PdfTrendsShowingLabel, "Showing:"},
    {TextId::PdfTrendsSortedLabel, "Sorted:"},
    {TextId::PdfTrendsGoodChoicesLabel, "Good choices:"},
    {TextId::PdfTrendsWrongChoicesLabel, "Wrong choices:"},
    {TextId::PdfTrendsOrderMostFirst, "Most to least logged"},
    {TextId::PdfTrendsOrderLeastFirst, "Least to most logged"},
    {TextId::PdfTrendsKindAll, "Good and wrong choices"},
    {TextId::PdfTrendsKindGood, "Good choices only"},
    {TextId::PdfTrendsKindWrong, "Wrong choices only"},
    {TextId::PdfTrendsSortByHour, "By time of day (12am to 11pm)"},
    {TextId::PdfTrendsSortByCount, "By most logged (all hours)"},
    {TextId::PdfTrendsRankingCaptionMostFirst,
     "Most to least logged. Choices that were never logged show 0. Ties are listed A-Z."},
    {TextId::PdfTrendsRankingCaptionLeastFirst,
     "Least to most logged. Choices that were never logged show 0. Ties are listed A-Z."},
    {TextId::PdfTrendsTimeOfDayTitle, "Behaviors by Time of Day"},
    {TextId::PdfTrendsTimeOfDayCaption,
     "How many times each behavior was logged in each one-hour block, "
     "across all included days."},
    {TextId::PdfTrendsTypeGood, "Good"},
    {TextId::PdfTrendsTypeWrong, "Wrong"},
};

// Forms: {Zero, One, Two, Few, Many, Other}. English uses One and Other.
const PluralEntry kPlurals[] = {
    {PluralId::TimesLogged,
     {nullptr, "{0} time", nullptr, nullptr, nullptr, "{0} times"}},
    {PluralId::HistoryCellEnding,
     {nullptr, "Ending: {0} pts", nullptr, nullptr, nullptr, "Ending: {0} pts"}},
    {PluralId::DayDetailBehaviorsLogged,
     {nullptr, "{0} behavior logged", nullptr, nullptr, nullptr, "{0} behaviors logged"}},
    {PluralId::TrendsAllDaysPreset,
     {nullptr, "All {0} day", nullptr, nullptr, nullptr, "All {0} days"}},
    {PluralId::TrendsAllDaysPresetHint,
     {nullptr, "Count everything stored: today and the {0} day before it.", nullptr, nullptr,
      nullptr, "Count everything stored: today and the {0} days before it."}},
    {PluralId::TrendsLoggedBehaviors,
     {nullptr, "{0} logged behavior", nullptr, nullptr, nullptr, "{0} logged behaviors"}},
    {PluralId::TrendsDayCount,
     {nullptr, "{0} day", nullptr, nullptr, nullptr, "{0} days"}},
    {PluralId::TrendsPickerFadedNote,
     {nullptr, "Faded dates are outside the {0}-day history.", nullptr, nullptr, nullptr,
      "Faded dates are outside the {0}-day history."}},
    {PluralId::DescribeDateRange,
     {nullptr, "{1} to {2} ({0} day)", nullptr, nullptr, nullptr, "{1} to {2} ({0} days)"}},
};

// English: "1 time", everything else (including 0) "N times".
PluralForm EnglishPluralRule(long long n)
{
    return (n == 1) ? PluralForm::One : PluralForm::Other;
}
} // namespace

const Language kEnglish = {
    "en",
    "English",
    kText,
    std::size(kText),
    kPlurals,
    std::size(kPlurals),
    EnglishPluralRule,
};
