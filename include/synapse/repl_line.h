#pragma once
#include <string>

// REPL line input, isolated behind this interface so readline's headers (which
// #define pervasive ALL-CAPS macros like RETURN that collide with our opcode
// names) never leak into the rest of the codebase. Implementation in
// src/repl_line.cpp; falls back to std::getline when readline isn't compiled in.
namespace syn {

// Prints `prompt`, reads one line into `out`. Returns false on EOF (Ctrl-D).
// With readline: arrow-key editing and history; the line is added to history.
bool repl_read_line(const char* prompt, std::string& out);

void repl_history_load(const char* path);
void repl_history_save(const char* path);

}  // namespace syn
