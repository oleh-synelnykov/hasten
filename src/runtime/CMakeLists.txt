cmake_minimum_required(VERSION 3.30)

add_library(hasten_runtime STATIC
    context.cpp
    frame.cpp
    executor.cpp
    serialization/hb1.cpp
    uds.cpp
)

target_include_directories(hasten_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(hasten_runtime PUBLIC
    pthread
)

install(TARGETS hasten_runtime
        EXPORT hasten_runtimeTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(DIRECTORY include/
        DESTINATION include)

install(EXPORT hasten_runtimeTargets
        FILE hasten_runtimeTargets.cmake
        NAMESPACE hasten::
        DESTINATION lib/cmake/hasten_runtime)
