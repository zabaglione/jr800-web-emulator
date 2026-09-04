set(JR800_ISA_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/hd6301-minimal.json)
set(JR800_ISA_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_isa.py)
set(JR800_ISA_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
set(
    JR800_ISA_GENERATED_HEADER
    ${JR800_ISA_GENERATED_INCLUDE_DIR}/jr800/isa/instruction_metadata.hpp
)
set(
    JR800_ISA_GENERATED_SOURCE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/src/instruction_metadata.cpp
)

add_custom_command(
    OUTPUT
        ${JR800_ISA_GENERATED_HEADER}
        ${JR800_ISA_GENERATED_SOURCE}
    COMMAND
        ${Python3_EXECUTABLE}
        ${JR800_ISA_GENERATOR}
        --input ${JR800_ISA_INPUT}
        --output-header ${JR800_ISA_GENERATED_HEADER}
        --output-source ${JR800_ISA_GENERATED_SOURCE}
    DEPENDS
        ${JR800_ISA_INPUT}
        ${JR800_ISA_GENERATOR}
    COMMENT "Generating validated ISA metadata"
    VERBATIM
)

add_library(
    jr800_isa
    STATIC
        ${JR800_ISA_GENERATED_HEADER}
        ${JR800_ISA_GENERATED_SOURCE}
)
add_library(jr800::isa ALIAS jr800_isa)

target_include_directories(
    jr800_isa
    PUBLIC
        ${JR800_ISA_GENERATED_INCLUDE_DIR}
)
target_link_libraries(jr800_isa PUBLIC jr800_project_options)
