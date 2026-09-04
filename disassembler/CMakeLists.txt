add_library(
    jr800_disassembler
    src/disassembler.cpp
)
add_library(jr800::disassembler ALIAS jr800_disassembler)

target_include_directories(
    jr800_disassembler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    jr800_disassembler
    PUBLIC
        jr800::isa
        jr800_project_options
)
