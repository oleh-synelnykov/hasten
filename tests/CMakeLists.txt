add_executable(test_idl_rules
    tests/test_idl_rules.cpp
)

find_package(Boost REQUIRED)

target_link_libraries(test_idl_rules PRIVATE
    Boost::boost
    hasten_idl
    GTest::gtest_main
)
