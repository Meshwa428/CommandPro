cmake_minimum_required(VERSION 3.14)
project(CommandPro C)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Export compilation database for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define directories
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

# Gather source files
file(GLOB_RECURSE SOURCES
    "${SRC_DIR}/*.c"
)

# Create the executable
add_executable(CommandPro ${SOURCES})

# Add include directories to the target
target_include_directories(CommandPro
    PRIVATE
        ${INCLUDE_DIR}
)

# Optional: Add compiler warnings
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(CommandPro PRIVATE 
        -Wall 
        -Wextra 
        -Werror
        -Wno-unused-parameter  # Ignore unused parameter warnings
    )
endif()

# Optional: Use CCache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found CCache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()