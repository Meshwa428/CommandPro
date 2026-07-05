# Unit tests
add_executable(syn_tests
    unit/test_lexer.cpp
    unit/test_parser.cpp
)
target_link_libraries(syn_tests
    PRIVATE
        libsynapse
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(syn_tests)

# Golden-file conformance/error/automation tests (design doc 003). Depends on
# the `syn` CLI existing at build/<config>/syn — run `cmake --build` first.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _syn_build_type_lower)
    if(_syn_build_type_lower STREQUAL "release")
        set(_syn_test_preset_flag --release)
    else()
        set(_syn_test_preset_flag --debug)
    endif()
    add_test(NAME golden_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run.py ${_syn_test_preset_flag}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
