add_library(
    jr800_assembler
    STATIC
        src/assembler.cpp
)
add_library(jr800::assembler ALIAS jr800_assembler)

target_include_directories(
    jr800_assembler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    jr800_assembler
    PUBLIC
        jr800::formats
        jr800::isa
        jr800_project_options
)
