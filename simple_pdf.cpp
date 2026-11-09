#include "simple_pdf.h"

#include <cstdio>
#include <sstream>

namespace
{
// Glyph widths (1/1000 em) for characters 32..126, WinAnsiEncoding.
const short kHelveticaWidths[95] = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
    1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
    333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584};

const short kHelveticaBoldWidths[95] = {
    278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333, 584, 584, 584, 611,
    975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556,
    333, 556, 611, 556, 611, 556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611,
    611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584};

// Only printable ASCII is supported by this minimal writer.
char sanitize(char c)
{
    return (c >= 32 && c <= 126) ? c : '?';
}

// Escapes a string for use inside a PDF literal string: ( ) and backslash.
std::string escapePdfString(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char raw : s)
    {
        const char c = sanitize(raw);
        if (c == '(' || c == ')' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

std::string num(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

std::string colorOp(PdfDocument::Color c, bool fill)
{
    return num(c.r) + " " + num(c.g) + " " + num(c.b) + (fill ? " rg\n" : " RG\n");
}
} // namespace

PdfDocument::PdfDocument(float pageWidth, float pageHeight)
    : m_width(pageWidth), m_height(pageHeight)
{
}

void PdfDocument::addPage()
{
    m_pages.emplace_back();
    m_current = static_cast<int>(m_pages.size()) - 1;
}

float PdfDocument::textWidth(const std::string &s, Font font, float size)
{
    const short *widths = (font == Font::Bold) ? kHelveticaBoldWidths : kHelveticaWidths;
    long total = 0;
    for (char raw : s)
        total += widths[sanitize(raw) - 32];
    return total * size / 1000.0f;
}

void PdfDocument::text(float x, float baselineY, const std::string &s,
                       Font font, float size, Color color)
{
    std::string &c = content();
    c += "BT\n";
    c += colorOp(color, true);
    c += std::string(font == Font::Bold ? "/F2 " : "/F1 ") + num(size) + " Tf\n";
    c += num(x) + " " + num(m_height - baselineY) + " Td\n";
    c += "(" + escapePdfString(s) + ") Tj\nET\n";
}

void PdfDocument::textRight(float rightX, float baselineY, const std::string &s,
                            Font font, float size, Color color)
{
    text(rightX - textWidth(s, font, size), baselineY, s, font, size, color);
}

void PdfDocument::fillRect(float x, float y, float w, float h, Color color)
{
    std::string &c = content();
    c += colorOp(color, true);
    c += num(x) + " " + num(m_height - y - h) + " " + num(w) + " " + num(h) + " re f\n";
}

void PdfDocument::line(float x1, float y1, float x2, float y2, float width, Color color)
{
    std::string &c = content();
    c += colorOp(color, false);
    c += num(width) + " w\n";
    c += num(x1) + " " + num(m_height - y1) + " m " + num(x2) + " " + num(m_height - y2) + " l S\n";
}

std::string PdfDocument::serialize() const
{
    // Object layout:
    //   1 = Catalog, 2 = Pages, 3 = Helvetica, 4 = Helvetica-Bold, 5 = Info
    //   then for each page: page object, content stream object
    const int pageCountValue = static_cast<int>(m_pages.size());
    const int firstPageObj = 6;
    const int objectCount = 5 + pageCountValue * 2;

    std::ostringstream out;
    std::vector<long> offsets(objectCount + 1, 0);

    out << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n"; // binary marker comment

    auto beginObj = [&](int id)
    {
        offsets[id] = static_cast<long>(out.tellp());
        out << id << " 0 obj\n";
    };

    beginObj(1);
    out << "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    beginObj(2);
    out << "<< /Type /Pages /Count " << pageCountValue << " /Kids [";
    for (int i = 0; i < pageCountValue; ++i)
        out << (firstPageObj + i * 2) << " 0 R ";
    out << "] >>\nendobj\n";

    beginObj(3);
    out << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n";

    beginObj(4);
    out << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>\nendobj\n";

    beginObj(5);
    out << "<< /Title (" << escapePdfString(m_title) << ") /Producer (Behavior Tracker) >>\nendobj\n";

    for (int i = 0; i < pageCountValue; ++i)
    {
        const int pageObj = firstPageObj + i * 2;
        const int contentObj = pageObj + 1;

        beginObj(pageObj);
        out << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << num(m_width) << " " << num(m_height)
            << "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents "
            << contentObj << " 0 R >>\nendobj\n";

        beginObj(contentObj);
        out << "<< /Length " << m_pages[i].size() << " >>\nstream\n"
            << m_pages[i] << "\nendstream\nendobj\n";
    }

    const long xrefOffset = static_cast<long>(out.tellp());
    out << "xref\n0 " << (objectCount + 1) << "\n";
    out << "0000000000 65535 f \n";
    for (int id = 1; id <= objectCount; ++id)
    {
        char entry[32];
        std::snprintf(entry, sizeof(entry), "%010ld 00000 n \n", offsets[id]);
        out << entry;
    }
    out << "trailer\n<< /Size " << (objectCount + 1) << " /Root 1 0 R /Info 5 0 R >>\n"
        << "startxref\n" << xrefOffset << "\n%%EOF\n";

    return out.str();
}
