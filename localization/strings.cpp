#include "strings.h"

#include <array>
#include <cctype>
#include <vector>

// Language-independent lookup code. The text itself is in strings_<code>.cpp.

namespace
{
constexpr int kTextCount = static_cast<int>(TextId::Count);
constexpr int kPluralCount = static_cast<int>(PluralId::Count);
constexpr int kFormCount = static_cast<int>(PluralForm::Count);
constexpr int kOther = static_cast<int>(PluralForm::Other);

static_assert(static_cast<int>(TextId::WeekdayShortSat) -
                  static_cast<int>(TextId::WeekdayShortSun) == 6,
              "Weekday IDs must stay together, Sunday to Saturday");
static_assert(static_cast<int>(TextId::MonthDecember) -
                  static_cast<int>(TextId::MonthJanuary) == 11,
              "Month IDs must stay together, January to December");
static_assert(static_cast<int>(TextId::MonthShortDec) -
                  static_cast<int>(TextId::MonthShortJan) == 11,
              "Short month IDs must stay together, January to December");
static_assert(static_cast<int>(TextId::TrendsIncludeSaturdaysHint) -
                  static_cast<int>(TextId::TrendsIncludeSundaysHint) == 6,
              "Weekday hint IDs must stay together, Sunday to Saturday");

// The current language's text, indexed by ID, with English filling any gaps.
struct ResolvedTables
{
    const Language *language = nullptr;
    std::array<const char *, kTextCount> text{};
    std::array<std::array<const char *, kFormCount>, kPluralCount> plurals{};
};

void Fill(ResolvedTables &tables, const Language &language)
{
    for (std::size_t i = 0; i < language.textCount; ++i)
    {
        const TextEntry &entry = language.text[i];
        const int id = static_cast<int>(entry.id);
        if (id >= 0 && id < kTextCount && entry.text != nullptr)
            tables.text[id] = entry.text;
    }
    for (std::size_t i = 0; i < language.pluralCount; ++i)
    {
        const PluralEntry &entry = language.plurals[i];
        const int id = static_cast<int>(entry.id);
        if (id < 0 || id >= kPluralCount || entry.forms[kOther] == nullptr)
            continue; // a usable entry always has the Other form
        for (int form = 0; form < kFormCount; ++form)
            tables.plurals[id][form] = entry.forms[form];
    }
}

ResolvedTables Resolve(const Language &language)
{
    ResolvedTables tables;
    tables.language = &language;
    if (&language != &kEnglish)
        Fill(tables, kEnglish);
    Fill(tables, language); // overrides English wherever it has text
    return tables;
}

// Built on first use, so Tr() works before SetLanguage() is ever called.
ResolvedTables &Tables()
{
    static ResolvedTables tables = Resolve(kEnglish);
    return tables;
}

// Shown instead of crashing if an ID has no text at all (a missing English
// entry). Visible on screen, so it gets noticed and fixed.
const char *kMissingText = "[missing text]";

// Replaces {i} with args[i]. Shared by Format() and TrPlural().
std::string FormatArgs(const char *pattern, const FormatArg *args, std::size_t count)
{
    std::string result;
    if (pattern == nullptr)
        return result;

    for (const char *p = pattern; *p != '\0';)
    {
        if (*p == '{' && std::isdigit(static_cast<unsigned char>(p[1])))
        {
            // Read "{digits}"
            const char *q = p + 1;
            std::size_t index = 0;
            while (std::isdigit(static_cast<unsigned char>(*q)) && index < 1000)
                index = index * 10 + static_cast<std::size_t>(*q++ - '0');
            if (*q == '}' && index < count)
            {
                result += args[index].text;
                p = q + 1;
                continue;
            }
        }
        result += *p++;
    }
    return result;
}
} // namespace

void SetLanguage(const Language &language)
{
    Tables() = Resolve(language);
}

const Language &CurrentLanguage()
{
    return *Tables().language;
}

const char *Tr(TextId id)
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= kTextCount)
        return kMissingText;
    const char *text = Tables().text[index];
    return text != nullptr ? text : kMissingText;
}

std::string Format(const char *pattern, std::initializer_list<FormatArg> args)
{
    return FormatArgs(pattern, args.begin(), args.size());
}

std::string TrPlural(PluralId id, long long n, std::initializer_list<FormatArg> extra)
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= kPluralCount)
        return kMissingText;

    const ResolvedTables &tables = Tables();
    const auto &forms = tables.plurals[index];
    int form = static_cast<int>(tables.language->pluralRule(n));
    if (form < 0 || form >= kFormCount || forms[form] == nullptr)
        form = kOther;
    const char *pattern = forms[form];
    if (pattern == nullptr)
        return kMissingText;

    // {0} is the number itself; 'extra' fills {1}, {2}, ...
    std::vector<FormatArg> args;
    args.reserve(extra.size() + 1);
    args.emplace_back(n);
    args.insert(args.end(), extra.begin(), extra.end());
    return FormatArgs(pattern, args.data(), args.size());
}

const char *WeekdayShortName(int weekday)
{
    if (weekday < 0 || weekday > 6)
        return kMissingText;
    return Tr(static_cast<TextId>(static_cast<int>(TextId::WeekdayShortSun) + weekday));
}

const char *MonthName(int month)
{
    if (month < 1 || month > 12)
        return kMissingText;
    return Tr(static_cast<TextId>(static_cast<int>(TextId::MonthJanuary) + month - 1));
}

const char *MonthShortName(int month)
{
    if (month < 1 || month > 12)
        return kMissingText;
    return Tr(static_cast<TextId>(static_cast<int>(TextId::MonthShortJan) + month - 1));
}
