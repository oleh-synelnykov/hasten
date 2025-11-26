add_executable(test_hasten_idl
    test_cli_options.cpp
    test_cli.cpp
    test_idl_rules.cpp
    test_parser.cpp
)

find_package(Boost REQUIRED)

target_link_libraries(test_hasten_idl PRIVATE
    Boost::boost
    GTest::gtest_main
    hasten_cli
)
