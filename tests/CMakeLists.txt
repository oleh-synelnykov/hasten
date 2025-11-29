add_executable(test_hasten
    test_cli_options.cpp
    test_cli.cpp
    test_codegen.cpp
    test_frontend.cpp
    test_frontend_detail.cpp
    test_idl_rules.cpp
    test_parser.cpp
    test_semantic.cpp
    test_runtime.cpp
    test_runtime_serialization.cpp
    test_executor.cpp
)

find_package(Boost REQUIRED)

target_link_libraries(test_hasten PRIVATE
    Boost::boost
    GTest::gtest_main
    hasten_cli
    hasten_frontend
    hasten_codegen
    hasten_runtime
)

add_test(NAME test_hasten COMMAND test_hasten)
