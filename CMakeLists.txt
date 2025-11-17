cmake_minimum_required(VERSION 3.30)

project(hasten CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)

# Hasten IDL static library
add_subdirectory(src/idl)

# Hasten executable
add_executable(hasten
    src/main.cpp
)

target_link_libraries(hasten
    hasten_idl
    fmt::fmt
    spdlog::spdlog
)

# Hasten IDL rules test executable
add_executable(test_idl_rules
    tests/test_idl_rules.cpp
)

target_link_libraries(test_idl_rules
    PRIVATE
        hasten_idl
        GTest::gtest_main
)

install(TARGETS hasten DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
