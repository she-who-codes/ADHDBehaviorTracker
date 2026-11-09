#pragma once

// Shared page layout for the PDF reports (Behavior History and Behavior
// Trends), so both use the same margins, fonts, colours and page footers.

#include <string>
#include <utility>
#include <vector>

#include "simple_pdf.h"

namespace pdf_report
{
using Font = PdfDocument::Font;
using Color = PdfDocument::Color;

// ---- Page layout (points; US Letter = 612 x 792) ----------------------------
inline constexpr float kMargin = 54.0f;                       // 0.75 inch
inline constexpr float kPageW = 612.0f;
inline constexpr float kPageH = 792.0f;
inline constexpr float kContentW = kPageW - 2 * kMargin;
inline constexpr float kBottomLimit = kPageH - kMargin - 10;  // keep clear of the footer
inline constexpr float kRowH = 18.0f;
inline constexpr float kBodySize = 10.5f;

// ---- Colours ----------------------------------------------------------------
inline constexpr Color kText{0.10f, 0.10f, 0.12f};
inline constexpr Color kMuted{0.42f, 0.44f, 0.50f};
inline constexpr Color kRule{0.75f, 0.77f, 0.82f};
inline constexpr Color kHeaderFill{0.90f, 0.91f, 0.95f};
inline constexpr Color kDayBandFill{0.84f, 0.87f, 0.96f};
inline constexpr Color kZebraFill{0.965f, 0.97f, 0.98f};
inline constexpr Color kGreen{0.08f, 0.52f, 0.24f};
inline constexpr Color kRed{0.80f, 0.13f, 0.13f};

// Lays out content top-to-bottom, starting new pages as needed.
class ReportWriter
{
public:
    PdfDocument doc;
    float y = kMargin;

    ReportWriter() { doc.addPage(); }

    // Starts a new page if 'height' more points won't fit. Returns true if
    // a page break happened.
    bool ensureSpace(float height)
    {
        if (y + height <= kBottomLimit)
            return false;
        doc.addPage();
        y = kMargin;
        return true;
    }

    void rule()
    {
        doc.line(kMargin, y, kMargin + kContentW, y, 0.75f, kRule);
        y += 14;
    }

    void sectionTitle(const std::string &title, const std::string &caption)
    {
        ensureSpace(60);
        doc.text(kMargin, y + 14, title, Font::Bold, 15, kText);
        y += 22;
        doc.text(kMargin, y + 10, caption, Font::Regular, 9.5f, kMuted);
        y += 20;
    }

    // A shaded header row with column titles.
    void tableHeader(const std::vector<std::pair<std::string, float>> &leftCols,
                     const std::vector<std::pair<std::string, float>> &rightCols)
    {
        doc.fillRect(kMargin, y, kContentW, kRowH, kHeaderFill);
        for (const auto &c : leftCols)
            doc.text(c.second, y + 12.5f, c.first, Font::Bold, 9.5f, kText);
        for (const auto &c : rightCols)
            doc.textRight(c.second, y + 12.5f, c.first, Font::Bold, 9.5f, kText);
        y += kRowH;
    }

    void zebra(int rowIndex)
    {
        if (rowIndex % 2 == 1)
            doc.fillRect(kMargin, y, kContentW, kRowH, kZebraFill);
    }

    // "Behavior Tracker - <reportName>" and "Page X of Y" on every page.
    void footers(const std::string &reportName)
    {
        const int pages = doc.pageCount();
        for (int i = 0; i < pages; ++i)
        {
            doc.setCurrentPage(i);
            const float fy = kPageH - 34;
            doc.line(kMargin, fy - 12, kMargin + kContentW, fy - 12, 0.5f, kRule);
            doc.text(kMargin, fy, "Behavior Tracker - " + reportName,
                     Font::Regular, 8.5f, kMuted);
            doc.textRight(kMargin + kContentW, fy,
                          "Page " + std::to_string(i + 1) + " of " + std::to_string(pages),
                          Font::Regular, 8.5f, kMuted);
        }
    }
};
} // namespace pdf_report
