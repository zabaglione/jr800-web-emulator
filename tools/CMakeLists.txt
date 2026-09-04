function(jr800_configure_tool target_name)
    target_link_libraries(${target_name} PRIVATE jr800_project_options)
    target_compile_definitions(
        ${target_name}
        PRIVATE
            JR800_PROJECT_VERSION="${PROJECT_VERSION}"
            JR800_TOOL_NAME="${target_name}"
    )
endfunction()

function(jr800_add_placeholder_tool target_name)
    add_executable(${target_name} version_main.cpp)
    jr800_configure_tool(${target_name})
endfunction()

add_executable(jr8as jr8as_main.cpp)
target_link_libraries(jr8as PRIVATE jr800::assembler)
jr800_configure_tool(jr8as)

add_executable(jr8ld jr8ld_main.cpp)
target_link_libraries(jr8ld PRIVATE jr800::linker)
jr800_configure_tool(jr8ld)

add_executable(jr8nm jr8nm_main.cpp)
target_link_libraries(jr8nm PRIVATE jr800::formats)
jr800_configure_tool(jr8nm)

add_executable(jr8objdump jr8objdump_main.cpp)
target_link_libraries(
    jr8objdump
    PRIVATE
        jr800::disassembler
        jr800::formats
)
jr800_configure_tool(jr8objdump)

add_executable(jr8objcopy jr8objcopy_main.cpp)
target_link_libraries(jr8objcopy PRIVATE jr800::formats)
jr800_configure_tool(jr8objcopy)

add_executable(jr8rom jr8rom_main.cpp)
target_link_libraries(jr8rom PRIVATE jr800::formats)
jr800_configure_tool(jr8rom)

add_executable(
    jr8run
    jr8run_main.cpp
    jr8run_jr800.cpp
)
target_link_libraries(jr8run PRIVATE jr800::debugger jr800::runtime)
jr800_configure_tool(jr8run)

add_executable(jr8wav jr8wav_main.cpp)
target_link_libraries(jr8wav PRIVATE jr800::formats)
jr800_configure_tool(jr8wav)
