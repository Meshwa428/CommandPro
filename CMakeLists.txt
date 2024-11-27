cmake_minimum_required(VERSION 3.10)
project(CommandParser)

set(CMAKE_CXX_STANDARD 17)

# Include the nlohmann/json library
include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.10.5
)
FetchContent_MakeAvailable(json)

add_executable(CommandParser main.cpp Lexer.cpp Parser.cpp)
target_include_directories(CommandParser PRIVATE ${json_SOURCE_DIR}/include)
