add_library(
    jr800_formats
    STATIC
        src/jro.cpp
        src/jr8app.cpp
        src/basic_program.cpp
        src/jr8dbg.cpp
        src/jr8rom.cpp
        src/rom_dump.cpp
        src/fsk_wav.cpp
        src/native_msave.cpp
        src/native_program_wav.cpp
        src/wav_pcm.cpp
        src/linked_error.cpp
        src/sha256.cpp
)
add_library(jr800::formats ALIAS jr800_formats)

target_include_directories(
    jr800_formats
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(jr800_formats PUBLIC jr800_project_options)
