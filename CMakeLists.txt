cmake_minimum_required(VERSION 3.30)

project(hasten CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GTest REQUIRED)

# Hasten IDL static library
add_subdirectory(src/idl)

# Hasten Frontend static library
add_subdirectory(src/frontend)

# Hasten CLI static library
add_subdirectory(src/cli)

# Hasten executable
add_executable(hasten
    src/main.cpp
)

target_link_libraries(hasten
    hasten_cli
    hasten_frontend
    boost::boost
    fmt::fmt
    spdlog::spdlog
)

# Tests
add_subdirectory(tests)

install(TARGETS hasten DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
