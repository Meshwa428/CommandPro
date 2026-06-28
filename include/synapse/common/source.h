#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace syn {

// Forward declaration
struct SourceLocation;

// ── Source ───────────────────────────────────────────────────────────────────
// Owns the raw source text and a line-start index for fast line/col lookup.
class Source
{
public:
    Source(std::string name, std::string text)
        : m_name(std::move(name)), m_text(std::move(text))
    {
        build_line_index();
    }

    std::string_view name() const { return m_name; }
    std::string_view text() const { return m_text; }
    std::size_t      size() const { return m_text.size(); }
    char operator[](std::size_t i) const { return m_text[i]; }

    // Convert a byte offset → (line, column) — both 1-based
    SourceLocation location_of(std::size_t offset) const;

    // Return the full text of a 1-based line number (without trailing newline)
    std::string_view line_text(uint32_t one_based_line) const;

private:
    void build_line_index();

    std::string              m_name;
    std::string              m_text;
    std::vector<std::size_t> m_line_starts; // byte offset of each line's first char
};

// ── SourceLocation ────────────────────────────────────────────────────────────
struct SourceLocation
{
    uint32_t           line   = 0; // 1-based
    uint32_t           column = 0; // 1-based
    std::size_t        offset = 0; // byte offset into source
    const std::string* file   = nullptr;

    bool valid() const { return line > 0; }
};

// ── Span ─────────────────────────────────────────────────────────────────────
// Half-open byte range [start, end) into a source.
struct Span
{
    std::size_t start = 0;
    std::size_t end   = 0;

    std::size_t  length()  const { return end - start; }
    bool         empty()   const { return start == end; }
    std::string_view view(const Source& src) const
    {
        return std::string_view(src.text()).substr(start, length());
    }
};

} // namespace syn
