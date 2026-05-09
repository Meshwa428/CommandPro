cmake_minimum_required(VERSION 3.10)
project(Synapse)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SYNAPSE_PROFILER "Enable VM profiling and telemetry" OFF)
if (SYNAPSE_PROFILER)
    add_definitions(-DSYNAPSE_PROFILER)
endif()

include_directories(include)

file(GLOB_RECURSE CORE_SRC
    "src/*.cpp"
)

add_executable(synapse ${CORE_SRC})

if (UNIX AND NOT APPLE)
    target_link_libraries(synapse X11 Xtst)
elseif (WIN32)
    # target_link_libraries(synapse user32 gdi32)
endif()
