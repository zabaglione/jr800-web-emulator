add_library(
    jr800_debugger
    src/debugger.cpp
    src/expression.cpp
)
add_library(jr800::debugger ALIAS jr800_debugger)

target_include_directories(
    jr800_debugger
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    jr800_debugger
    PUBLIC
        jr800::core
        jr800::formats
        jr800_project_options
    PRIVATE
        jr800::disassembler
)
