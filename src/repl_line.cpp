#include "synapse/repl_line.h"

#ifdef SYN_HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <cstdlib>  // free
#else
#include <iostream>
#endif

namespace syn {

bool repl_read_line(const char* prompt, std::string& out)
{
#ifdef SYN_HAVE_READLINE
    char* raw = readline(prompt);
    if (!raw) return false;           // Ctrl-D / EOF
    out = raw;
    if (*raw) add_history(raw);
    free(raw);
    return true;
#else
    std::cout << prompt;
    return bool(std::getline(std::cin, out));
#endif
}

void repl_history_load(const char* path)
{
#ifdef SYN_HAVE_READLINE
    read_history(path);
#else
    (void)path;
#endif
}

void repl_history_save(const char* path)
{
#ifdef SYN_HAVE_READLINE
    write_history(path);
#else
    (void)path;
#endif
}

}  // namespace syn
