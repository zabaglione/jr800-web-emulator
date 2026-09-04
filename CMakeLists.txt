cmake_minimum_required(VERSION 3.25)

project(
    jr800
    VERSION 0.1.0
    DESCRIPTION "JR-800 emulator, debugger, and SDK"
    LANGUAGES C CXX
)

include(CTest)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)

option(JR800_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(
    JR800_REQUIRE_EMSCRIPTEN
    "Fail configuration unless the Emscripten toolchain is active"
    OFF
)

if(JR800_REQUIRE_EMSCRIPTEN AND NOT EMSCRIPTEN)
    message(FATAL_ERROR "This preset must be configured through emcmake")
endif()

add_library(jr800_project_options INTERFACE)
target_compile_features(jr800_project_options INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(
        jr800_project_options
        INTERFACE
            /W4
            $<$<BOOL:${JR800_WARNINGS_AS_ERRORS}>:/WX>
    )
else()
    target_compile_options(
        jr800_project_options
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            $<$<BOOL:${JR800_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

if(EMSCRIPTEN)
    target_compile_options(
        jr800_project_options
        INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
    )
    target_link_options(jr800_project_options INTERFACE -fexceptions)
endif()

add_subdirectory(core)
add_subdirectory(debugger)
add_subdirectory(formats)
add_subdirectory(isa)
add_subdirectory(disassembler)
add_subdirectory(assembler)
add_subdirectory(linker)
add_subdirectory(runtime)
add_subdirectory(wasm)

if(NOT EMSCRIPTEN)
    add_subdirectory(tools)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
