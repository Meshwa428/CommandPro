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
)
target_include_directories(libsynapse PUBLIC include)
target_compile_features(libsynapse PUBLIC cxx_std_20)
target_link_libraries(libsynapse PUBLIC dl)

# ── CLI executable ────────────────────────────────────────────────────────────
# jit_runtime.cpp is compiled into the executable (not libsynapse) so its
# syn_rt_* symbols are present in the dynamic symbol table and resolvable by
# dlopened JIT modules. ENABLE_EXPORTS adds -rdynamic / --export-dynamic.
add_executable(syn src/main.cpp src/backend/jit_runtime.cpp)
target_link_libraries(syn PRIVATE libsynapse)
set_target_properties(syn PROPERTIES ENABLE_EXPORTS ON)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
