add_library(hasten_frontend STATIC
    frontend.cpp
)

target_include_directories(hasten_frontend PUBLIC
    "include"
)

target_link_libraries(hasten_frontend PUBLIC
    hasten_idl
)
