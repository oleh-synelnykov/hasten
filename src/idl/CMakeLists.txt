add_library(hasten_idl STATIC
    parser.cpp
    rules.cpp
    json_dump.cpp
)

target_include_directories(hasten_idl PUBLIC
    include
)

find_package(Boost REQUIRED)

target_link_libraries(hasten_idl PUBLIC
    Boost::boost
    nlohmann_json::nlohmann_json
)
