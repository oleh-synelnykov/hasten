add_library(hasten_frontend STATIC
    diagnostic.cpp
    frontend.cpp
    semantics/validator_impl.cpp
    semantics/validator.cpp
)

target_include_directories(hasten_frontend PUBLIC
    "include"
)

target_link_libraries(hasten_frontend PUBLIC
    hasten_idl
)
