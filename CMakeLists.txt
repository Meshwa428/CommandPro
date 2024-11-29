cmake_minimum_required(VERSION 3.14)
project(CommandPro)

#find_program(CCACHE_PROGRAM ccache)
#if(CCACHE_PROGRAM) 
#  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "{$CCACHE_PROGRAM}")
#endif()

# Export compilation database in JSON format.
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

# Define project directories
#set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")


#set(SOURCE_FILES
#    ${SOURCE_DIR}/main.c
#)

# Create the executable
add_executable(CommandPro src/main.c)