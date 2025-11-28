cmake_minimum_required(VERSION 3.30)
project(hasten_example_echo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}") # or let hasten do it

execute_process(
    COMMAND hasten --input-file "${CMAKE_CURRENT_SOURCE_DIR}/echo.hidl" --output-dir "${GENERATED_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND_ERROR_IS_FATAL ANY
)

include("${GENERATED_DIR}/generated.cmake")

add_executable(echo_client client_main.cpp)
target_include_directories(echo_client PRIVATE "${GENERATED_DIR}")
target_link_libraries(echo_client PRIVATE
    hasten_runtime::hasten_runtime
    hasten_sample_core_common)
target_sources(echo_client PRIVATE $<TARGET_OBJECTS:hasten_sample_core_Echo_client>)

add_executable(echo_server server_main.cpp)
target_include_directories(echo_server PRIVATE "${GENERATED_DIR}")
target_link_libraries(echo_server PRIVATE
    hasten_runtime::hasten_runtime
    hasten_sample_core_common)
target_sources(echo_server PRIVATE $<TARGET_OBJECTS:hasten_sample_core_Echo_server>)
