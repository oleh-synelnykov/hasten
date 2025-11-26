add_library(hasten_cli STATIC
    hasten.cpp
    options.cpp
)

target_include_directories(hasten_cli PUBLIC
    include
)

target_link_libraries(hasten_cli PUBLIC
    hasten_idl
    boost::boost
    fmt::fmt
    spdlog::spdlog
)
