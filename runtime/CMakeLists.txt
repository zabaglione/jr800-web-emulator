add_library(
    jr800_runtime
    STATIC
        src/application_loader.cpp
        src/jr800_machine_runner.cpp
)
add_library(jr800::runtime ALIAS jr800_runtime)

target_include_directories(
    jr800_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    jr800_runtime
    PUBLIC
        jr800::core
        jr800::formats
        jr800_project_options
)
