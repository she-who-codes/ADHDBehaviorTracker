#pragma once

#include <string>
#include <vector>

// Minimal PDF writer: text, filled rectangles and lines on any number of
// pages, using the built-in Helvetica fonts (no font files or external
// libraries needed). Coordinates are in points (1/72 inch) measured from
// the TOP-LEFT corner of the page; text y is the baseline.
class PdfDocument
{
public:
    enum class Font
    {
        Regular, // Helvetica
        Bold,    // Helvetica-Bold
    };

    struct Color
    {
        float r, g, b; // 0..1
    };

    static constexpr Color kBlack{0.0f, 0.0f, 0.0f};

    // Default size is US Letter (8.5 x 11 in).
    explicit PdfDocument(float pageWidth = 612.0f, float pageHeight = 792.0f);

    float pageWidth() const { return m_width; }
    float pageHeight() const { return m_height; }

    void setTitle(const std::string &title) { m_title = title; }

    // Starts a new page and makes it current.
    void addPage();
    int pageCount() const { return static_cast<int>(m_pages.size()); }
    // Makes an existing page current (e.g. to add "Page X of Y" footers).
    void setCurrentPage(int index) { m_current = index; }

    void text(float x, float baselineY, const std::string &s,
              Font font, float size, Color color = kBlack);
    // Draws text so that it ENDS at rightX.
    void textRight(float rightX, float baselineY, const std::string &s,
                   Font font, float size, Color color = kBlack);
    void fillRect(float x, float y, float w, float h, Color color);
    void line(float x1, float y1, float x2, float y2, float width, Color color);

    // Width of a string in points for the given font and size.
    static float textWidth(const std::string &s, Font font, float size);

    // The complete PDF file contents.
    std::string serialize() const;

private:
    std::string &content() { return m_pages[m_current]; }

    float m_width;
    float m_height;
    std::string m_title;
    std::vector<std::string> m_pages; // one content stream per page
    int m_current = -1;
};
