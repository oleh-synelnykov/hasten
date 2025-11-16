cmake_minimum_required(VERSION 3.30)

project(hasten CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_executable(hasten
    src/main.cpp
)

target_link_libraries(hasten
    boost::boost
    fmt::fmt
    spdlog::spdlog
)

install(TARGETS hasten DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
