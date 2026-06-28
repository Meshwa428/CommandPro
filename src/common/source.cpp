#include "synapse/common/source.h"

namespace syn {

SourceLocation Source::location_of(std::size_t offset) const
{
    // Binary search through line start offsets
    std::size_t lo = 0, hi = m_line_starts.size();
    while (lo + 1 < hi) {
        std::size_t mid = (lo + hi) / 2;
        if (m_line_starts[mid] <= offset)
            lo = mid;
        else
            hi = mid;
    }
    uint32_t line   = static_cast<uint32_t>(lo) + 1;
    uint32_t column = static_cast<uint32_t>(offset - m_line_starts[lo]) + 1;
    return {line, column, offset, &m_name};
}

void Source::build_line_index()
{
    m_line_starts.push_back(0);
    for (std::size_t i = 0; i < m_text.size(); ++i) {
        if (m_text[i] == '\n') {
            m_line_starts.push_back(i + 1);
        }
    }
}

std::string_view Source::line_text(uint32_t one_based_line) const
{
    if (one_based_line == 0 || one_based_line > m_line_starts.size())
        return {};
    std::size_t start = m_line_starts[one_based_line - 1];
    std::size_t end   = (one_based_line < m_line_starts.size())
                          ? m_line_starts[one_based_line]
                          : m_text.size();
    // strip trailing newline for display
    if (end > start && m_text[end - 1] == '\n') --end;
    return std::string_view(m_text).substr(start, end - start);
}

} // namespace syn
