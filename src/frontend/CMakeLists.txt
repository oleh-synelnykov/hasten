add_library(hasten_frontend STATIC
    diagnostic.cpp
    frontend.cpp
    semantic/declaration_index_pass.cpp
    semantic/enum_validation_pass.cpp
    semantic/interface_validation_pass.cpp
    semantic/module_index_pass.cpp
    semantic/semantic_context.cpp
    semantic/struct_validation_pass.cpp
    semantic/type_validator.cpp
    semantic/validator.cpp
)

target_include_directories(hasten_frontend PUBLIC
    "include"
)

target_link_libraries(hasten_frontend PUBLIC
    hasten_idl
)
