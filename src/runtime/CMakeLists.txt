cmake_minimum_required(VERSION 3.30)

add_library(hasten_runtime INTERFACE)

target_include_directories(hasten_runtime INTERFACE
    include
)
