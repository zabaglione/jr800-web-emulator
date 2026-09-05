add_library(jr800_wasm_api STATIC src/api.cpp)
add_library(jr800::wasm_api ALIAS jr800_wasm_api)

target_include_directories(
    jr800_wasm_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    jr800_wasm_api
    PUBLIC
        jr800::debugger
        jr800::runtime
        jr800_project_options
)

if(EMSCRIPTEN)
    set(
        JR800_WASM_EXPORTED_FUNCTIONS
        _malloc
        _free
        _jr800_machine_abi_version
        _jr800_machine_create
        _jr800_machine_create_jr800
        _jr800_machine_destroy
        _jr800_machine_load_application
        _jr800_machine_load_debug_info
        _jr800_machine_load_logical_rom
        _jr800_machine_load_jr8rom
        _jr800_machine_load_program
        _jr800_machine_load_native_program_wav
        _jr800_machine_get_program_saves
        _jr800_machine_get_saved_program_info
        _jr800_machine_export_saved_program
        _jr800_machine_clear_program_saves
        _jr800_machine_reset
        _jr800_machine_get_state
        _jr800_machine_step
        _jr800_machine_step_over
        _jr800_machine_step_out
        _jr800_machine_run
        _jr800_machine_run_to
        _jr800_machine_advance_suspended_cycles
        _jr800_machine_advance_calendar_oscillator_ticks
        _jr800_machine_adjust_calendar_seconds
        _jr800_machine_set_calendar_datetime
        _jr800_machine_set_keyboard_bus_response
        _jr800_machine_set_keyboard_key_state
        _jr800_machine_get_keyboard_activity
        _jr800_machine_clear_keyboard_activity
        _jr800_machine_set_execution_breakpoint
        _jr800_machine_set_conditional_execution_breakpoint
        _jr800_machine_set_expression_watch
        _jr800_machine_clear_expression_watch
        _jr800_machine_evaluate_expression_watch
        _jr800_machine_set_symbol_watch
        _jr800_machine_clear_symbol_watch
        _jr800_machine_evaluate_symbol_watch
        _jr800_machine_set_memory_watchpoint
        _jr800_machine_history_count
        _jr800_machine_copy_history
        _jr800_machine_access_count
        _jr800_machine_copy_accesses
        _jr800_machine_clear_history
        _jr800_machine_read_memory
        _jr800_machine_copy_lcd_panel
        _jr800_machine_copy_lcd_indicators
        _jr800_machine_source_at
        _jr800_machine_source_address
        _jr800_machine_symbol_address
        _jr800_machine_source_path_size
        _jr800_machine_copy_source_path
        _jr800_machine_disassemble
        _jr800_machine_disassembly_text_size
        _jr800_machine_copy_disassembly_text
    )
    list(TRANSFORM JR800_WASM_EXPORTED_FUNCTIONS PREPEND "'")
    list(TRANSFORM JR800_WASM_EXPORTED_FUNCTIONS APPEND "'")
    list(JOIN JR800_WASM_EXPORTED_FUNCTIONS "," JR800_WASM_EXPORTED_FUNCTIONS_OPTION)

    add_executable(jr800_wasm src/module.cpp)
    target_link_libraries(jr800_wasm PRIVATE jr800::wasm_api)
    target_link_options(
        jr800_wasm
        PRIVATE
            "--no-entry"
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createJr800Module"
            "-sENVIRONMENT=web,worker,node"
            "-sFILESYSTEM=0"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sEXPORTED_FUNCTIONS=[${JR800_WASM_EXPORTED_FUNCTIONS_OPTION}]"
            "-sEXPORTED_RUNTIME_METHODS=['cwrap','HEAPU8','HEAPU32']"
    )
    set_target_properties(
        jr800_wasm
        PROPERTIES
            SUFFIX ".mjs"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web-module"
    )
    set(
        JR800_WEB_ASSET_NAMES
        styles.css
        app.mjs
        i18n.mjs
        locale-ja.json
        audio-output.mjs
        audio-signal.mjs
        basic-boot-profile.mjs
        lcd-indicator-view.mjs
        lcd-panel-view.mjs
        keyboard-input.mjs
        virtual-keyboard-input.mjs
        virtual-keyboard-legends.mjs
        rom-keyboard-glyphs.mjs
        program-save-view.mjs
        rom-storage.mjs
        run-pacer.mjs
        jr800-worker.mjs
        wasm-machine.mjs
    )
    set(JR800_WEB_ASSET_SOURCES ${JR800_WEB_ASSET_NAMES})
    list(TRANSFORM JR800_WEB_ASSET_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/web/")
    list(APPEND JR800_WEB_ASSET_SOURCES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    )
    set(JR800_WEB_ASSET_OUTPUTS ${JR800_WEB_ASSET_NAMES})
    list(APPEND JR800_WEB_ASSET_OUTPUTS LICENSE THIRD_PARTY_NOTICES.md)
    list(TRANSFORM JR800_WEB_ASSET_OUTPUTS PREPEND "${CMAKE_BINARY_DIR}/web-module/")
    add_custom_target(
        jr800_web_bundle ALL
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            ${JR800_WEB_ASSET_SOURCES}
            ${CMAKE_BINARY_DIR}/web-module
        COMMAND
            ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/build_web_bundle.py
            --index ${PROJECT_SOURCE_DIR}/web/index.html
            --output ${CMAKE_BINARY_DIR}/web
            ${JR800_WEB_ASSET_OUTPUTS}
            $<TARGET_FILE:jr800_wasm>
            $<TARGET_FILE_DIR:jr800_wasm>/jr800_wasm.wasm
        DEPENDS
            jr800_wasm
            ${PROJECT_SOURCE_DIR}/tools/build_web_bundle.py
            ${PROJECT_SOURCE_DIR}/web/index.html
            ${JR800_WEB_ASSET_SOURCES}
        BYPRODUCTS ${JR800_WEB_ASSET_OUTPUTS}
        VERBATIM
    )
endif()
