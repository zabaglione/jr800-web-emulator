add_library(
    jr800_linker
    STATIC
        src/linker.cpp
        src/script.cpp
)
add_library(jr800::linker ALIAS jr800_linker)

target_include_directories(
    jr800_linker
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    jr800_linker
    PUBLIC
        jr800::formats
        jr800_project_options
)
