#include "synapse/common/diag.h"
#include <iostream>

namespace syn {

void DiagEngine::error(SourceLocation loc, std::string msg, std::string label)
{
    m_diags.push_back({DiagLevel::Error, std::move(msg), loc, std::move(label)});
    m_error_count++;
}

void DiagEngine::warn(SourceLocation loc, std::string msg, std::string label)
{
    m_diags.push_back({DiagLevel::Warning, std::move(msg), loc, std::move(label)});
}

void DiagEngine::note(SourceLocation loc, std::string msg)
{
    m_diags.push_back({DiagLevel::Note, std::move(msg), loc, ""});
}

void DiagEngine::print_all(const Source& src) const
{
    for (const auto& diag : m_diags) {
        std::ostream& os = std::cerr;

        // Print header: file:line:col: level: message
        if (diag.loc.valid()) {
            os << *diag.loc.file << ":" << diag.loc.line << ":" << diag.loc.column << ": ";
        } else {
            os << src.name() << ": ";
        }

        switch (diag.level) {
            case DiagLevel::Error:   os << "\033[1;31merror:\033[0m "; break;
            case DiagLevel::Warning: os << "\033[1;35mwarning:\033[0m "; break;
            case DiagLevel::Note:    os << "\033[1;36mnote:\033[0m "; break;
        }

        os << diag.message << "\n";

        // Print line snippet and caret
        if (diag.loc.valid()) {
            std::string_view line = src.line_text(diag.loc.line);
            os << " " << diag.loc.line << " | " << line << "\n";
            
            // Build the indicator line (spaces up to column - 1, then carets)
            os << "   | ";
            for (uint32_t i = 1; i < diag.loc.column; ++i) {
                // Handle tabs correctly for spacing
                if (i <= line.size() && line[i - 1] == '\t') {
                    os << "\t";
                } else {
                    os << " ";
                }
            }
            os << "\033[1;32m^\033[0m";
            if (!diag.label.empty()) {
                os << " " << diag.label;
            }
            os << "\n";
        }
    }
}

} // namespace syn
