#pragma once

// ---------------------------------------------------------------------------
// Localization: every piece of user-facing text in the app.
//
// Code never contains user-facing text directly. It asks for text by ID:
//
//     ImGui::TextUnformatted(Tr(TextId::CommonGoodChoices));
//     Format(Tr(TextId::CommonCurrentPoints), {points}); // "Current Points: {0}"
//     TrPlural(PluralId::TimesLogged, count);            // "1 time" / "15 times"
//
// Each language lives in its own file in this folder (strings_eng.cpp, ...)
// and lists its text as {ID, "text"} pairs. English is the fallback: if a
// language is missing an entry, the English text is shown instead.
//
// To add a piece of text:
//   1. Add an ID to TextId (or PluralId) below, in the section for its screen.
//   2. Add the English text for it in strings_eng.cpp (and other languages).
//
// To add a language:
//   1. Copy strings_eng.cpp to strings_<code>.cpp and translate the text.
//   2. Declare its Language object below (next to kEnglish) and add the
//      .cpp file to CMakeLists.txt.
//   3. Call SetLanguage() with it.
//
// NOTE: include this as "localization/strings.h". Do not add the
// localization folder to the compiler's include directories: that would hide
// the system header <strings.h> that some platform headers include.
// ---------------------------------------------------------------------------

#include <cstddef>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Text IDs
//
// Placeholders: {0}, {1}, ... are replaced by Format() / TrPlural(). A
// translation may use them in any order (word order differs by language).
//
// Text used by more than one screen (or by a screen and its PDF) has one
// shared ID, so it always reads the same everywhere. Screen-specific text is
// grouped under its screen. Leading/trailing spaces used only for layout are
// added by the code, not stored here.
// ---------------------------------------------------------------------------
enum class TextId
{
    // ---- Shared names ------------------------------------------------------
    // Keep each group in order (Sunday first / January first): the lookup
    // helpers below index into them.
    WeekdayShortSun,
    WeekdayShortMon,
    WeekdayShortTue,
    WeekdayShortWed,
    WeekdayShortThu,
    WeekdayShortFri,
    WeekdayShortSat,

    MonthJanuary,
    MonthFebruary,
    MonthMarch,
    MonthApril,
    MonthMay,
    MonthJune,
    MonthJuly,
    MonthAugust,
    MonthSeptember,
    MonthOctober,
    MonthNovember,
    MonthDecember,

    MonthShortJan,
    MonthShortFeb,
    MonthShortMar,
    MonthShortApr,
    MonthShortMay,
    MonthShortJun,
    MonthShortJul,
    MonthShortAug,
    MonthShortSep,
    MonthShortOct,
    MonthShortNov,
    MonthShortDec,

    // ---- Shared formats ----------------------------------------------------
    MonthYearTitle,             // calendar heading: {0} = month name, {1} = year
    ShortDate,                  // "Sat, Oct 10": {0} = weekday, {1} = month, {2} = day
    ListSeparator,              // between items of a list: "Mon, Wed, Fri"
    DateRangeFromTo,            // {0} = first date, {1} = last date
    DateAtTime,                 // {0} = date, {1} = time
    HourAm,                     // {0} = hour 1-12: "7am"
    HourPm,                     // {0} = hour 1-12: "7pm"
    HourRange,                  // one-hour block: {0} = start, {1} = end: "7am-8am"

    // ---- Shared across screens (and PDFs) ----------------------------------
    CommonBackButton,           // "< Back"
    CommonTrendsButton,
    CommonExportPdfButton,
    CommonCurrentPoints,        // {0} = points
    CommonGoodChoices,          // heading: "Good Choices"
    CommonWrongChoices,
    CommonNoDataLogged,         // a day with nothing logged
    CommonGoldOutlineToday,     // calendar legend note

    // Table column headings
    ColumnTime,
    ColumnBehavior,
    ColumnPoints,
    ColumnTotalAfter,
    ColumnTimesLogged,
    ColumnType,

    // ---- Main screen (main.cpp) --------------------------------------------
    MainWelcome,                // "Welcome to the Behavior Tracker!"
    MainHistoryButton,

    // Default behavior buttons, created on first run
    DefaultChoiceMorningRoutine,
    DefaultChoiceFollowedDirections,
    DefaultChoiceSchoolwork,
    DefaultChoiceNegativeBehavior,
    DefaultChoiceIncompleteSchoolwork,

    // ---- "Create a choice" popup (main.cpp) --------------------------------
    AddChoiceGoodButton,        // "+  CREATE A GOOD CHOICE"
    AddChoiceWrongButton,
    AddChoiceGoodTitle,         // popup title
    AddChoiceWrongTitle,
    AddChoiceNameLabel,
    AddChoiceGoodNameHint,      // example shown in the empty name box
    AddChoiceWrongNameHint,
    AddChoicePointsToAdd,
    AddChoicePointsToSubtract,
    AddChoiceGoodPointsHelp,
    AddChoiceWrongPointsHelp,
    AddChoiceAddButton,
    AddChoiceCancelButton,

    // ---- History calendar (history_view.cpp) -------------------------------
    HistoryTitle,               // "History - Last 30 Days"
    HistoryLegendTitle,
    HistoryLegendLogged,
    HistoryLegendNoData,        // {0} = CommonNoDataLogged, as shown in the cells
    HistoryLegendBlank,
    HistoryLegendDetailIntro,   // "On a day's detail screen:"
    HistoryLegendPointsEarned,  // shown after a sample "+5"
    HistoryLegendPointsLost,    // shown after a sample "-3"
    HistoryLegendClickDay,
    HistoryLegendRetentionNote,

    // ---- Day detail (history_view.cpp) -------------------------------------
    DayDetailBackButton,        // "< Back to Calendar"
    DayDetailRemoved,           // the day has dropped out of the history
    DayDetailTitleToday,        // {0} = the date, for today's page
    DayDetailNoBehaviors,
    DayDetailEndingPoints,      // {0} = points

    // ---- PDF export popup (history_view.cpp) -------------------------------
    ExportPopupTitle,
    ExportComplete,
    ExportSavedHistory,         // "... was saved to your Downloads folder:"
    ExportSavedTrends,
    ExportOpenedInViewer,
    ExportNotOpened,
    ExportFailed,
    ExportOkButton,

    // Export errors (history_export.cpp). {0} = folder or file path
    ExportErrorNoDownloadsFolder,
    ExportErrorCreateFile,
    ExportErrorWriteFile,

    // ---- Trends screen (trends_view.cpp) -----------------------------------
    TrendsTitle,
    TrendsSubtitle,
    TrendsBackHintHistory,
    TrendsBackHintMain,
    TrendsExportHint,
    TrendsExportHintNoDays,
    TrendsTabMostLeast,         // tab name, also the PDF's "View:"
    TrendsTabMostLeastHint,
    TrendsTabTimeOfDay,
    TrendsTabTimeOfDayHint,

    // Filters: dates
    TrendsDatesLabel,           // "Dates:"
    TrendsDateRangeTo,          // between the two date boxes: "to"
    TrendsFromDateHint,
    TrendsToDateHint,
    TrendsLast7Days,
    TrendsLast7DaysHint,
    TrendsLast14Days,
    TrendsLast14DaysHint,
    TrendsResetButton,
    TrendsResetHint,

    // Filters: days of the week
    TrendsDaysLabel,            // "Days:"
    // Keep these seven together, Sunday to Saturday.
    TrendsIncludeSundaysHint,
    TrendsIncludeMondaysHint,
    TrendsIncludeTuesdaysHint,
    TrendsIncludeWednesdaysHint,
    TrendsIncludeThursdaysHint,
    TrendsIncludeFridaysHint,
    TrendsIncludeSaturdaysHint,
    TrendsAllDaysButton,
    TrendsAllDaysHint,
    TrendsAllDaysHintAlready,
    TrendsClearDaysButton,
    TrendsClearDaysHint,
    TrendsClearDaysHintNone,
    TrendsPickAtLeastOneDay,
    TrendsCountingSummary,      // {0} = TrendsLoggedBehaviors, {1} = TrendsDayCount,
                                // {2} = which days of the week

    // Calendar date picker
    TrendsPickerRangeNote,

    // "Most / Least Logged" tab
    TrendsMostFirst,
    TrendsMostFirstGoodHint,
    TrendsMostFirstWrongHint,
    TrendsLeastFirst,
    TrendsLeastFirstGoodHint,
    TrendsLeastFirstWrongHint,
    TrendsNoGoodChoicesYet,     // also in the PDF
    TrendsNoWrongChoicesYet,

    // "Time of Day" tab
    TrendsShowLabel,            // "Show:"
    TrendsSortLabel,            // "Sort:"
    TrendsKindAll,
    TrendsKindAllHint,
    TrendsKindGood,
    TrendsKindGoodHint,
    TrendsKindWrong,
    TrendsKindWrongHint,
    TrendsSortByHour,
    TrendsSortByHourHint,
    TrendsSortByCount,
    TrendsSortByCountHint,
    TrendsTimeOfDayNote,
    TrendsGreenNote,            // "Green = good choice."
    TrendsRedNote,

    // ---- Trends text shared by the screen and the PDF (trends.cpp) ---------
    WeekdaysAll,                // which days of the week are included
    WeekdaysSchoolDays,         // Monday to Friday
    WeekdaysWeekend,            // Saturday and Sunday
    WeekdaysNone,
    DescribeDateRangeNone,      // no day of the history is in the chosen range
    DescribeDaysOfWeekLabel,    // "Days of week:"
    EmptyHourAll,               // an hour with nothing logged
    EmptyHourGood,
    EmptyHourWrong,

    // ---- PDF reports (shared) ----------------------------------------------
    PdfFooter,                  // {0} = report title
    PdfPageNumber,              // {0} = page, {1} = page count
    PdfDateRangeLabel,          // "Date range:"
    PdfCreatedLabel,            // "Created:"

    // ---- History PDF (history_export.cpp) ----------------------------------
    PdfHistoryTitle,
    PdfHistoryCurrentPoints,    // "Current points:"
    PdfHistorySummaryTitle,
    PdfHistorySummaryCaption,
    PdfHistoryNoBehaviors,
    PdfHistoryDailyLogTitle,
    PdfHistoryDailyLogCaption,
    PdfHistoryDayContinued,     // {0} = the date, when a day runs onto a new page
    PdfHistoryEndingPoints,     // {0} = points

    // ---- Trends PDF (trends_export.cpp) ------------------------------------
    PdfTrendsTitle,
    PdfTrendsViewLabel,         // "View:"
    PdfTrendsDaysIncludedLabel,
    PdfTrendsShowingLabel,
    PdfTrendsSortedLabel,
    PdfTrendsGoodChoicesLabel,  // "Good choices:"
    PdfTrendsWrongChoicesLabel,
    PdfTrendsOrderMostFirst,
    PdfTrendsOrderLeastFirst,
    PdfTrendsKindAll,
    PdfTrendsKindGood,
    PdfTrendsKindWrong,
    PdfTrendsSortByHour,
    PdfTrendsSortByCount,
    PdfTrendsRankingCaptionMostFirst,
    PdfTrendsRankingCaptionLeastFirst,
    PdfTrendsTimeOfDayTitle,
    PdfTrendsTimeOfDayCaption,
    PdfTrendsTypeGood,          // "Type" column
    PdfTrendsTypeWrong,

    Count // keep last
};

// Text that changes with a number ("1 time" / "2 times"). Each language picks
// the right form for the number using its own plural rule. {0} is always the
// number; any other placeholders are listed next to the ID.
enum class PluralId
{
    TimesLogged,                // "1 time", "15 times"
    HistoryCellEnding,          // calendar cell: points at the end of the day
    DayDetailBehaviorsLogged,   // "3 behaviors logged"
    TrendsAllDaysPreset,        // preset button: "All 31 days"
    TrendsAllDaysPresetHint,    // {0} = days before today
    TrendsLoggedBehaviors,      // "68 logged behaviors" (in TrendsCountingSummary)
    TrendsDayCount,             // "31 days" (in TrendsCountingSummary)
    TrendsPickerFadedNote,      // {0} = days in the history
    DescribeDateRange,          // {1} = first date, {2} = last date, {0} = days

    Count // keep last
};

// Plural forms, as defined by the Unicode CLDR plural rules. English only
// uses One and Other; other languages may use more. Any form a language
// leaves empty falls back to Other.
enum class PluralForm
{
    Zero,
    One,
    Two,
    Few,
    Many,
    Other,

    Count // keep last
};

// ---------------------------------------------------------------------------
// Language tables (defined in strings_<code>.cpp)
// ---------------------------------------------------------------------------

struct TextEntry
{
    TextId id;
    const char *text;
};

struct PluralEntry
{
    PluralId id;
    // Indexed by PluralForm. nullptr = not used by this language.
    const char *forms[static_cast<int>(PluralForm::Count)];
};

struct Language
{
    const char *code;                       // e.g. "en"
    const char *name;                       // in the language itself, e.g. "English"
    const TextEntry *text;
    std::size_t textCount;
    const PluralEntry *plurals;
    std::size_t pluralCount;
    PluralForm (*pluralRule)(long long n);  // which form to use for n
};

extern const Language kEnglish; // strings_eng.cpp

// Switches all text to 'language'. English is used until this is called.
void SetLanguage(const Language &language);
const Language &CurrentLanguage();

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------

// The text for 'id' in the current language. Never returns nullptr.
const char *Tr(TextId id);

// One argument for Format(): text or a whole number.
struct FormatArg
{
    std::string text;

    FormatArg(const char *s) : text(s ? s : "") {}
    FormatArg(std::string s) : text(std::move(s)) {}
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    FormatArg(T n) : text(std::to_string(n)) {}
};

// Replaces {0}, {1}, ... in 'pattern' with args[0], args[1], ...
// Anything else (including a placeholder with no matching argument) is kept
// as-is. Use this instead of printf-style "%s"/"%d" so translators can
// reorder placeholders.
std::string Format(const char *pattern, std::initializer_list<FormatArg> args);

// The right plural form of 'id' for n, with {0} replaced by n and {1}, {2},
// ... by 'extra'.
std::string TrPlural(PluralId id, long long n, std::initializer_list<FormatArg> extra = {});

// Calendar names in the current language.
const char *WeekdayShortName(int weekday); // 0 = Sunday ... 6 = Saturday: "Sun"
const char *MonthName(int month);          // 1 = January ... 12: "January"
const char *MonthShortName(int month);     // 1 = January ... 12: "Jan"
