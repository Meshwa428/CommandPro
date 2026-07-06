cmake_minimum_required(VERSION 3.25)
project(synapse VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler flags ────────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-g>
        $<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
    )
endif()

# ── ASan / UBSan presets ──────────────────────────────────────────────────────
option(SYN_ASAN  "Enable AddressSanitizer"   OFF)
option(SYN_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(SYN_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(SYN_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# ── Catch2 (test framework) ───────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

# ── Source library (libsynapse) ───────────────────────────────────────────────
add_library(libsynapse STATIC
    src/common/source.cpp
    src/common/diag.cpp
    src/frontend/token.cpp
    src/frontend/lexer.cpp
    src/frontend/parser.cpp
    src/runtime/value.cpp
    src/runtime/chunk.cpp
    src/runtime/vm.cpp
    src/compiler/compiler.cpp
    src/backend/jit.cpp
    src/platform/mock/mock_platform.cpp
    src/runtime/automation_stdlib.cpp
    src/rat/rat_model.cpp
)
target_include_directories(libsynapse PRIVATE src/platform/mock)
target_include_directories(libsynapse PUBLIC include)
target_compile_features(libsynapse PUBLIC cxx_std_20)
if(UNIX)
    target_link_libraries(libsynapse PUBLIC dl)
endif()

# ── Linux platform backend (uinput/X11 real automation) ──────────────────────
# X11-only for now (XTest for input, Xlib+EWMH for windows); Wayland/uinput and
# Windows/macOS backends are future work (design 002 §7). MockPlatform above
# always builds regardless of platform, so `syn` still builds everywhere.
#
# Built as a SEPARATE plugin (dlopen'd at runtime by main.cpp, only for
# scripts that actually use automation — see script_uses_automation()), not
# linked into libsynapse/syn directly: linking X11+Xtst+PNG into the main
# binary pulls in 12 extra transitive .so's (libX11, libxcb, libpng16, ...)
# that the dynamic linker loads on *every* process start regardless of
# whether the script ever touches automation — measured as a real ~1ms/run
# fixed cost across the whole benchmark suite. dlopen defers that cost to
# only the processes that need it.
if(UNIX AND NOT APPLE)
    find_package(X11)
    find_package(PNG)
    if(X11_FOUND AND X11_Xtst_FOUND AND PNG_FOUND)
        add_library(syn_linux_platform MODULE src/platform/linux/linux_platform.cpp)
        target_include_directories(syn_linux_platform PRIVATE include)
        target_compile_features(syn_linux_platform PRIVATE cxx_std_20)
        target_link_libraries(syn_linux_platform PRIVATE ${X11_LIBRARIES} ${X11_Xtst_LIB} PNG::PNG)
        set_target_properties(syn_linux_platform PROPERTIES PREFIX "")
    endif()
endif()

# ── Windows platform backend (SendInput/EnumWindows/GDI+ automation) ─────────
# Same plugin pattern as Linux: LoadLibrary'd at runtime by main.cpp only for
# scripts that use automation. All deps (user32/gdi32/gdiplus/shell32) ship
# with Windows. NOTE: the rest of the tree (JIT mmap, dlfcn in main.cpp) is
# not Windows-ported yet — this target is the automation backend half of the
# port, kept building-ready for the first Windows CI run.
if(WIN32)
    add_library(syn_windows_platform MODULE src/platform/windows/windows_platform.cpp)
    target_include_directories(syn_windows_platform PRIVATE include)
    target_compile_features(syn_windows_platform PRIVATE cxx_std_20)
    target_link_libraries(syn_windows_platform PRIVATE user32 gdi32 gdiplus shell32)
    set_target_properties(syn_windows_platform PROPERTIES PREFIX "")
endif()

# ── CLI executable ────────────────────────────────────────────────────────────
# jit_runtime.cpp is compiled into the executable (not libsynapse) so its
# syn_rt_* symbols are present in the dynamic symbol table and resolvable by
# dlopened JIT modules. ENABLE_EXPORTS adds -rdynamic / --export-dynamic.
add_executable(syn src/main.cpp src/backend/jit_runtime.cpp src/repl_line.cpp)
target_link_libraries(syn PRIVATE libsynapse)
set_target_properties(syn PROPERTIES ENABLE_EXPORTS ON)

# Optional GNU readline for REPL line editing + history. Falls back to plain
# getline (no arrow keys/history) when unavailable, so it's not required. Only
# src/repl_line.cpp sees readline's headers (its macros collide with opcode
# names), so the define is scoped to that source file.
find_library(READLINE_LIB readline)
find_path(READLINE_INCLUDE readline/readline.h)
if(READLINE_LIB AND READLINE_INCLUDE)
    set_source_files_properties(src/repl_line.cpp PROPERTIES
        COMPILE_DEFINITIONS SYN_HAVE_READLINE
        INCLUDE_DIRECTORIES ${READLINE_INCLUDE})
    target_link_libraries(syn PRIVATE ${READLINE_LIB})
    message(STATUS "REPL: GNU readline enabled (${READLINE_LIB})")
else()
    message(STATUS "REPL: readline not found — using plain getline")
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
