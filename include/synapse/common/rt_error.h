#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "synapse/runtime/value.h"

namespace syn {

// One entry per live call frame at throw time, innermost first.
struct TraceEntry {
    std::string fn_name;  // "__main__" for the top-level frame
    uint32_t    line;      // 0 = unknown
};

// Runtime-level error: carries a stable code + best-effort source line +
// Synapse call stack, so it can be printed clang-style like compile diagnostics
// (see print_runtime_error in common/diag.h) instead of a bare message.
//
// `payload` is the Value a `catch (e)` clause binds `e` to: for `throw expr`
// it's exactly the thrown value (any type — string, map, custom object, not
// just a string), for VM-internal errors (E01xx) it's an ObjError wrapping
// the message + code so catch code can inspect `.message`/`.type`.
class RuntimeError : public std::runtime_error {
public:
    RuntimeError(std::string code, std::string msg, uint32_t line,
                 std::vector<TraceEntry> trace, Value payload)
        : std::runtime_error(std::move(msg)), m_code(std::move(code)),
          m_line(line), m_trace(std::move(trace)), m_payload(payload) {}

    const std::string& code()  const { return m_code; }
    uint32_t            line()  const { return m_line; }
    const std::vector<TraceEntry>& trace() const { return m_trace; }
    Value payload() const { return m_payload; }

private:
    std::string             m_code;
    uint32_t                m_line;
    std::vector<TraceEntry> m_trace;
    Value                   m_payload;
};

} // namespace syn
