cmake_minimum_required(VERSION 3.15)
project(CommandPro)

set(CMAKE_C_STANDARD 99)

# Source files
set(SOURCE_FILES
    src/main.c
    src/lexer.c
)

# Include directories
include_directories(include)

# Add executable
add_executable(MyLexer ${SOURCE_FILES})
