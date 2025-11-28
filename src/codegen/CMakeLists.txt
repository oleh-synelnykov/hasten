add_library(hasten_codegen STATIC
    generator.cpp
    ir_builder.cpp
    emitter.cpp
    file_writer.cpp
)

target_include_directories(hasten_codegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hasten_codegen PUBLIC
    hasten_frontend
    hasten_runtime
    fmt::fmt
)
