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
