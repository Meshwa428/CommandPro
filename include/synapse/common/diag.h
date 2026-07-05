#pragma once
#include <string>
#include <vector>
#include "synapse/common/source.h"
#include "synapse/common/rt_error.h"

namespace syn {

// Prints a RuntimeError clang-style: file:line, source snippet, then the
// Synapse-level call stack. Declared here so both compile-time diagnostics
// and runtime errors share one formatter.
void print_runtime_error(const RuntimeError& err, const Source& src);

enum class DiagLevel { Error, Warning, Note };

struct Diagnostic
{
    DiagLevel    level;
    std::string  message;
    SourceLocation loc;
    std::string  label; // underline label (optional)
};

// Collects diagnostics during compilation.
// Errors abort the current phase when emitted via require_ok().
class DiagEngine
{
public:
    void error(SourceLocation loc, std::string msg, std::string label = {});
    void warn (SourceLocation loc, std::string msg, std::string label = {});
    void note (SourceLocation loc, std::string msg);

    bool     has_errors() const { return m_error_count > 0; }
    unsigned error_count() const { return m_error_count; }

    // Print all diagnostics to stderr in clang-style format
    void print_all(const Source& src) const;

    const std::vector<Diagnostic>& diagnostics() const { return m_diags; }

private:
    std::vector<Diagnostic> m_diags;
    unsigned                m_error_count = 0;
};

} // namespace syn
