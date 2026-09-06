if(EXISTS "${PROJECT_SOURCE_DIR}/docs/wasm/c-abi-v42.md")
    add_test(
        NAME abi_version_contract
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/abi_version_contract_test.py
            --header ${PROJECT_SOURCE_DIR}/wasm/include/jr800/wasm/api.h
            --adapter ${PROJECT_SOURCE_DIR}/web/wasm-machine.mjs
            --document ${PROJECT_SOURCE_DIR}/docs/wasm/c-abi-v42.md
    )
    set_tests_properties(
        abi_version_contract
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )
endif()

if(EMSCRIPTEN)
    find_program(NODE_EXECUTABLE node REQUIRED)
    add_test(NAME relic_dive_host COMMAND ${NODE_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/relic_dive_host_test.mjs
        $<TARGET_FILE_DIR:jr800_wasm>/relic-dive.mjs)
    add_test(
        NAME program_save_view
        COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/program_save_view_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/program-save-view.mjs
    )
    add_test(
        NAME web_bundle
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/web_bundle_test.py
            ${PROJECT_SOURCE_DIR}/tools/build_web_bundle.py
    )
    add_test(
        NAME wasm_worker_vertical_slice
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/wasm_worker_vertical_slice.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/jr800-worker.mjs
            $<TARGET_FILE:jr800_wasm>
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8a.hex
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8d.hex
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.expected.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slp.j8a.hex
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/jr800-nop.expected.json
    )
    add_test(
        NAME web_ui_contract
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/web_ui_contract_test.mjs
            ${CMAKE_BINARY_DIR}/web/index.html
            $<TARGET_FILE_DIR:jr800_wasm>/app.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/styles.css
    )
    add_test(
        NAME i18n
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/i18n_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/i18n.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/locale-ja.json
    )
    add_test(
        NAME audio_output
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/audio_output_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/audio-output.mjs
    )
    add_test(
        NAME run_pacer
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/run_pacer_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/run-pacer.mjs
    )
    add_test(
        NAME basic_boot_profile
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/basic_boot_profile_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/basic-boot-profile.mjs
    )
    add_test(
        NAME lcd_panel_view
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/lcd_panel_view_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/lcd-panel-view.mjs
    )
    add_test(
        NAME lcd_indicator_view
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/lcd_indicator_view_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/lcd-indicator-view.mjs
    )
    add_test(
        NAME keyboard_input
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/keyboard_input_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/keyboard-input.mjs
    )
    add_test(
        NAME virtual_keyboard_input
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/virtual_keyboard_input_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/virtual-keyboard-input.mjs
    )
    add_test(
        NAME virtual_keyboard_legends
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/virtual_keyboard_legends_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/virtual-keyboard-legends.mjs
    )
    add_test(
        NAME rom_keyboard_glyphs
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/rom_keyboard_glyphs_test.mjs
            $<TARGET_FILE_DIR:jr800_wasm>/rom-keyboard-glyphs.mjs
    )
else()
    find_program(MAKE_EXECUTABLE make REQUIRED)

    add_executable(native_smoke native_smoke.cpp)
    target_link_libraries(
        native_smoke
        PRIVATE
            jr800::wasm_api
    )
    add_test(NAME native_smoke COMMAND native_smoke)

    add_executable(core_execution_test core_execution_test.cpp)
    target_link_libraries(core_execution_test PRIVATE jr800::debugger)
    add_test(NAME core_execution_test COMMAND core_execution_test)

    add_executable(debugger_expression_test debugger_expression_test.cpp)
    target_link_libraries(debugger_expression_test PRIVATE jr800::debugger)
    add_test(NAME debugger_expression_test COMMAND debugger_expression_test)

    add_executable(cpu_extended_instruction_test cpu_extended_instruction_test.cpp)
    target_link_libraries(cpu_extended_instruction_test PRIVATE jr800::core)
    add_test(NAME cpu_extended_instruction_test COMMAND cpu_extended_instruction_test)

    add_executable(jr800_memory_test jr800_memory_test.cpp)
    target_link_libraries(jr800_memory_test PRIVATE jr800::core)
    add_test(NAME jr800_memory_test COMMAND jr800_memory_test)

    add_executable(
        jr800_experimental_memory_test
        jr800_experimental_memory_test.cpp
    )
    target_link_libraries(jr800_experimental_memory_test PRIVATE jr800::core)
    add_test(
        NAME jr800_experimental_memory_test
        COMMAND jr800_experimental_memory_test
    )

    add_executable(jr800_keyboard_test jr800_keyboard_test.cpp)
    target_link_libraries(jr800_keyboard_test PRIVATE jr800::core)
    add_test(NAME jr800_keyboard_test COMMAND jr800_keyboard_test)

    add_executable(jr800_lcd_test jr800_lcd_test.cpp)
    target_link_libraries(jr800_lcd_test PRIVATE jr800::core)
    add_test(NAME jr800_lcd_test COMMAND jr800_lcd_test)

    add_executable(jr800_lcd_bus_test jr800_lcd_bus_test.cpp)
    target_link_libraries(jr800_lcd_bus_test PRIVATE jr800::core)
    add_test(NAME jr800_lcd_bus_test COMMAND jr800_lcd_bus_test)

    add_executable(hd44102_test hd44102_test.cpp)
    target_link_libraries(hd44102_test PRIVATE jr800::core)
    add_test(NAME hd44102_test COMMAND hd44102_test)

    add_executable(
        rp5c01_register_file_test
        rp5c01_register_file_test.cpp
    )
    target_link_libraries(rp5c01_register_file_test PRIVATE jr800::core)
    add_test(
        NAME rp5c01_register_file_test
        COMMAND rp5c01_register_file_test
    )

    add_executable(jr800_calendar_bus_test jr800_calendar_bus_test.cpp)
    target_link_libraries(jr800_calendar_bus_test PRIVATE jr800::core)
    add_test(NAME jr800_calendar_bus_test COMMAND jr800_calendar_bus_test)

    add_executable(hd6301v1_timer_test hd6301v1_timer_test.cpp)
    target_link_libraries(hd6301v1_timer_test PRIVATE jr800::core)
    add_test(NAME hd6301v1_timer_test COMMAND hd6301v1_timer_test)

    add_executable(hd6301v1_sci_test hd6301v1_sci_test.cpp)
    target_link_libraries(hd6301v1_sci_test PRIVATE jr800::core)
    add_test(NAME hd6301v1_sci_test COMMAND hd6301v1_sci_test)

    add_executable(hd6301v1_ports_test hd6301v1_ports_test.cpp)
    target_link_libraries(hd6301v1_ports_test PRIVATE jr800::core)
    add_test(NAME hd6301v1_ports_test COMMAND hd6301v1_ports_test)

    add_executable(
        hd6301v1_ram_control_test
        hd6301v1_ram_control_test.cpp
    )
    target_link_libraries(hd6301v1_ram_control_test PRIVATE jr800::core)
    add_test(
        NAME hd6301v1_ram_control_test
        COMMAND hd6301v1_ram_control_test
    )

    add_executable(jr800_bus_test jr800_bus_test.cpp)
    target_link_libraries(jr800_bus_test PRIVATE jr800::core)
    add_test(NAME jr800_bus_test COMMAND jr800_bus_test)

    add_executable(machine_state_test machine_state_test.cpp)
    target_link_libraries(machine_state_test PRIVATE jr800::core)
    add_test(NAME machine_state_test COMMAND machine_state_test)

    add_executable(jr800_machine_test jr800_machine_test.cpp)
    target_link_libraries(jr800_machine_test PRIVATE jr800::debugger)
    add_test(NAME jr800_machine_test COMMAND jr800_machine_test)

    add_executable(jr800_machine_runner_test jr800_machine_runner_test.cpp)
    target_link_libraries(jr800_machine_runner_test PRIVATE jr800::runtime)
    add_test(NAME jr800_machine_runner_test COMMAND jr800_machine_runner_test)

    add_executable(c_api_smoke c_api_smoke.c)
    set_target_properties(c_api_smoke PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(c_api_smoke PRIVATE jr800::wasm_api)
    add_test(NAME c_api_smoke COMMAND c_api_smoke)

    add_executable(basic_program_test basic_program_test.cpp)
    target_link_libraries(basic_program_test PRIVATE jr800::runtime jr800::debugger)
    add_test(NAME basic_program_test COMMAND basic_program_test)
    add_executable(program_save_test program_save_test.cpp)
    target_link_libraries(program_save_test PRIVATE jr800::wasm_api)
    add_test(NAME program_save_test COMMAND program_save_test)

    add_executable(calendar_startup_test calendar_startup_test.cpp)
    target_link_libraries(calendar_startup_test PRIVATE jr800::wasm_api)
    add_test(NAME calendar_startup_test COMMAND calendar_startup_test)

    add_executable(c_api_jr800_test c_api_jr800_test.cpp)
    target_link_libraries(c_api_jr800_test PRIVATE jr800::wasm_api)
    add_test(NAME c_api_jr800_test COMMAND c_api_jr800_test)

    add_executable(
        c_api_vertical_slice_snapshot
        c_api_vertical_slice_snapshot.cpp
    )
    target_link_libraries(
        c_api_vertical_slice_snapshot
        PRIVATE
            jr800::wasm_api
    )
    add_test(
        NAME c_api_vertical_slice
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/snapshot_parity_test.py
            --snapshot $<TARGET_FILE:c_api_vertical_slice_snapshot>
            --expected ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.expected.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8a.hex
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8d.hex
    )
    set_tests_properties(
        c_api_vertical_slice
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_executable(
        c_api_jr800_parity_snapshot
        c_api_jr800_parity_snapshot.cpp
    )
    target_link_libraries(
        c_api_jr800_parity_snapshot
        PRIVATE
            jr800::wasm_api
    )
    add_test(
        NAME c_api_jr800_parity
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/snapshot_parity_test.py
            --snapshot $<TARGET_FILE:c_api_jr800_parity_snapshot>
            --expected ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/jr800-nop.expected.json
    )
    set_tests_properties(
        c_api_jr800_parity
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_executable(isa_metadata_test isa_metadata_test.cpp)
    target_link_libraries(isa_metadata_test PRIVATE jr800::isa)
    add_test(NAME isa_metadata_test COMMAND isa_metadata_test)

    add_executable(disassembler_test disassembler_test.cpp)
    target_link_libraries(disassembler_test PRIVATE jr800::disassembler)
    add_test(NAME disassembler_test COMMAND disassembler_test)

    add_executable(jro_format_test jro_format_test.cpp)
    target_link_libraries(jro_format_test PRIVATE jr800::formats)
    add_test(NAME jro_format_test COMMAND jro_format_test)

    add_executable(linked_format_test linked_format_test.cpp)
    target_link_libraries(linked_format_test PRIVATE jr800::formats)
    add_test(NAME linked_format_test COMMAND linked_format_test)

    add_executable(jr8rom_format_test jr8rom_format_test.cpp)
    target_link_libraries(jr8rom_format_test PRIVATE jr800::formats)
    add_test(NAME jr8rom_format_test COMMAND jr8rom_format_test)

    add_executable(rom_dump_codec_test rom_dump_codec_test.cpp)
    target_link_libraries(rom_dump_codec_test PRIVATE jr800::formats)
    add_test(NAME rom_dump_codec_test COMMAND rom_dump_codec_test)

    add_executable(native_msave_test native_msave_test.cpp)
    target_link_libraries(native_msave_test PRIVATE jr800::formats)
    add_test(NAME native_msave_test COMMAND native_msave_test)

    add_executable(assembler_test assembler_test.cpp)
    target_link_libraries(assembler_test PRIVATE jr800::assembler)
    add_test(NAME assembler_test COMMAND assembler_test)

    add_executable(linker_test linker_test.cpp)
    target_link_libraries(linker_test PRIVATE jr800::assembler jr800::linker)
    add_test(NAME linker_test COMMAND linker_test)

    add_executable(execution_vertical_slice_test execution_vertical_slice_test.cpp)
    target_link_libraries(
        execution_vertical_slice_test
        PRIVATE
            jr800::assembler
            jr800::debugger
            jr800::linker
            jr800::runtime
    )
    add_test(NAME execution_vertical_slice_test COMMAND execution_vertical_slice_test)

    add_test(
        NAME assembler_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/assembler_cli_test.py
            --assembler $<TARGET_FILE:jr8as>
    )
    set_tests_properties(
        assembler_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME linker_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/linker_cli_test.py
            --assembler $<TARGET_FILE:jr8as>
            --linker $<TARGET_FILE:jr8ld>
    )
    set_tests_properties(
        linker_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME jr8nm_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/jr8nm_cli_test.py
            --assembler $<TARGET_FILE:jr8as>
            --tool $<TARGET_FILE:jr8nm>
            --debug-fixture ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8d.hex
    )
    set_tests_properties(
        jr8nm_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME jr8objdump_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/jr8objdump_cli_test.py
            --assembler $<TARGET_FILE:jr8as>
            --linker $<TARGET_FILE:jr8ld>
            --tool $<TARGET_FILE:jr8objdump>
    )
    set_tests_properties(
        jr8objdump_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME jr8objcopy_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/jr8objcopy_cli_test.py
            --assembler $<TARGET_FILE:jr8as>
            --linker $<TARGET_FILE:jr8ld>
            --tool $<TARGET_FILE:jr8objcopy>
    )
    set_tests_properties(
        jr8objcopy_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME jr8rom_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/jr8rom_cli_test.py
            --tool $<TARGET_FILE:jr8rom>
    )
    set_tests_properties(
        jr8rom_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME jr8wav_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/jr8wav_cli_test.py
            --tool $<TARGET_FILE:jr8wav>
            --jr8rom-tool $<TARGET_FILE:jr8rom>
            --runner $<TARGET_FILE:jr8run>
    )
    set_tests_properties(
        jr8wav_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME jr800_runner_cli_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/jr800_runner_cli_test.py
            --runner $<TARGET_FILE:jr8run>
    )
    set_tests_properties(
        jr800_runner_cli_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME sample_make_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/sample_make_test.py
            --make ${MAKE_EXECUTABLE}
            --source-dir ${PROJECT_SOURCE_DIR}/sdk/examples/write-watch
            --assembler $<TARGET_FILE:jr8as>
            --linker $<TARGET_FILE:jr8ld>
            --runner $<TARGET_FILE:jr8run>
            --application-fixture ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8a.hex
            --debug-fixture ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/write-watch.j8d.hex
    )
    set_tests_properties(
        sample_make_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    add_test(
        NAME isa_generator_test
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/isa_generator_test.py
            --generator ${PROJECT_SOURCE_DIR}/isa/tools/generate_isa.py
            --input ${PROJECT_SOURCE_DIR}/isa/hd6301-minimal.json
    )
    set_tests_properties(
        isa_generator_test
        PROPERTIES
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    )

    if(EXISTS "${PROJECT_SOURCE_DIR}/tools/validate_evidence_package.py")
        add_test(
            NAME evidence_package_validator_test
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/evidence_package_validator_test.py
                --validator ${PROJECT_SOURCE_DIR}/tools/validate_evidence_package.py
        )
        set_tests_properties(
            evidence_package_validator_test
            PROPERTIES
                ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
        )
    endif()

    foreach(
        tool_name
        IN ITEMS jr8as jr8ld jr8nm jr8objcopy jr8objdump jr8rom jr8run jr8wav
    )
        add_test(NAME ${tool_name}_version COMMAND ${tool_name} --version)
        set_tests_properties(
            ${tool_name}_version
            PROPERTIES
                PASS_REGULAR_EXPRESSION "${tool_name} ${PROJECT_VERSION}"
        )
    endforeach()
endif()
