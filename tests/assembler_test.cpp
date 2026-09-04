// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/assembler/assembler.hpp"
#include "jr800/formats/jro.hpp"

namespace {

using jr800::assembler::Diagnostic;
using jr800::assembler::Options;
using jr800::assembler::Result;
using jr800::assembler::Source;
using jr800::formats::jro::ObjectFile;
using jr800::formats::jro::Section;
using jr800::formats::Sha256Digest;
using jr800::formats::jro::Symbol;

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool expect_success(const Result& result, std::string_view message) {
    if (result.succeeded()) {
        return true;
    }
    std::cerr << message << '\n';
    for (const auto& diagnostic : result.diagnostics) {
        std::cerr << diagnostic.path << ':' << diagnostic.line << ':'
                  << diagnostic.column << ' ' << diagnostic.code << ' '
                  << diagnostic.message << '\n';
    }
    return false;
}

const Section* find_section(const ObjectFile& object, std::string_view name) {
    const auto found = std::find_if(
        object.sections.begin(),
        object.sections.end(),
        [&](const Section& section) { return section.name == name; }
    );
    return found == object.sections.end() ? nullptr : &*found;
}

const Symbol* find_symbol(const ObjectFile& object, std::string_view name) {
    const auto found = std::find_if(
        object.symbols.begin(),
        object.symbols.end(),
        [&](const Symbol& symbol) { return symbol.name == name; }
    );
    return found == object.symbols.end() ? nullptr : &*found;
}

const Diagnostic* find_diagnostic(const Result& result, std::string_view code) {
    const auto found = std::find_if(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [&](const Diagnostic& diagnostic) { return diagnostic.code == code; }
    );
    return found == result.diagnostics.end() ? nullptr : &*found;
}

bool test_multi_source_objects() {
    using namespace jr800::formats::jro;

    constexpr std::string_view main_source =
        ".section .text, code\n"
        ".global entry\n"
        ".extern helper\n"
        ".local done\n"
        ".global buffer\n"
        "entry:\n"
        "    LDAA #$20 + 10\n"
        "    STAA buffer\n"
        "    BSR helper\n"
        "    BRA done\n"
        "done:\n"
        "    NOP\n"
        ".section .bss, bss\n"
        "buffer:\n"
        "    .space 1\n";
    constexpr std::string_view library_source =
        ".section .text, code\n"
        ".global helper\n"
        ".extern buffer\n"
        "helper:\n"
        "    AIM #$F0, buffer\n"
        "    RTS\n";
    const Options options{"hd6301v1", "test-version"};
    const auto main_result = jr800::assembler::assemble(
        Source{"src/main.s", std::string{main_source}},
        options
    );
    const auto library_result = jr800::assembler::assemble(
        Source{"src/lib.s", std::string{library_source}},
        options
    );

    bool passed = expect_success(main_result, "Main source did not assemble");
    passed &= expect_success(library_result, "Library source did not assemble");
    if (!main_result.succeeded() || !library_result.succeeded()) {
        return false;
    }

    const auto& main_object = main_result.output->object;
    const auto& library_object = library_result.output->object;
    passed &= expect(main_object.target_profile == "hd6301v1", "Target profile mismatch");
    passed &= expect(main_object.build.producer == "jr8as", "Producer identity mismatch");
    passed &= expect(main_object.source_files.size() == 1U, "Main source identity missing");
    passed &= expect(
        main_object.source_files.front().path == "src/main.s",
        "Main logical path mismatch"
    );

    const auto* text = find_section(main_object, ".text");
    const auto* bss = find_section(main_object, ".bss");
    passed &= expect(text != nullptr, "Main text section missing");
    passed &= expect(bss != nullptr, "Main bss section missing");
    if (text != nullptr) {
        passed &= expect(
            text->data == std::vector<std::uint8_t>{
                0x86, 0x2A, 0xB7, 0x00, 0x00,
                0x8D, 0x00, 0x20, 0x00, 0x01,
            },
            "Main instruction encoding mismatch"
        );
    }
    if (bss != nullptr) {
        passed &= expect(bss->type == SectionType::no_bits, "Bss type mismatch");
        passed &= expect(bss->logical_size == 1U, "Bss logical size mismatch");
        passed &= expect(bss->data.empty(), "Bss must not contain file bytes");
    }

    const auto* entry = find_symbol(main_object, "entry");
    const auto* helper = find_symbol(main_object, "helper");
    const auto* done = find_symbol(main_object, "done");
    const auto* buffer = find_symbol(main_object, "buffer");
    passed &= expect(entry != nullptr, "Entry symbol missing");
    passed &= expect(helper != nullptr, "Helper symbol missing");
    passed &= expect(done != nullptr, "Done symbol missing");
    passed &= expect(buffer != nullptr, "Buffer symbol missing");
    if (entry != nullptr) {
        passed &= expect(entry->binding == SymbolBinding::global, "Entry binding mismatch");
        passed &= expect(entry->value == 0U, "Entry value mismatch");
    }
    if (helper != nullptr) {
        passed &= expect(
            helper->definition == SymbolDefinition::undefined,
            "Helper must remain undefined in main object"
        );
    }
    if (done != nullptr) {
        passed &= expect(done->binding == SymbolBinding::local, "Done binding mismatch");
        passed &= expect(done->value == 9U, "Done value mismatch");
    }
    if (buffer != nullptr) {
        passed &= expect(buffer->binding == SymbolBinding::global, "Buffer binding mismatch");
        passed &= expect(buffer->section_index == 1U, "Buffer section mismatch");
    }

    passed &= expect(main_object.relocations.size() == 3U, "Main relocation count mismatch");
    if (main_object.relocations.size() == 3U) {
        passed &= expect(
            main_object.relocations[0].type == RelocationType::abs16_be
                && main_object.relocations[0].offset == 3U,
            "STAA relocation mismatch"
        );
        passed &= expect(
            main_object.relocations[1].type == RelocationType::rel8
                && main_object.relocations[1].offset == 6U,
            "BSR relocation mismatch"
        );
        passed &= expect(
            main_object.relocations[2].type == RelocationType::rel8
                && main_object.relocations[2].offset == 8U,
            "BRA relocation mismatch"
        );
    }
    passed &= expect(main_object.source_lines.size() == 6U, "Source mapping count mismatch");
    passed &= expect(
        main_result.output->listing.find("Target: hd6301v1") != std::string::npos
            && main_result.output->listing.find("86 2A") != std::string::npos,
        "Main listing content mismatch"
    );

    const auto* library_text = find_section(library_object, ".text");
    passed &= expect(library_text != nullptr, "Library text section missing");
    if (library_text != nullptr) {
        passed &= expect(
            library_text->data == std::vector<std::uint8_t>{0x71, 0xF0, 0x00, 0x39},
            "Library instruction encoding mismatch"
        );
    }
    passed &= expect(
        library_object.relocations.size() == 1U
            && library_object.relocations.front().type == RelocationType::direct8
            && library_object.relocations.front().offset == 2U,
        "AIM relocation mismatch"
    );

    const auto repeated = jr800::assembler::assemble(
        Source{"src/main.s", std::string{main_source}},
        options
    );
    passed &= expect_success(repeated, "Repeated assembly failed");
    if (repeated.succeeded()) {
        const auto first_bytes = write(main_object);
        const auto second_bytes = write(repeated.output->object);
        passed &= expect(first_bytes == second_bytes, "Assembly output is not deterministic");
        passed &= expect(read(first_bytes) == main_object, "Assembler JRO round trip failed");
    }
    return passed;
}

bool test_expressions_and_digest() {
    constexpr std::string_view expression_source =
        ".section .data, data\n"
        ".byte 1 + 2 * 3, %1010, $2A, -1\n"
        ".word $1234, 1 << 8 | 2\n";
    const auto result = jr800::assembler::assemble(
        Source{"src/data.s", std::string{expression_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(result, "Expression source did not assemble");
    if (result.succeeded()) {
        const auto* data = find_section(result.output->object, ".data");
        passed &= expect(data != nullptr, "Data section missing");
        if (data != nullptr) {
            passed &= expect(
                data->data == std::vector<std::uint8_t>{
                    0x07, 0x0A, 0x2A, 0xFF, 0x12, 0x34, 0x01, 0x02,
                },
                "Expression encoding mismatch"
            );
        }
    }

    const auto empty = jr800::assembler::assemble(
        Source{"src/empty.s", ""},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(empty, "Empty source did not assemble");
    if (empty.succeeded()) {
        constexpr Sha256Digest expected{
            0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14,
            0x9A, 0xFB, 0xF4, 0xC8, 0x99, 0x6F, 0xB9, 0x24,
            0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B, 0x93, 0x4C,
            0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55,
        };
        passed &= expect(
            empty.output->object.source_files.front().content_sha256 == expected,
            "Empty source SHA-256 mismatch"
        );
    }
    return passed;
}

bool test_extended_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "SEI\n"
        "CLRA\n"
        "JMP $8123\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/jump-absolute.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Absolute JMP did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0x0FU,
                        0x4FU,
                        0x7EU,
                        0x81U,
                        0x23U,
                    }
                && absolute.output->object.relocations.empty(),
            "Absolute JMP encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern target\n"
        "SEI\n"
        "CLRA\n"
        "JMP target\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/jump-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated JMP did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0x0FU,
                        0x4FU,
                        0x7EU,
                        0x00U,
                        0x00U,
                    }
                && relocated.output->object.relocations.size() == 1U
                && relocated.output->object.relocations.front().offset == 3U
                && relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated JMP encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-jump.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 JMP form was accepted"
    );
    return passed;
}

bool test_indexed_jump_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "JMP $20,X\n"
        "JMP 0,x\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/jump-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed JMP did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x6EU, 0x20U,
                    0x6EU, 0x00U,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed JMP encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/jump-indexed-range.s",
            ".section .text, code\nJMP 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed JMP accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/jump-indexed-negative.s",
            ".section .text, code\nJMP -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed JMP accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/jump-indexed-register.s",
            ".section .text, code\nJMP $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed JMP accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "JMP DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/jump-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed JMP did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x6EU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed JMP encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-jump-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed JMP form was accepted"
    );
    return passed;
}

bool test_jsr_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "JSR $20\n"
        "JSR $3456\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/jsr-absolute.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Absolute JSR did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0x9DU, 0x20U,
                        0xBDU, 0x34U, 0x56U,
                    }
                && absolute.output->object.relocations.empty(),
            "Direct/extended JSR address selection differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern target\n"
        "JSR target\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/jsr-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated extended JSR did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xBDU, 0x00U, 0x00U}
                && relocated.output->object.relocations.size() == 1U
                && relocated.output->object.relocations.front().offset == 1U
                && relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated extended JSR encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "JSR $20,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{
            "src/jsr-indexed.s",
            std::string{indexed_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed JSR did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xADU, 0x20U}
                && indexed.output->object.relocations.empty(),
            "Indexed JSR encoding differs"
        );
    }

    constexpr std::string_view indexed_relocated_source =
        ".extern displacement\n"
        ".section .text, code\n"
        "JSR displacement,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/jsr-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed JSR did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(indexed_relocated.output->object, ".text");
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xADU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed JSR encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/jsr-indexed-range.s",
            ".section .text, code\nJSR 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed JSR accepted a displacement above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-jsr.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed JSR form was accepted"
    );
    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-jsr-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed JSR form was accepted"
    );

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/jsr-range.s",
            ".section .text, code\nJSR $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended JSR accepted a target above 16 bits"
    );
    return passed;
}

bool test_tap_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TAP\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/tap.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "TAP did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x06U}
                && assembled.output->object.relocations.empty(),
            "TAP encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/tap-operand.s",
            ".section .text, code\nTAP $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "TAP accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tap.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 TAP form was accepted"
    );
    return passed;
}

bool test_tpa_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TPA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/tpa.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "TPA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x07U}
                && assembled.output->object.relocations.empty(),
            "TPA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/tpa-operand.s",
            ".section .text, code\nTPA $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "TPA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tpa.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 TPA form was accepted"
    );
    return passed;
}

bool test_clv_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "CLV\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/clv.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "CLV did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x0AU}
                && assembled.output->object.relocations.empty(),
            "CLV encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/clv-operand.s",
            ".section .text, code\nCLV $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "CLV accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-clv.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 CLV form was accepted"
    );
    return passed;
}

bool test_sev_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "SEV\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sev.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "SEV did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x0BU}
                && assembled.output->object.relocations.empty(),
            "SEV encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/sev-operand.s",
            ".section .text, code\nSEV $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "SEV accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sev.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 SEV form was accepted"
    );
    return passed;
}

bool test_clc_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "CLC\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/clc.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "CLC did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x0CU}
                && assembled.output->object.relocations.empty(),
            "CLC encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/clc-operand.s",
            ".section .text, code\nCLC $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "CLC accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-clc.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 CLC form was accepted"
    );
    return passed;
}

bool test_sec_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "SEC\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sec.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "SEC did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x0DU}
                && assembled.output->object.relocations.empty(),
            "SEC encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/sec-operand.s",
            ".section .text, code\nSEC $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "SEC accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sec.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 SEC form was accepted"
    );
    return passed;
}

bool test_cli_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "CLI\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cli.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "CLI did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x0EU}
                && assembled.output->object.relocations.empty(),
            "CLI encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/cli-operand.s",
            ".section .text, code\nCLI $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "CLI accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cli.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 CLI form was accepted"
    );
    return passed;
}

bool test_tsx_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TSX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/tsx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "TSX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x30U}
                && assembled.output->object.relocations.empty(),
            "TSX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/tsx-operand.s",
            ".section .text, code\nTSX $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "TSX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tsx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 TSX form was accepted"
    );
    return passed;
}

bool test_ins_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "INS\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ins.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "INS did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x31U}
                && assembled.output->object.relocations.empty(),
            "INS encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/ins-operand.s",
            ".section .text, code\nINS $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "INS accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ins.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 INS form was accepted"
    );
    return passed;
}

bool test_pula_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "PULA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/pula.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "PULA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x32U}
                && assembled.output->object.relocations.empty(),
            "PULA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/pula-operand.s",
            ".section .text, code\nPULA $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "PULA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-pula.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 PULA form was accepted"
    );
    return passed;
}

bool test_pulb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "PULB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/pulb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "PULB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x33U}
                && assembled.output->object.relocations.empty(),
            "PULB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/pulb-operand.s",
            ".section .text, code\nPULB $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "PULB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-pulb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 PULB form was accepted"
    );
    return passed;
}

bool test_des_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "DES\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/des.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "DES did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x34U}
                && assembled.output->object.relocations.empty(),
            "DES encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/des-operand.s",
            ".section .text, code\nDES $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "DES accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-des.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 DES form was accepted"
    );
    return passed;
}

bool test_txs_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TXS\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/txs.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "TXS did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x35U}
                && assembled.output->object.relocations.empty(),
            "TXS encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/txs-operand.s",
            ".section .text, code\nTXS $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "TXS accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-txs.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 TXS form was accepted"
    );
    return passed;
}

bool test_psha_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "PSHA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/psha.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "PSHA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x36U}
                && assembled.output->object.relocations.empty(),
            "PSHA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/psha-operand.s",
            ".section .text, code\nPSHA $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "PSHA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-psha.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 PSHA form was accepted"
    );
    return passed;
}

bool test_pshb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "PSHB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/pshb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "PSHB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x37U}
                && assembled.output->object.relocations.empty(),
            "PSHB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/pshb-operand.s",
            ".section .text, code\nPSHB $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "PSHB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-pshb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 PSHB form was accepted"
    );
    return passed;
}

bool test_pshx_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "PSHX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/pshx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "PSHX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x3CU}
                && assembled.output->object.relocations.empty(),
            "PSHX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/pshx-operand.s",
            ".section .text, code\nPSHX $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "PSHX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-pshx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 PSHX form was accepted"
    );
    return passed;
}

bool test_pulx_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "PULX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/pulx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "PULX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x38U}
                && assembled.output->object.relocations.empty(),
            "PULX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/pulx-operand.s",
            ".section .text, code\nPULX $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "PULX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-pulx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 PULX form was accepted"
    );
    return passed;
}

bool test_conditional_branch_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view brn_source =
        ".section .text, code\n"
        ".extern target\n"
        "BRN target\n";
    const auto assembled_brn = jr800::assembler::assemble(
        Source{"src/brn.s", std::string{brn_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled_brn, "BRN did not assemble");
    if (assembled_brn.succeeded()) {
        const auto* text = find_section(assembled_brn.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x21U, 0x00U}
                && assembled_brn.output->object.relocations.size() == 1U
                && assembled_brn.output->object.relocations.front().offset == 1U
                && assembled_brn.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BRN encoding or relocation differs"
        );
    }

    const auto unstaged_brn = jr800::assembler::assemble(
        Source{"src/mc6801-brn.s", std::string{brn_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_brn.succeeded()
            && find_diagnostic(unstaged_brn, "E3401") != nullptr,
        "Unreviewed MC6801 BRN form was accepted"
    );

    const auto invalid_brn_indexed = jr800::assembler::assemble(
        Source{
            "src/brn-indexed.s",
            ".section .text, code\nBRN $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_brn_indexed.succeeded()
            && find_diagnostic(invalid_brn_indexed, "E3401") != nullptr,
        "BRN accepted an indexed operand"
    );

    constexpr std::string_view bhi_source =
        ".section .text, code\n"
        ".extern target\n"
        "BHI target\n";
    const auto assembled_bhi = jr800::assembler::assemble(
        Source{"src/bhi.s", std::string{bhi_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bhi, "BHI did not assemble");
    if (assembled_bhi.succeeded()) {
        const auto* text = find_section(assembled_bhi.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x22U, 0x00U}
                && assembled_bhi.output->object.relocations.size() == 1U
                && assembled_bhi.output->object.relocations.front().offset == 1U
                && assembled_bhi.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BHI encoding or relocation differs"
        );
    }

    const auto unstaged_bhi = jr800::assembler::assemble(
        Source{"src/mc6801-bhi.s", std::string{bhi_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bhi.succeeded()
            && find_diagnostic(unstaged_bhi, "E3401") != nullptr,
        "Unreviewed MC6801 BHI form was accepted"
    );

    const auto invalid_bhi_indexed = jr800::assembler::assemble(
        Source{
            "src/bhi-indexed.s",
            ".section .text, code\nBHI $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bhi_indexed.succeeded()
            && find_diagnostic(invalid_bhi_indexed, "E3401") != nullptr,
        "BHI accepted an indexed operand"
    );

    constexpr std::string_view bls_source =
        ".section .text, code\n"
        ".extern target\n"
        "BLS target\n";
    const auto assembled_bls = jr800::assembler::assemble(
        Source{"src/bls.s", std::string{bls_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bls, "BLS did not assemble");
    if (assembled_bls.succeeded()) {
        const auto* text = find_section(assembled_bls.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x23U, 0x00U}
                && assembled_bls.output->object.relocations.size() == 1U
                && assembled_bls.output->object.relocations.front().offset == 1U
                && assembled_bls.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BLS encoding or relocation differs"
        );
    }

    const auto unstaged_bls = jr800::assembler::assemble(
        Source{"src/mc6801-bls.s", std::string{bls_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bls.succeeded()
            && find_diagnostic(unstaged_bls, "E3401") != nullptr,
        "Unreviewed MC6801 BLS form was accepted"
    );

    const auto invalid_bls_indexed = jr800::assembler::assemble(
        Source{
            "src/bls-indexed.s",
            ".section .text, code\nBLS $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bls_indexed.succeeded()
            && find_diagnostic(invalid_bls_indexed, "E3401") != nullptr,
        "BLS accepted an indexed operand"
    );

    constexpr std::string_view bcc_source =
        ".section .text, code\n"
        ".extern target\n"
        "BCC target\n";
    const auto assembled_bcc = jr800::assembler::assemble(
        Source{"src/bcc.s", std::string{bcc_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bcc, "BCC did not assemble");
    if (assembled_bcc.succeeded()) {
        const auto* text = find_section(assembled_bcc.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x24U, 0x00U}
                && assembled_bcc.output->object.relocations.size() == 1U
                && assembled_bcc.output->object.relocations.front().offset == 1U
                && assembled_bcc.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BCC encoding or relocation differs"
        );
    }

    const auto unstaged_bcc = jr800::assembler::assemble(
        Source{"src/mc6801-bcc.s", std::string{bcc_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bcc.succeeded()
            && find_diagnostic(unstaged_bcc, "E3401") != nullptr,
        "Unreviewed MC6801 BCC form was accepted"
    );

    constexpr std::string_view bcs_source =
        ".section .text, code\n"
        ".extern target\n"
        "BCS target\n";
    const auto assembled_bcs = jr800::assembler::assemble(
        Source{"src/bcs.s", std::string{bcs_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bcs, "BCS did not assemble");
    if (assembled_bcs.succeeded()) {
        const auto* text = find_section(assembled_bcs.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x25U, 0x00U}
                && assembled_bcs.output->object.relocations.size() == 1U
                && assembled_bcs.output->object.relocations.front().offset == 1U
                && assembled_bcs.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BCS encoding or relocation differs"
        );
    }

    const auto unstaged_bcs = jr800::assembler::assemble(
        Source{"src/mc6801-bcs.s", std::string{bcs_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bcs.succeeded()
            && find_diagnostic(unstaged_bcs, "E3401") != nullptr,
        "Unreviewed MC6801 BCS form was accepted"
    );

    const auto invalid_bcs_indexed = jr800::assembler::assemble(
        Source{
            "src/bcs-indexed.s",
            ".section .text, code\nBCS $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bcs_indexed.succeeded()
            && find_diagnostic(invalid_bcs_indexed, "E3401") != nullptr,
        "BCS accepted an indexed operand"
    );

    constexpr std::string_view bne_source =
        ".section .text, code\n"
        ".extern target\n"
        "BNE target\n";
    const auto assembled_bne = jr800::assembler::assemble(
        Source{"src/bne.s", std::string{bne_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bne, "BNE did not assemble");
    if (assembled_bne.succeeded()) {
        const auto* text = find_section(assembled_bne.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x26U, 0x00U}
                && assembled_bne.output->object.relocations.size() == 1U
                && assembled_bne.output->object.relocations.front().offset == 1U
                && assembled_bne.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BNE encoding or relocation differs"
        );
    }

    const auto unstaged_bne = jr800::assembler::assemble(
        Source{"src/mc6801-bne.s", std::string{bne_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bne.succeeded()
            && find_diagnostic(unstaged_bne, "E3401") != nullptr,
        "Unreviewed MC6801 BNE form was accepted"
    );

    constexpr std::string_view beq_source =
        ".section .text, code\n"
        ".extern target\n"
        "BEQ target\n";
    const auto assembled_beq = jr800::assembler::assemble(
        Source{"src/beq.s", std::string{beq_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_beq, "BEQ did not assemble");
    if (assembled_beq.succeeded()) {
        const auto* text = find_section(assembled_beq.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x27U, 0x00U}
                && assembled_beq.output->object.relocations.size() == 1U
                && assembled_beq.output->object.relocations.front().offset == 1U
                && assembled_beq.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BEQ encoding or relocation differs"
        );
    }

    const auto unstaged_beq = jr800::assembler::assemble(
        Source{"src/mc6801-beq.s", std::string{beq_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_beq.succeeded()
            && find_diagnostic(unstaged_beq, "E3401") != nullptr,
        "Unreviewed MC6801 BEQ form was accepted"
    );

    const auto invalid_beq_indexed = jr800::assembler::assemble(
        Source{
            "src/beq-indexed.s",
            ".section .text, code\nBEQ $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_beq_indexed.succeeded()
            && find_diagnostic(invalid_beq_indexed, "E3401") != nullptr,
        "BEQ accepted an indexed operand"
    );

    constexpr std::string_view bvc_source =
        ".section .text, code\n"
        ".extern target\n"
        "BVC target\n";
    const auto assembled_bvc = jr800::assembler::assemble(
        Source{"src/bvc.s", std::string{bvc_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bvc, "BVC did not assemble");
    if (assembled_bvc.succeeded()) {
        const auto* text = find_section(assembled_bvc.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x28U, 0x00U}
                && assembled_bvc.output->object.relocations.size() == 1U
                && assembled_bvc.output->object.relocations.front().offset == 1U
                && assembled_bvc.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BVC encoding or relocation differs"
        );
    }

    const auto unstaged_bvc = jr800::assembler::assemble(
        Source{"src/mc6801-bvc.s", std::string{bvc_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bvc.succeeded()
            && find_diagnostic(unstaged_bvc, "E3401") != nullptr,
        "Unreviewed MC6801 BVC form was accepted"
    );

    const auto invalid_bvc_indexed = jr800::assembler::assemble(
        Source{
            "src/bvc-indexed.s",
            ".section .text, code\nBVC $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bvc_indexed.succeeded()
            && find_diagnostic(invalid_bvc_indexed, "E3401") != nullptr,
        "BVC accepted an indexed operand"
    );

    constexpr std::string_view bvs_source =
        ".section .text, code\n"
        ".extern target\n"
        "BVS target\n";
    const auto assembled_bvs = jr800::assembler::assemble(
        Source{"src/bvs.s", std::string{bvs_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bvs, "BVS did not assemble");
    if (assembled_bvs.succeeded()) {
        const auto* text = find_section(assembled_bvs.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x29U, 0x00U}
                && assembled_bvs.output->object.relocations.size() == 1U
                && assembled_bvs.output->object.relocations.front().offset == 1U
                && assembled_bvs.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BVS encoding or relocation differs"
        );
    }

    const auto unstaged_bvs = jr800::assembler::assemble(
        Source{"src/mc6801-bvs.s", std::string{bvs_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bvs.succeeded()
            && find_diagnostic(unstaged_bvs, "E3401") != nullptr,
        "Unreviewed MC6801 BVS form was accepted"
    );

    const auto invalid_bvs_indexed = jr800::assembler::assemble(
        Source{
            "src/bvs-indexed.s",
            ".section .text, code\nBVS $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bvs_indexed.succeeded()
            && find_diagnostic(invalid_bvs_indexed, "E3401") != nullptr,
        "BVS accepted an indexed operand"
    );

    constexpr std::string_view bpl_source =
        ".section .text, code\n"
        ".extern target\n"
        "BPL target\n";
    const auto assembled_bpl = jr800::assembler::assemble(
        Source{"src/bpl.s", std::string{bpl_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bpl, "BPL did not assemble");
    if (assembled_bpl.succeeded()) {
        const auto* text = find_section(assembled_bpl.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x2AU, 0x00U}
                && assembled_bpl.output->object.relocations.size() == 1U
                && assembled_bpl.output->object.relocations.front().offset == 1U
                && assembled_bpl.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BPL encoding or relocation differs"
        );
    }

    const auto unstaged_bpl = jr800::assembler::assemble(
        Source{"src/mc6801-bpl.s", std::string{bpl_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bpl.succeeded()
            && find_diagnostic(unstaged_bpl, "E3401") != nullptr,
        "Unreviewed MC6801 BPL form was accepted"
    );

    constexpr std::string_view bmi_source =
        ".section .text, code\n"
        ".extern target\n"
        "BMI target\n";
    const auto assembled_bmi = jr800::assembler::assemble(
        Source{"src/bmi.s", std::string{bmi_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bmi, "BMI did not assemble");
    if (assembled_bmi.succeeded()) {
        const auto* text = find_section(assembled_bmi.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x2BU, 0x00U}
                && assembled_bmi.output->object.relocations.size() == 1U
                && assembled_bmi.output->object.relocations.front().offset == 1U
                && assembled_bmi.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BMI encoding or relocation differs"
        );
    }

    const auto unstaged_bmi = jr800::assembler::assemble(
        Source{"src/mc6801-bmi.s", std::string{bmi_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bmi.succeeded()
            && find_diagnostic(unstaged_bmi, "E3401") != nullptr,
        "Unreviewed MC6801 BMI form was accepted"
    );

    const auto invalid_bmi_indexed = jr800::assembler::assemble(
        Source{
            "src/bmi-indexed.s",
            ".section .text, code\nBMI $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bmi_indexed.succeeded()
            && find_diagnostic(invalid_bmi_indexed, "E3401") != nullptr,
        "BMI accepted an indexed operand"
    );

    constexpr std::string_view bge_source =
        ".section .text, code\n"
        ".extern target\n"
        "BGE target\n";
    const auto assembled_bge = jr800::assembler::assemble(
        Source{"src/bge.s", std::string{bge_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bge, "BGE did not assemble");
    if (assembled_bge.succeeded()) {
        const auto* text = find_section(assembled_bge.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x2CU, 0x00U}
                && assembled_bge.output->object.relocations.size() == 1U
                && assembled_bge.output->object.relocations.front().offset == 1U
                && assembled_bge.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BGE encoding or relocation differs"
        );
    }

    const auto unstaged_bge = jr800::assembler::assemble(
        Source{"src/mc6801-bge.s", std::string{bge_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bge.succeeded()
            && find_diagnostic(unstaged_bge, "E3401") != nullptr,
        "Unreviewed MC6801 BGE form was accepted"
    );

    const auto invalid_bge_indexed = jr800::assembler::assemble(
        Source{
            "src/bge-indexed.s",
            ".section .text, code\nBGE $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bge_indexed.succeeded()
            && find_diagnostic(invalid_bge_indexed, "E3401") != nullptr,
        "BGE accepted an indexed operand"
    );

    constexpr std::string_view blt_source =
        ".section .text, code\n"
        ".extern target\n"
        "BLT target\n";
    const auto assembled_blt = jr800::assembler::assemble(
        Source{"src/blt.s", std::string{blt_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_blt, "BLT did not assemble");
    if (assembled_blt.succeeded()) {
        const auto* text = find_section(assembled_blt.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x2DU, 0x00U}
                && assembled_blt.output->object.relocations.size() == 1U
                && assembled_blt.output->object.relocations.front().offset == 1U
                && assembled_blt.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BLT encoding or relocation differs"
        );
    }

    const auto unstaged_blt = jr800::assembler::assemble(
        Source{"src/mc6801-blt.s", std::string{blt_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_blt.succeeded()
            && find_diagnostic(unstaged_blt, "E3401") != nullptr,
        "Unreviewed MC6801 BLT form was accepted"
    );

    const auto invalid_blt_indexed = jr800::assembler::assemble(
        Source{
            "src/blt-indexed.s",
            ".section .text, code\nBLT $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_blt_indexed.succeeded()
            && find_diagnostic(invalid_blt_indexed, "E3401") != nullptr,
        "BLT accepted an indexed operand"
    );

    constexpr std::string_view bgt_source =
        ".section .text, code\n"
        ".extern target\n"
        "BGT target\n";
    const auto assembled_bgt = jr800::assembler::assemble(
        Source{"src/bgt.s", std::string{bgt_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_bgt, "BGT did not assemble");
    if (assembled_bgt.succeeded()) {
        const auto* text = find_section(assembled_bgt.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x2EU, 0x00U}
                && assembled_bgt.output->object.relocations.size() == 1U
                && assembled_bgt.output->object.relocations.front().offset == 1U
                && assembled_bgt.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BGT encoding or relocation differs"
        );
    }

    const auto unstaged_bgt = jr800::assembler::assemble(
        Source{"src/mc6801-bgt.s", std::string{bgt_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_bgt.succeeded()
            && find_diagnostic(unstaged_bgt, "E3401") != nullptr,
        "Unreviewed MC6801 BGT form was accepted"
    );

    const auto invalid_bgt_indexed = jr800::assembler::assemble(
        Source{
            "src/bgt-indexed.s",
            ".section .text, code\nBGT $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_bgt_indexed.succeeded()
            && find_diagnostic(invalid_bgt_indexed, "E3401") != nullptr,
        "BGT accepted an indexed operand"
    );

    constexpr std::string_view ble_source =
        ".section .text, code\n"
        ".extern target\n"
        "BLE target\n";
    const auto assembled_ble = jr800::assembler::assemble(
        Source{"src/ble.s", std::string{ble_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(assembled_ble, "BLE did not assemble");
    if (assembled_ble.succeeded()) {
        const auto* text = find_section(assembled_ble.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x2FU, 0x00U}
                && assembled_ble.output->object.relocations.size() == 1U
                && assembled_ble.output->object.relocations.front().offset == 1U
                && assembled_ble.output->object.relocations.front().type
                    == RelocationType::rel8,
            "BLE encoding or relocation differs"
        );
    }

    const auto unstaged_ble = jr800::assembler::assemble(
        Source{"src/mc6801-ble.s", std::string{ble_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_ble.succeeded()
            && find_diagnostic(unstaged_ble, "E3401") != nullptr,
        "Unreviewed MC6801 BLE form was accepted"
    );

    const auto invalid_ble_indexed = jr800::assembler::assemble(
        Source{
            "src/ble-indexed.s",
            ".section .text, code\nBLE $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !invalid_ble_indexed.succeeded()
            && find_diagnostic(invalid_ble_indexed, "E3401") != nullptr,
        "BLE accepted an indexed operand"
    );
    return passed;
}

bool test_or_immediate_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "OIM #$03, $20\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/oim-absolute.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Absolute OIM did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x72U, 0x03U, 0x20U}
                && absolute.output->object.relocations.empty(),
            "Absolute OIM encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern mask\n"
        ".extern destination\n"
        "OIM #mask, destination\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/oim-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated OIM did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x72U, 0x00U, 0x00U}
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs8
                && relocations[1].offset == 2U
                && relocations[1].type == RelocationType::direct8,
            "Relocated OIM encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-oim.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "HD6301V1-only OIM was accepted for MC6801"
    );

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "OIM #$03, $20,x\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/oim-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed OIM did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x62U, 0x03U, 0x20U}
                && indexed.output->object.relocations.empty(),
            "Indexed OIM encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/oim-indexed-range.s",
            ".section .text, code\nOIM #$03, 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed OIM accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/oim-indexed-negative.s",
            ".section .text, code\nOIM #$03, -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed OIM accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern MASK\n"
        ".extern DISP\n"
        ".section .text, code\n"
        "OIM #MASK, DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/oim-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed OIM did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x62U, 0x00U, 0x00U}
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs8
                && relocations[1].offset == 2U
                && relocations[1].type == RelocationType::abs8,
            "Relocated indexed OIM encoding differs"
        );
    }

    const auto wrong_index_register = jr800::assembler::assemble(
        Source{
            "src/oim-indexed-register.s",
            ".section .text, code\nOIM #$03, $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index_register.succeeded(),
        "Indexed OIM accepted a register other than X"
    );

    const auto unstaged_indexed = jr800::assembler::assemble(
        Source{"src/mc6801-oim-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed.succeeded()
            && find_diagnostic(unstaged_indexed, "E3401") != nullptr,
        "Indexed OIM was accepted for MC6801"
    );
    return passed;
}

bool test_and_exclusive_or_immediate_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "EIM #$00, $00\n"
        "EIM #$F0, $20\n"
        "EIM #$FF, $FF\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/eim-direct.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Direct EIM did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x75U, 0x00U, 0x00U,
                    0x75U, 0xF0U, 0x20U,
                    0x75U, 0xFFU, 0xFFU,
                }
                && absolute.output->object.relocations.empty(),
            "Direct EIM encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern MASK\n"
        ".extern ADDRESS\n"
        ".section .text, code\n"
        "EIM #MASK, ADDRESS\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eim-direct-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated direct EIM did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x75U, 0x00U, 0x00U}
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs8
                && relocations[1].offset == 2U
                && relocations[1].type == RelocationType::direct8,
            "Relocated direct EIM encoding differs"
        );
    }

    const auto immediate_out_of_range = jr800::assembler::assemble(
        Source{
            "src/eim-direct-immediate-range.s",
            ".section .text, code\nEIM #256, $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !immediate_out_of_range.succeeded()
            && find_diagnostic(immediate_out_of_range, "E3502") != nullptr,
        "Direct EIM accepted an immediate above 255"
    );

    const auto address_out_of_range = jr800::assembler::assemble(
        Source{
            "src/eim-direct-address-range.s",
            ".section .text, code\nEIM #$F0, $100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !address_out_of_range.succeeded()
            && find_diagnostic(address_out_of_range, "E3503") != nullptr,
        "Direct EIM accepted an address above page zero"
    );

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "AIM #$0F, $20,X\n"
        "EIM #$F0, $21,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{
            "src/aim-eim-indexed.s",
            std::string{indexed_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed AIM/EIM did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x61U, 0x0FU, 0x20U,
                    0x65U, 0xF0U, 0x21U,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed AIM/EIM encoding differs"
        );
    }

    constexpr std::string_view indexed_relocated_source =
        ".extern MASK\n"
        ".extern DISP\n"
        ".section .text, code\n"
        "EIM #MASK, DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/eim-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed EIM did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x65U, 0x00U, 0x00U}
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs8
                && relocations[1].offset == 2U
                && relocations[1].type == RelocationType::abs8,
            "Relocated indexed EIM encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eim.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "HD6301V1-only direct EIM was accepted for MC6801"
    );
    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-aim-eim-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "HD6301V1-only indexed AIM/EIM was accepted for MC6801"
    );
    return passed;
}

bool test_indexed_test_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "TIM #$F0, $20,X\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/tim-indexed.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Indexed TIM did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x6BU, 0xF0U, 0x20U}
                && absolute.output->object.relocations.empty(),
            "Indexed TIM encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern MASK\n"
        ".extern DISP\n"
        ".section .text, code\n"
        "TIM #MASK, DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/tim-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed TIM did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x6BU, 0x00U, 0x00U}
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs8
                && relocations[1].offset == 2U
                && relocations[1].type == RelocationType::abs8,
            "Relocated indexed TIM encoding differs"
        );
    }

    const auto immediate_out_of_range = jr800::assembler::assemble(
        Source{
            "src/tim-immediate-range.s",
            ".section .text, code\nTIM #256, $20,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !immediate_out_of_range.succeeded()
            && find_diagnostic(immediate_out_of_range, "E3502") != nullptr,
        "Indexed TIM accepted an immediate above 255"
    );

    const auto displacement_out_of_range = jr800::assembler::assemble(
        Source{
            "src/tim-displacement-range.s",
            ".section .text, code\nTIM #$F0, 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !displacement_out_of_range.succeeded()
            && find_diagnostic(displacement_out_of_range, "E3507") != nullptr,
        "Indexed TIM accepted a displacement above 255"
    );

    const auto negative_displacement = jr800::assembler::assemble(
        Source{
            "src/tim-negative-displacement.s",
            ".section .text, code\nTIM #$F0, -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative_displacement.succeeded()
            && find_diagnostic(negative_displacement, "E3507") != nullptr,
        "Indexed TIM accepted a negative displacement"
    );

    const auto wrong_index_register = jr800::assembler::assemble(
        Source{
            "src/tim-index-register.s",
            ".section .text, code\nTIM #$F0, $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index_register.succeeded(),
        "Indexed TIM accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tim.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "HD6301V1-only indexed TIM was accepted for MC6801"
    );
    return passed;
}

bool test_direct_test_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "TIM #$00, $00\n"
        "TIM #$F0, $20\n"
        "TIM #$FF, $FF\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/tim-direct.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Direct TIM did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x7BU, 0x00U, 0x00U,
                    0x7BU, 0xF0U, 0x20U,
                    0x7BU, 0xFFU, 0xFFU,
                }
                && absolute.output->object.relocations.empty(),
            "Direct TIM encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern MASK\n"
        ".extern ADDRESS\n"
        ".section .text, code\n"
        "TIM #MASK, ADDRESS\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/tim-direct-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated direct TIM did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x7BU, 0x00U, 0x00U}
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs8
                && relocations[1].offset == 2U
                && relocations[1].type == RelocationType::direct8,
            "Relocated direct TIM encoding differs"
        );
    }

    const auto immediate_out_of_range = jr800::assembler::assemble(
        Source{
            "src/tim-direct-immediate-range.s",
            ".section .text, code\nTIM #256, $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !immediate_out_of_range.succeeded()
            && find_diagnostic(immediate_out_of_range, "E3502") != nullptr,
        "Direct TIM accepted an immediate above 255"
    );

    const auto address_out_of_range = jr800::assembler::assemble(
        Source{
            "src/tim-direct-address-range.s",
            ".section .text, code\nTIM #$F0, $100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !address_out_of_range.succeeded()
            && find_diagnostic(address_out_of_range, "E3503") != nullptr,
        "Direct TIM accepted an address above page zero"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tim-direct.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "HD6301V1-only direct TIM was accepted for MC6801"
    );
    return passed;
}

bool test_tst_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "TST $0000\n"
        "TST $00FF\n"
        "TST $0100\n"
        "TST $8123\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/tst-extended.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Extended TST did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x7DU, 0x00U, 0x00U,
                    0x7DU, 0x00U, 0xFFU,
                    0x7DU, 0x01U, 0x00U,
                    0x7DU, 0x81U, 0x23U,
                }
                && absolute.output->object.relocations.empty(),
            "Extended TST encoding or small-address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ target_byte, $20\n"
        ".section .text, code\n"
        "TST target_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/tst-extended-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic extended TST did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x7DU, 0x00U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic extended TST encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target_byte\n"
        ".section .text, code\n"
        "TST target_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/tst-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended TST did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x7DU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended TST encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/tst-extended-range.s",
            ".section .text, code\nTST $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended TST accepted an address above 16 bits"
    );

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "TST $00,X\n"
        "TST $20,x\n"
        "TST $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/tst-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed TST did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x6DU, 0x00U,
                    0x6DU, 0x20U,
                    0x6DU, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed TST encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/tst-indexed-range.s",
            ".section .text, code\nTST 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed TST accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/tst-indexed-negative.s",
            ".section .text, code\nTST -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed TST accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern OFFSET\n"
        ".section .text, code\n"
        "TST OFFSET,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/tst-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed TST did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x6DU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed TST encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/tst-indexed-register.s",
            ".section .text, code\nTST $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed TST accepted a register other than X"
    );

    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tst-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed TST form was accepted"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tst-extended.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 extended TST form was accepted"
    );
    return passed;
}

bool test_accumulator_test_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TSTA\n"
        "TSTB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/tst-accumulators.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(
        assembled,
        "Accumulator TST instructions did not assemble"
    );
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x4DU, 0x5DU}
                && assembled.output->object.relocations.empty(),
            "Accumulator TST encodings differ"
        );
    }

    const auto tsta_extra_operand = jr800::assembler::assemble(
        Source{
            "src/tsta-extra-operand.s",
            ".section .text, code\nTSTA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !tsta_extra_operand.succeeded()
            && find_diagnostic(tsta_extra_operand, "E3401") != nullptr,
        "TSTA accepted an operand"
    );

    const auto tstb_extra_operand = jr800::assembler::assemble(
        Source{
            "src/tstb-extra-operand.s",
            ".section .text, code\nTSTB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !tstb_extra_operand.succeeded()
            && find_diagnostic(tstb_extra_operand, "E3401") != nullptr,
        "TSTB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tst-accumulators.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 accumulator TST form was accepted"
    );
    return passed;
}

bool test_load_instruction_address_selection() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "LDAA $20\n"
        "LDAA $FF\n"
        "LDAA $100\n"
        "LDAA $8123\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/ldaa-absolute.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(
        absolute,
        "Absolute LDAA address selection did not assemble"
    );
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0x96U, 0x20U,
                        0x96U, 0xFFU,
                        0xB6U, 0x01U, 0x00U,
                        0xB6U, 0x81U, 0x23U,
                    }
                && absolute.output->object.relocations.empty(),
            "Absolute LDAA direct/extended selection differs"
        );
    }

    constexpr std::string_view equ_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "LDAA direct_byte\n";
    const auto equ = jr800::assembler::assemble(
        Source{"src/ldaa-direct-equ.s", std::string{equ_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(equ, "Absolute-symbol direct LDAA did not assemble");
    if (equ.succeeded()) {
        const auto* text = find_section(equ.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x96U, 0x20U}
                && equ.output->object.relocations.empty(),
            "Absolute-symbol direct LDAA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern source_byte\n"
        "LDAA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldaa-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended LDAA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB6U, 0x00U, 0x00U}
                && relocated.output->object.relocations.size() == 1U
                && relocated.output->object.relocations.front().offset == 1U
                && relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated extended LDAA encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{
            "src/mc6801-ldaa-addresses.s",
            ".section .text, code\nLDAA $20\nLDAA $8123\n"
        },
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed LDAA form was accepted"
    );
    return passed;
}

bool test_store_instruction_address_selection() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "STAA $20\n"
        "STAA $FF\n"
        "STAA $100\n"
        "STAA $8123\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/staa-absolute.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(
        absolute,
        "Absolute STAA address selection did not assemble"
    );
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0x97U, 0x20U,
                        0x97U, 0xFFU,
                        0xB7U, 0x01U, 0x00U,
                        0xB7U, 0x81U, 0x23U,
                    }
                && absolute.output->object.relocations.empty(),
            "Absolute STAA direct/extended selection differs"
        );
    }

    constexpr std::string_view equ_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "STAA direct_byte\n";
    const auto equ = jr800::assembler::assemble(
        Source{"src/staa-direct-equ.s", std::string{equ_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(equ.succeeded(), "Absolute-symbol direct STAA failed");
    if (equ.succeeded()) {
        const auto* text = find_section(equ.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x97U, 0x20U}
                && equ.output->object.relocations.empty(),
            "Absolute-symbol direct STAA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern destination\n"
        "STAA destination\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/staa-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended STAA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB7U, 0x00U, 0x00U}
                && relocated.output->object.relocations.size() == 1U
                && relocated.output->object.relocations.front().offset == 1U
                && relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated extended STAA encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{
            "src/mc6801-staa-extended.s",
            ".section .text, code\nSTAA $8123\n",
        },
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3503") != nullptr,
        "Unreviewed MC6801 extended STAA form was accepted"
    );
    return passed;
}

bool test_stab_address_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "STAB $00\n"
        "STAB $20\n"
        "STAB $FF\n"
        "STAB $100\n"
        "STAB $8123\n"
        "STAB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/stab-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed STAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD7U, 0x00U,
                    0xD7U, 0x20U,
                    0xD7U, 0xFFU,
                    0xF7U, 0x01U, 0x00U,
                    0xF7U, 0x81U, 0x23U,
                    0xF7U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "STAB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "STAB direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/stab-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct STAB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD7U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct STAB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern destination\n"
        ".section .text, code\n"
        "STAB destination\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/stab-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended STAB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF7U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended STAB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/stab-extended-range.s",
            ".section .text, code\nSTAB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended STAB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-stab-direct.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed STAB forms were accepted"
    );
    return passed;
}

bool test_stab_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "STAB $00,X\n"
        "STAB $20,x\n"
        "STAB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/stab-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed STAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE7U, 0x00U,
                    0xE7U, 0x20U,
                    0xE7U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed STAB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/stab-indexed-range.s",
            ".section .text, code\nSTAB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed STAB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/stab-indexed-negative.s",
            ".section .text, code\nSTAB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed STAB accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/stab-indexed-register.s",
            ".section .text, code\nSTAB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed STAB accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "STAB DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/stab-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed STAB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE7U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed STAB encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-stab-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed STAB form was accepted"
    );
    return passed;
}

bool test_indexed_store_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "STAA $20,x\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/staa-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed STAA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA7U, 0x20U}
                && assembled.output->object.relocations.empty(),
            "Indexed STAA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/staa-indexed-range.s",
            ".section .text, code\nSTAA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed STAA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/staa-indexed-negative.s",
            ".section .text, code\nSTAA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed STAA accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/staa-indexed-register.s",
            ".section .text, code\nSTAA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed STAA accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "STAA DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/staa-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed STAA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA7U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed STAA encoding differs"
        );
    }

    const auto unstaged = jr800::assembler::assemble(
        Source{"src/mc6801-staa-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged.succeeded()
            && find_diagnostic(unstaged, "E3401") != nullptr,
        "Indexed STAA was accepted for MC6801"
    );
    return passed;
}

bool test_indexed_load_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDAA $20,x\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldaa-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed LDAA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA6U, 0x20U}
                && assembled.output->object.relocations.empty(),
            "Indexed LDAA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldaa-indexed-range.s",
            ".section .text, code\nLDAA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed LDAA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/ldaa-indexed-negative.s",
            ".section .text, code\nLDAA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed LDAA accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/ldaa-indexed-register.s",
            ".section .text, code\nLDAA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed LDAA accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "LDAA DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldaa-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed LDAA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA6U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed LDAA encoding differs"
        );
    }

    const auto unstaged = jr800::assembler::assemble(
        Source{"src/mc6801-ldaa-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged.succeeded()
            && find_diagnostic(unstaged, "E3401") != nullptr,
        "Indexed LDAA was accepted for MC6801"
    );
    return passed;
}

bool test_clear_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "CLR $20,x\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/clr-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed CLR did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x6FU, 0x20U}
                && assembled.output->object.relocations.empty(),
            "Indexed CLR encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/clr-indexed-range.s",
            ".section .text, code\nCLR 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed CLR accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/clr-indexed-negative.s",
            ".section .text, code\nCLR -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed CLR accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/clr-indexed-register.s",
            ".section .text, code\nCLR $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed CLR accepted a register other than X"
    );

    constexpr std::string_view extended_source =
        ".section .text, code\n"
        "CLR $0020\n"
        "CLR $8123\n"
        "CLR $FFFF\n";
    const auto extended = jr800::assembler::assemble(
        Source{"src/clr-extended.s", std::string{extended_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(extended, "Extended CLR did not assemble");
    if (extended.succeeded()) {
        const auto* text = find_section(extended.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x7FU, 0x00U, 0x20U,
                    0x7FU, 0x81U, 0x23U,
                    0x7FU, 0xFFU, 0xFFU,
                }
                && extended.output->object.relocations.empty(),
            "Extended CLR encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "CLR DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/clr-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed CLR did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x6FU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed CLR encoding differs"
        );
    }

    constexpr std::string_view extended_relocated_source =
        ".extern DESTINATION\n"
        ".section .text, code\n"
        "CLR DESTINATION\n";
    const auto extended_relocated = jr800::assembler::assemble(
        Source{
            "src/clr-extended-relocated.s",
            std::string{extended_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        extended_relocated,
        "Relocated extended CLR did not assemble"
    );
    if (extended_relocated.succeeded()) {
        const auto* text = find_section(
            extended_relocated.output->object,
            ".text"
        );
        const auto& relocations = extended_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x7FU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended CLR encoding differs"
        );
    }

    const auto unstaged = jr800::assembler::assemble(
        Source{"src/mc6801-clr-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged.succeeded()
            && find_diagnostic(unstaged, "E3401") != nullptr,
        "Indexed CLR was accepted for MC6801"
    );
    const auto unstaged_extended = jr800::assembler::assemble(
        Source{"src/mc6801-clr-extended.s", std::string{extended_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_extended.succeeded()
            && find_diagnostic(unstaged_extended, "E3401") != nullptr,
        "Extended CLR was accepted for MC6801"
    );
    return passed;
}

bool test_deca_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "DECA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/deca.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "DECA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x4AU}
                && assembled.output->object.relocations.empty(),
            "DECA encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-deca.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 DECA form was accepted"
    );
    return passed;
}

bool test_clrb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "CLRB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/clrb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "CLRB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x5FU}
                && assembled.output->object.relocations.empty(),
            "CLRB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/clrb-operand.s",
            ".section .text, code\nCLRB $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "CLRB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-clrb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 CLRB form was accepted"
    );
    return passed;
}

bool test_decb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "DECB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/decb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "DECB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x5AU}
                && assembled.output->object.relocations.empty(),
            "DECB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/decb-operand.s",
            ".section .text, code\nDECB $20\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "DECB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-decb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 DECB form was accepted"
    );
    return passed;
}

bool test_inca_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "INCA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/inca.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "INCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x4CU}
                && assembled.output->object.relocations.empty(),
            "INCA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/inca-extra-operand.s",
            ".section .text, code\nINCA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "INCA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-inca.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 INCA form was accepted"
    );
    return passed;
}

bool test_incb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "INCB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/incb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "INCB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x5CU}
                && assembled.output->object.relocations.empty(),
            "INCB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/incb-extra-operand.s",
            ".section .text, code\nINCB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "INCB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-incb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 INCB form was accepted"
    );
    return passed;
}

bool test_inc_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "INC $0000\n"
        "INC $00FF\n"
        "INC $0100\n"
        "INC $8123\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/inc-extended.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Extended INC did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x7CU, 0x00U, 0x00U,
                    0x7CU, 0x00U, 0xFFU,
                    0x7CU, 0x01U, 0x00U,
                    0x7CU, 0x81U, 0x23U,
                }
                && assembled.output->object.relocations.empty(),
            "Extended INC encoding or small-address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ counter, $20\n"
        ".section .text, code\n"
        "INC counter\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/inc-extended-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic extended INC did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x7CU, 0x00U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic extended INC encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern counter\n"
        ".section .text, code\n"
        "INC counter\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/inc-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended INC did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x7CU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended INC encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/inc-extended-range.s",
            ".section .text, code\nINC $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended INC accepted an address above 16 bits"
    );

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "INC $00,X\n"
        "INC $20,x\n"
        "INC $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/inc-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed INC did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x6CU, 0x00U,
                    0x6CU, 0x20U,
                    0x6CU, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed INC encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/inc-indexed-range.s",
            ".section .text, code\nINC 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed INC accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/inc-indexed-negative.s",
            ".section .text, code\nINC -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed INC accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern OFFSET\n"
        ".section .text, code\n"
        "INC OFFSET,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/inc-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed INC did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x6CU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed INC encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/inc-indexed-register.s",
            ".section .text, code\nINC $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed INC accepted a register other than X"
    );

    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-inc-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed INC form was accepted"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-inc-extended.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 extended INC form was accepted"
    );
    return passed;
}

bool test_neg_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "NEG $0000\n"
        "NEG $8123\n"
        "NEG $00,X\n"
        "NEG $20,x\n"
        "NEG $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/neg-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory NEG did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x70U, 0x00U, 0x00U,
                    0x70U, 0x81U, 0x23U,
                    0x60U, 0x00U,
                    0x60U, 0x20U,
                    0x60U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory NEG encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "NEG target\n"
        "NEG OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/neg-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory NEG did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x70U, 0x00U, 0x00U,
                    0x60U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory NEG encoding differs"
        );
    }

    const auto extended_out_of_range = jr800::assembler::assemble(
        Source{
            "src/neg-extended-range.s",
            ".section .text, code\nNEG $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extended_out_of_range.succeeded()
            && find_diagnostic(extended_out_of_range, "E3504") != nullptr,
        "Extended NEG accepted an address above 16 bits"
    );

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/neg-indexed-range.s",
            ".section .text, code\nNEG 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed NEG accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/neg-indexed-negative.s",
            ".section .text, code\nNEG -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed NEG accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/neg-indexed-register.s",
            ".section .text, code\nNEG $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed NEG accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-neg-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory NEG forms were accepted"
    );
    return passed;
}

bool test_com_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "COM $0000\n"
        "COM $8123\n"
        "COM $00,X\n"
        "COM $20,x\n"
        "COM $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/com-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory COM did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x73U, 0x00U, 0x00U,
                    0x73U, 0x81U, 0x23U,
                    0x63U, 0x00U,
                    0x63U, 0x20U,
                    0x63U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory COM encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "COM target\n"
        "COM OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/com-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory COM did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x73U, 0x00U, 0x00U,
                    0x63U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory COM encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-com-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory COM forms were accepted"
    );
    return passed;
}

bool test_lsr_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LSR $0000\n"
        "LSR $8123\n"
        "LSR $00,X\n"
        "LSR $20,x\n"
        "LSR $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/lsr-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory LSR did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x74U, 0x00U, 0x00U,
                    0x74U, 0x81U, 0x23U,
                    0x64U, 0x00U,
                    0x64U, 0x20U,
                    0x64U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory LSR encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "LSR target\n"
        "LSR OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/lsr-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory LSR did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x74U, 0x00U, 0x00U,
                    0x64U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory LSR encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lsr-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory LSR forms were accepted"
    );
    return passed;
}

bool test_ror_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ROR $0000\n"
        "ROR $8123\n"
        "ROR $00,X\n"
        "ROR $20,x\n"
        "ROR $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ror-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory ROR did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x76U, 0x00U, 0x00U,
                    0x76U, 0x81U, 0x23U,
                    0x66U, 0x00U,
                    0x66U, 0x20U,
                    0x66U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory ROR encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "ROR target\n"
        "ROR OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ror-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory ROR did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x76U, 0x00U, 0x00U,
                    0x66U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory ROR encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ror-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory ROR forms were accepted"
    );
    return passed;
}

bool test_asr_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ASR $0000\n"
        "ASR $8123\n"
        "ASR $00,X\n"
        "ASR $20,x\n"
        "ASR $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/asr-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory ASR did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x77U, 0x00U, 0x00U,
                    0x77U, 0x81U, 0x23U,
                    0x67U, 0x00U,
                    0x67U, 0x20U,
                    0x67U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory ASR encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "ASR target\n"
        "ASR OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/asr-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory ASR did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x77U, 0x00U, 0x00U,
                    0x67U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory ASR encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-asr-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory ASR forms were accepted"
    );
    return passed;
}

bool test_asl_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ASL $0000\n"
        "ASL $8123\n"
        "ASL $00,X\n"
        "ASL $20,x\n"
        "ASL $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/asl-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory ASL did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x78U, 0x00U, 0x00U,
                    0x78U, 0x81U, 0x23U,
                    0x68U, 0x00U,
                    0x68U, 0x20U,
                    0x68U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory ASL encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "ASL target\n"
        "ASL OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/asl-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory ASL did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x78U, 0x00U, 0x00U,
                    0x68U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory ASL encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-asl-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory ASL forms were accepted"
    );
    return passed;
}

bool test_rol_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ROL $0000\n"
        "ROL $8123\n"
        "ROL $00,X\n"
        "ROL $20,x\n"
        "ROL $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/rol-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory ROL did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x79U, 0x00U, 0x00U,
                    0x79U, 0x81U, 0x23U,
                    0x69U, 0x00U,
                    0x69U, 0x20U,
                    0x69U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory ROL encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "ROL target\n"
        "ROL OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/rol-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory ROL did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x79U, 0x00U, 0x00U,
                    0x69U, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory ROL encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-rol-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory ROL forms were accepted"
    );
    return passed;
}

bool test_dec_memory_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "DEC $0000\n"
        "DEC $8123\n"
        "DEC $00,X\n"
        "DEC $20,x\n"
        "DEC $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/dec-memory.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Memory DEC did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x7AU, 0x00U, 0x00U,
                    0x7AU, 0x81U, 0x23U,
                    0x6AU, 0x00U,
                    0x6AU, 0x20U,
                    0x6AU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Memory DEC encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern target\n"
        ".extern OFFSET\n"
        ".section .text, code\n"
        "DEC target\n"
        "DEC OFFSET,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/dec-memory-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated memory DEC did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x7AU, 0x00U, 0x00U,
                    0x6AU, 0x00U,
                }
                && relocations.size() == 2U
                && relocations[0].offset == 1U
                && relocations[0].type == RelocationType::abs16_be
                && relocations[1].offset == 4U
                && relocations[1].type == RelocationType::abs8,
            "Relocated memory DEC encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-dec-memory.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 memory DEC forms were accepted"
    );
    return passed;
}

bool test_inx_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "INX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/inx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "INX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x08U}
                && assembled.output->object.relocations.empty(),
            "INX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/inx-extra-operand.s",
            ".section .text, code\nINX $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "INX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-inx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 INX form was accepted"
    );
    return passed;
}

bool test_dex_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "DEX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/dex.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "DEX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x09U}
                && assembled.output->object.relocations.empty(),
            "DEX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/dex-extra-operand.s",
            ".section .text, code\nDEX $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "DEX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-dex.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 DEX form was accepted"
    );
    return passed;
}

bool test_mul_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "MUL\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/mul.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "MUL did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x3DU}
                && assembled.output->object.relocations.empty(),
            "MUL encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/mul-extra-operand.s",
            ".section .text, code\nMUL $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "MUL accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-mul.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 MUL form was accepted"
    );
    return passed;
}

bool test_tab_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TAB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/tab.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "TAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x16U}
                && assembled.output->object.relocations.empty(),
            "TAB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/tab-extra-operand.s",
            ".section .text, code\nTAB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "TAB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tab.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 TAB form was accepted"
    );
    return passed;
}

bool test_tba_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "TBA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/tba.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "TBA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x17U}
                && assembled.output->object.relocations.empty(),
            "TBA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/tba-extra-operand.s",
            ".section .text, code\nTBA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "TBA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-tba.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 TBA form was accepted"
    );
    return passed;
}

bool test_aba_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ABA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/aba.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ABA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x1BU}
                && assembled.output->object.relocations.empty(),
            "ABA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/aba-extra-operand.s",
            ".section .text, code\nABA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ABA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-aba.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ABA form was accepted"
    );
    return passed;
}

bool test_cba_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "CBA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cba.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "CBA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x11U}
                && assembled.output->object.relocations.empty(),
            "CBA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/cba-extra-operand.s",
            ".section .text, code\nCBA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "CBA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cba.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 CBA form was accepted"
    );
    return passed;
}

bool test_sba_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "SBA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sba.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "SBA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x10U}
                && assembled.output->object.relocations.empty(),
            "SBA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/sba-extra-operand.s",
            ".section .text, code\nSBA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "SBA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sba.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 SBA form was accepted"
    );
    return passed;
}

bool test_lsrd_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "LSRD\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/lsrd.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "LSRD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x04U}
                && assembled.output->object.relocations.empty(),
            "LSRD encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/lsrd-extra-operand.s",
            ".section .text, code\nLSRD $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "LSRD accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lsrd.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 LSRD form was accepted"
    );
    return passed;
}

bool test_asld_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ASLD\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/asld.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ASLD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x05U}
                && assembled.output->object.relocations.empty(),
            "ASLD encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/asld-extra-operand.s",
            ".section .text, code\nASLD $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ASLD accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-asld.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ASLD form was accepted"
    );
    return passed;
}

bool test_abx_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ABX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/abx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ABX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x3AU}
                && assembled.output->object.relocations.empty(),
            "ABX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/abx-extra-operand.s",
            ".section .text, code\nABX $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ABX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-abx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ABX form was accepted"
    );
    return passed;
}

bool test_xgdx_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "XGDX\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/xgdx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "XGDX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x18U}
                && assembled.output->object.relocations.empty(),
            "XGDX encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/xgdx-extra-operand.s",
            ".section .text, code\nXGDX $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "XGDX accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-xgdx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 XGDX form was accepted"
    );
    return passed;
}

bool test_slp_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "SLP\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/slp.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "SLP did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x1AU}
                && assembled.output->object.relocations.empty(),
            "SLP encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/slp-extra-operand.s",
            ".section .text, code\nSLP $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "SLP accepted an operand"
    );

    const auto undefined_profile = jr800::assembler::assemble(
        Source{"src/mc6801-slp.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !undefined_profile.succeeded()
            && find_diagnostic(undefined_profile, "E3401") != nullptr,
        "Undefined MC6801 opcode 0x1A accepted SLP"
    );
    return passed;
}

bool test_wai_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "WAI\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/wai.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "WAI did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x3EU}
                && assembled.output->object.relocations.empty(),
            "WAI encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/wai-extra-operand.s",
            ".section .text, code\nWAI $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "WAI accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-wai.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 WAI form was accepted"
    );
    return passed;
}

bool test_swi_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "SWI\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/swi.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "SWI did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x3FU}
                && assembled.output->object.relocations.empty(),
            "SWI encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/swi-extra-operand.s",
            ".section .text, code\nSWI $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "SWI accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-swi.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 SWI form was accepted"
    );
    return passed;
}

bool test_daa_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "DAA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/daa.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "DAA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x19U}
                && assembled.output->object.relocations.empty(),
            "DAA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/daa-extra-operand.s",
            ".section .text, code\nDAA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "DAA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-daa.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 DAA form was accepted"
    );
    return passed;
}

bool test_rti_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "RTI\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/rti.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "RTI did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x3BU}
                && assembled.output->object.relocations.empty(),
            "RTI encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/rti-extra-operand.s",
            ".section .text, code\nRTI $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "RTI accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-rti.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 RTI form was accepted"
    );
    return passed;
}

bool test_nega_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "NEGA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/nega.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "NEGA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x40U}
                && assembled.output->object.relocations.empty(),
            "NEGA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/nega-extra-operand.s",
            ".section .text, code\nNEGA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "NEGA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-nega.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 NEGA form was accepted"
    );
    return passed;
}

bool test_negb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "NEGB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/negb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "NEGB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x50U}
                && assembled.output->object.relocations.empty(),
            "NEGB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/negb-extra-operand.s",
            ".section .text, code\nNEGB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "NEGB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-negb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 NEGB form was accepted"
    );
    return passed;
}

bool test_coma_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "COMA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/coma.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "COMA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x43U}
                && assembled.output->object.relocations.empty(),
            "COMA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/coma-extra-operand.s",
            ".section .text, code\nCOMA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "COMA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-coma.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 COMA form was accepted"
    );
    return passed;
}

bool test_comb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "COMB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/comb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "COMB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x53U}
                && assembled.output->object.relocations.empty(),
            "COMB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/comb-extra-operand.s",
            ".section .text, code\nCOMB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "COMB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-comb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 COMB form was accepted"
    );
    return passed;
}

bool test_lsra_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "LSRA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/lsra.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "LSRA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x44U}
                && assembled.output->object.relocations.empty(),
            "LSRA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/lsra-extra-operand.s",
            ".section .text, code\nLSRA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "LSRA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lsra.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 LSRA form was accepted"
    );

    return passed;
}

bool test_lsrb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "LSRB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/lsrb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "LSRB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x54U}
                && assembled.output->object.relocations.empty(),
            "LSRB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/lsrb-extra-operand.s",
            ".section .text, code\nLSRB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "LSRB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lsrb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 LSRB form was accepted"
    );
    return passed;
}

bool test_rola_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ROLA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/rola.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ROLA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x49U}
                && assembled.output->object.relocations.empty(),
            "ROLA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/rola-extra-operand.s",
            ".section .text, code\nROLA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ROLA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-rola.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ROLA form was accepted"
    );
    return passed;
}

bool test_rolb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ROLB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/rolb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ROLB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x59U}
                && assembled.output->object.relocations.empty(),
            "ROLB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/rolb-extra-operand.s",
            ".section .text, code\nROLB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ROLB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-rolb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ROLB form was accepted"
    );
    return passed;
}

bool test_rora_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "RORA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/rora.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "RORA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x46U}
                && assembled.output->object.relocations.empty(),
            "RORA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/rora-extra-operand.s",
            ".section .text, code\nRORA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "RORA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-rora.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 RORA form was accepted"
    );

    return passed;
}

bool test_rorb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "RORB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/rorb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "RORB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x56U}
                && assembled.output->object.relocations.empty(),
            "RORB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/rorb-extra-operand.s",
            ".section .text, code\nRORB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "RORB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-rorb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 RORB form was accepted"
    );
    return passed;
}

bool test_asla_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ASLA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/asla.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ASLA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x48U}
                && assembled.output->object.relocations.empty(),
            "ASLA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/asla-extra-operand.s",
            ".section .text, code\nASLA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ASLA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-asla.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ASLA form was accepted"
    );
    return passed;
}

bool test_aslb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ASLB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/aslb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ASLB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x58U}
                && assembled.output->object.relocations.empty(),
            "ASLB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/aslb-extra-operand.s",
            ".section .text, code\nASLB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ASLB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-aslb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ASLB form was accepted"
    );
    return passed;
}

bool test_asra_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ASRA\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/asra.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ASRA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x47U}
                && assembled.output->object.relocations.empty(),
            "ASRA encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/asra-extra-operand.s",
            ".section .text, code\nASRA $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ASRA accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-asra.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ASRA form was accepted"
    );
    return passed;
}

bool test_asrb_instruction() {
    constexpr std::string_view source =
        ".section .text, code\n"
        "ASRB\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/asrb.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ASRB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x57U}
                && assembled.output->object.relocations.empty(),
            "ASRB encoding differs"
        );
    }

    const auto extra_operand = jr800::assembler::assemble(
        Source{
            "src/asrb-extra-operand.s",
            ".section .text, code\nASRB $01\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !extra_operand.succeeded()
            && find_diagnostic(extra_operand, "E3401") != nullptr,
        "ASRB accepted an operand"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-asrb.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ASRB form was accepted"
    );

    return passed;
}

bool test_addd_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDD $0000\n"
        "ADDD $20\n"
        "ADDD $FF\n"
        "ADDD $100\n"
        "ADDD $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/addd-addressing.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "ADDD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD3U, 0x00U,
                    0xD3U, 0x20U,
                    0xD3U, 0xFFU,
                    0xF3U, 0x01U, 0x00U,
                    0xF3U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ADDD direct/extended address selection differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/addd-extended-range.s",
            ".section .text, code\nADDD $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ADDD accepted an address above 16 bits"
    );

    constexpr std::string_view relocated_source =
        ".extern SOURCE\n"
        ".section .text, code\n"
        "ADDD SOURCE\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/addd-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ADDD did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF3U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ADDD encoding differs"
        );
    }

    constexpr std::string_view immediate_source =
        ".section .text, code\n"
        "ADDD #$0000\n"
        "ADDD #$7FFF\n"
        "ADDD #$8000\n"
        "ADDD #$FFFF\n";
    const auto immediate = jr800::assembler::assemble(
        Source{"src/addd-immediate.s", std::string{immediate_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(immediate, "Immediate ADDD did not assemble");
    if (immediate.succeeded()) {
        const auto* text = find_section(immediate.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC3U, 0x00U, 0x00U,
                    0xC3U, 0x7FU, 0xFFU,
                    0xC3U, 0x80U, 0x00U,
                    0xC3U, 0xFFU, 0xFFU,
                }
                && immediate.output->object.relocations.empty(),
            "Immediate ADDD encoding differs"
        );
    }

    const auto immediate_out_of_range = jr800::assembler::assemble(
        Source{
            "src/addd-immediate-range.s",
            ".section .text, code\nADDD #$10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !immediate_out_of_range.succeeded()
            && find_diagnostic(immediate_out_of_range, "E3504") != nullptr,
        "Immediate ADDD accepted an operand above 16 bits"
    );

    constexpr std::string_view relocated_immediate_source =
        ".extern VALUE\n"
        ".section .text, code\n"
        "ADDD #VALUE\n";
    const auto relocated_immediate = jr800::assembler::assemble(
        Source{
            "src/addd-immediate-relocated.s",
            std::string{relocated_immediate_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated_immediate,
        "Relocated immediate ADDD did not assemble"
    );
    if (relocated_immediate.succeeded()) {
        const auto* text = find_section(
            relocated_immediate.output->object,
            ".text"
        );
        const auto& relocations = relocated_immediate.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xC3U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated immediate ADDD encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "ADDD $00,X\n"
        "ADDD $20,x\n"
        "ADDD $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/addd-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed ADDD did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE3U, 0x00U,
                    0xE3U, 0x20U,
                    0xE3U, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed ADDD encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/addd-indexed-range.s",
            ".section .text, code\nADDD 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed ADDD accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/addd-indexed-negative.s",
            ".section .text, code\nADDD -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed ADDD accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "ADDD DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/addd-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed ADDD did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE3U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ADDD encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/addd-indexed-register.s",
            ".section .text, code\nADDD $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ADDD accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-addd.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 ADDD form was accepted"
    );
    const auto unstaged_immediate_profile = jr800::assembler::assemble(
        Source{
            "src/mc6801-addd-immediate.s",
            ".section .text, code\nADDD #$1234\n",
        },
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_immediate_profile.succeeded()
            && find_diagnostic(unstaged_immediate_profile, "E3401")
                != nullptr,
        "Unreviewed MC6801 immediate ADDD form was accepted"
    );
    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-addd-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ADDD form was accepted"
    );
    return passed;
}

bool test_subd_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SUBD $20\n"
        "SUBD $FF\n"
        "SUBD $100\n"
        "SUBD $8123\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/subd-direct.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "SUBD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x93U, 0x20U,
                    0x93U, 0xFFU,
                    0xB3U, 0x01U, 0x00U,
                    0xB3U, 0x81U, 0x23U,
                }
                && assembled.output->object.relocations.empty(),
            "SUBD direct/extended address selection differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/subd-extended-range.s",
            ".section .text, code\nSUBD $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended SUBD accepted an address above 16 bits"
    );

    constexpr std::string_view relocated_source =
        ".extern SOURCE\n"
        ".section .text, code\n"
        "SUBD SOURCE\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/subd-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended SUBD did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB3U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended SUBD encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "SUBD $00,X\n"
        "SUBD $20,x\n"
        "SUBD $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/subd-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed SUBD did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA3U, 0x00U,
                    0xA3U, 0x20U,
                    0xA3U, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed SUBD encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/subd-indexed-range.s",
            ".section .text, code\nSUBD 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed SUBD accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/subd-indexed-negative.s",
            ".section .text, code\nSUBD -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed SUBD accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "SUBD DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/subd-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed SUBD did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA3U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed SUBD encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/subd-indexed-register.s",
            ".section .text, code\nSUBD $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed SUBD accepted a register other than X"
    );

    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-subd-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed SUBD form was accepted"
    );

    constexpr std::string_view immediate_source =
        ".section .text, code\n"
        "SUBD #$0000\n"
        "SUBD #$7FFF\n"
        "SUBD #$8000\n"
        "SUBD #$FFFF\n";
    const auto immediate = jr800::assembler::assemble(
        Source{"src/subd-immediate.s", std::string{immediate_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(immediate, "Immediate SUBD did not assemble");
    if (immediate.succeeded()) {
        const auto* text = find_section(immediate.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x83U, 0x00U, 0x00U,
                    0x83U, 0x7FU, 0xFFU,
                    0x83U, 0x80U, 0x00U,
                    0x83U, 0xFFU, 0xFFU,
                }
                && immediate.output->object.relocations.empty(),
            "Immediate SUBD encoding differs"
        );
    }

    constexpr std::string_view immediate_relocated_source =
        ".extern SUBTRAHEND\n"
        ".section .text, code\n"
        "SUBD #SUBTRAHEND\n";
    const auto immediate_relocated = jr800::assembler::assemble(
        Source{
            "src/subd-immediate-relocated.s",
            std::string{immediate_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        immediate_relocated,
        "Relocated immediate SUBD did not assemble"
    );
    if (immediate_relocated.succeeded()) {
        const auto* text = find_section(
            immediate_relocated.output->object,
            ".text"
        );
        const auto& relocations = immediate_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x83U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated immediate SUBD encoding differs"
        );
    }

    const auto immediate_out_of_range = jr800::assembler::assemble(
        Source{
            "src/subd-immediate-range.s",
            ".section .text, code\nSUBD #$10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !immediate_out_of_range.succeeded()
            && find_diagnostic(immediate_out_of_range, "E3504") != nullptr,
        "Immediate SUBD accepted a value above 65535"
    );

    const auto unstaged_immediate_profile = jr800::assembler::assemble(
        Source{
            "src/mc6801-subd-immediate.s",
            std::string{immediate_source},
        },
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_immediate_profile.succeeded()
            && find_diagnostic(unstaged_immediate_profile, "E3401")
                != nullptr,
        "Unreviewed MC6801 immediate SUBD form was accepted"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-subd-direct.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 direct SUBD form was accepted"
    );
    return passed;
}

bool test_stx_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "STX $80\n"
        "STX $FF\n"
        "STX $100\n"
        "STX $8123\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/stx.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "STX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xDFU, 0x80U,
                    0xDFU, 0xFFU,
                    0xFFU, 0x01U, 0x00U,
                    0xFFU, 0x81U, 0x23U,
                }
                && assembled.output->object.relocations.empty(),
            "STX direct/extended address selection differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern DESTINATION\n"
        ".section .text, code\n"
        "STX DESTINATION\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/stx-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended STX did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xFFU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended STX encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "STX $00,X\n"
        "STX $7F,x\n"
        "STX $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/stx-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed STX did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xEFU, 0x00U,
                    0xEFU, 0x7FU,
                    0xEFU, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed STX encoding differs"
        );
    }

    constexpr std::string_view indexed_relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "STX DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/stx-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed STX did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(indexed_relocated.output->object, ".text");
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xEFU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed STX encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/stx-indexed-range.s",
            ".section .text, code\nSTX 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed STX accepted a displacement above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-stx.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 STX forms were accepted"
    );
    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-stx-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed STX form was accepted"
    );
    return passed;
}

bool test_sts_address_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "STS $00\n"
        "STS $20\n"
        "STS $FF\n"
        "STS $100\n"
        "STS $8123\n"
        "STS $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sts-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed STS did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x9FU, 0x00U,
                    0x9FU, 0x20U,
                    0x9FU, 0xFFU,
                    0xBFU, 0x01U, 0x00U,
                    0xBFU, 0x81U, 0x23U,
                    0xBFU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "STS direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "STS direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/sts-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct STS did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x9FU, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct STS encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern destination\n"
        ".section .text, code\n"
        "STS destination\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/sts-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended STS did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xBFU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended STS encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/sts-extended-range.s",
            ".section .text, code\nSTS $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended STS accepted an address above 16 bits"
    );

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "STS $20,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/sts-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed STS did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xAFU, 0x20U}
                && indexed.output->object.relocations.empty(),
            "Indexed STS encoding differs"
        );
    }

    constexpr std::string_view indexed_relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "STS DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/sts-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed STS did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(indexed_relocated.output->object, ".text");
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xAFU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed STS encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/sts-indexed-range.s",
            ".section .text, code\nSTS 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed STS accepted a displacement above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sts-direct.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed STS forms were accepted"
    );
    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sts-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed STS form was accepted"
    );
    return passed;
}

bool test_std_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "STD $20\n"
        "STD $FF\n"
        "STD $100\n"
        "STD $8123\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/std-direct.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Direct STD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xDDU, 0x20U,
                    0xDDU, 0xFFU,
                    0xFDU, 0x01U, 0x00U,
                    0xFDU, 0x81U, 0x23U,
                }
                && assembled.output->object.relocations.empty(),
            "STD direct/extended address selection differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern DESTINATION\n"
        ".section .text, code\n"
        "STD DESTINATION\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/std-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended STD did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xFDU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended STD encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "STD $00,X\n"
        "STD $20,x\n"
        "STD $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{"src/std-indexed.s", std::string{indexed_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed STD did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xEDU, 0x00U,
                    0xEDU, 0x20U,
                    0xEDU, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed STD encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/std-indexed-range.s",
            ".section .text, code\nSTD 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed STD accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/std-indexed-negative.s",
            ".section .text, code\nSTD -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed STD accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "STD DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/std-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed STD did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xEDU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed STD encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/std-indexed-register.s",
            ".section .text, code\nSTD $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed STD accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-std.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 direct or extended STD form was accepted"
    );

    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-std-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed STD form was accepted"
    );
    return passed;
}

bool test_lds_address_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDS $00\n"
        "LDS $20\n"
        "LDS $FF\n"
        "LDS $100\n"
        "LDS $8123\n"
        "LDS $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/lds-direct.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed LDS did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x9EU, 0x00U,
                    0x9EU, 0x20U,
                    0x9EU, 0xFFU,
                    0xBEU, 0x01U, 0x00U,
                    0xBEU, 0x81U, 0x23U,
                    0xBEU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "LDS direct/extended address selection differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/lds-direct-range.s",
            ".section .text, code\nLDS $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended LDS accepted an address above 16 bits"
    );

    constexpr std::string_view relocated_source =
        ".extern SOURCE\n"
        ".section .text, code\n"
        "LDS SOURCE\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/lds-direct-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated extended LDS did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xBEU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended LDS encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "LDS $20,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{
            "src/lds-indexed.s",
            std::string{indexed_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed LDS did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xAEU, 0x20U}
                && indexed.output->object.relocations.empty(),
            "Indexed LDS encoding differs"
        );
    }

    constexpr std::string_view indexed_relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "LDS DISP,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/lds-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed LDS did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(indexed_relocated.output->object, ".text");
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xAEU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed LDS encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/lds-indexed-range.s",
            ".section .text, code\nLDS 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed LDS accepted a displacement above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lds-direct.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed LDS form was accepted"
    );
    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lds-indexed.s", std::string{indexed_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed LDS form was accepted"
    );
    return passed;
}

bool test_ldx_address_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDX $20\n"
        "LDX $FF\n"
        "LDX $100\n"
        "LDX $8123\n"
        "LDX $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldx-addresses.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "LDX addresses did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xDEU, 0x20U,
                    0xDEU, 0xFFU,
                    0xFEU, 0x01U, 0x00U,
                    0xFEU, 0x81U, 0x23U,
                    0xFEU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "LDX direct/extended address selection differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldx-address-range.s",
            ".section .text, code\nLDX $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended LDX accepted an address above 16 bits"
    );

    constexpr std::string_view relocated_source =
        ".extern SOURCE\n"
        ".section .text, code\n"
        "LDX SOURCE\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldx-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended LDX did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xFEU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended LDX encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldx-addresses.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 LDX address form was accepted"
    );
    return passed;
}

bool test_ldx_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDX $20,X\n"
        "LDX 0,x\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldx-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed LDX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xEEU, 0x20U,
                    0xEEU, 0x00U,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed LDX encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldx-indexed-range.s",
            ".section .text, code\nLDX 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed LDX accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/ldx-indexed-negative.s",
            ".section .text, code\nLDX -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed LDX accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/ldx-indexed-register.s",
            ".section .text, code\nLDX $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed LDX accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "LDX DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldx-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed LDX did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xEEU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed LDX encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldx-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed LDX form was accepted"
    );
    return passed;
}

bool test_ldab_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDAB #$00\n"
        "LDAB #$7F\n"
        "LDAB #$80\n"
        "LDAB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldab-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate LDAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC6U, 0x00U,
                    0xC6U, 0x7FU,
                    0xC6U, 0x80U,
                    0xC6U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate LDAB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern initial_b\n"
        ".section .text, code\n"
        "LDAB #initial_b\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldab-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate LDAB did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC6U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate LDAB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldab-immediate-range.s",
            ".section .text, code\nLDAB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate LDAB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldab-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate LDAB form was accepted"
    );
    return passed;
}

bool test_ldab_address_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDAB $00\n"
        "LDAB $20\n"
        "LDAB $FF\n"
        "LDAB $100\n"
        "LDAB $8123\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldab-addresses.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "LDAB addresses did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD6U, 0x00U,
                    0xD6U, 0x20U,
                    0xD6U, 0xFFU,
                    0xF6U, 0x01U, 0x00U,
                    0xF6U, 0x81U, 0x23U,
                }
                && assembled.output->object.relocations.empty(),
            "LDAB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "LDAB direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/ldab-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct LDAB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD6U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct LDAB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "LDAB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldab-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended LDAB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF6U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended LDAB encoding differs"
        );
    }

    constexpr std::string_view indexed_source =
        ".section .text, code\n"
        "LDAB $00,X\n"
        "LDAB $20,x\n"
        "LDAB $FF,X\n";
    const auto indexed = jr800::assembler::assemble(
        Source{
            "src/ldab-indexed.s",
            std::string{indexed_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(indexed, "Indexed LDAB did not assemble");
    if (indexed.succeeded()) {
        const auto* text = find_section(indexed.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE6U, 0x00U,
                    0xE6U, 0x20U,
                    0xE6U, 0xFFU,
                }
                && indexed.output->object.relocations.empty(),
            "Indexed LDAB encoding differs"
        );
    }

    const auto indexed_out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldab-indexed-range.s",
            ".section .text, code\nLDAB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_out_of_range.succeeded()
            && find_diagnostic(indexed_out_of_range, "E3507") != nullptr,
        "Indexed LDAB accepted a displacement above 255"
    );

    const auto indexed_negative = jr800::assembler::assemble(
        Source{
            "src/ldab-indexed-negative.s",
            ".section .text, code\nLDAB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !indexed_negative.succeeded()
            && find_diagnostic(indexed_negative, "E3507") != nullptr,
        "Indexed LDAB accepted a negative displacement"
    );

    constexpr std::string_view indexed_relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "LDAB offset,X\n";
    const auto indexed_relocated = jr800::assembler::assemble(
        Source{
            "src/ldab-indexed-relocated.s",
            std::string{indexed_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        indexed_relocated,
        "Relocated indexed LDAB did not assemble"
    );
    if (indexed_relocated.succeeded()) {
        const auto* text = find_section(
            indexed_relocated.output->object,
            ".text"
        );
        const auto& relocations = indexed_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE6U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed LDAB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/ldab-indexed-register.s",
            ".section .text, code\nLDAB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed LDAB accepted a register other than X"
    );

    const auto unstaged_indexed_profile = jr800::assembler::assemble(
        Source{
            "src/mc6801-ldab-indexed.s",
            std::string{indexed_source},
        },
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_indexed_profile.succeeded()
            && find_diagnostic(unstaged_indexed_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed LDAB form was accepted"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldab-addresses.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 LDAB forms were accepted"
    );
    return passed;
}

bool test_adda_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDA #$00\n"
        "ADDA #$7F\n"
        "ADDA #$80\n"
        "ADDA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adda-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ADDA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x8BU, 0x00U,
                    0x8BU, 0x7FU,
                    0x8BU, 0x80U,
                    0x8BU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ADDA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern addend\n"
        ".section .text, code\n"
        "ADDA #addend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adda-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate ADDA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x8BU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ADDA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adda-immediate-range.s",
            ".section .text, code\nADDA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ADDA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adda-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ADDA form was accepted"
    );
    return passed;
}

bool test_adda_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDA $00\n"
        "ADDA $20\n"
        "ADDA $FF\n"
        "ADDA $100\n"
        "ADDA $8123\n"
        "ADDA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adda-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed ADDA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x9BU, 0x00U,
                    0x9BU, 0x20U,
                    0x9BU, 0xFFU,
                    0xBBU, 0x01U, 0x00U,
                    0xBBU, 0x81U, 0x23U,
                    0xBBU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ADDA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "ADDA source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/adda-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ADDA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x9BU, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ADDA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ADDA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adda-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ADDA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xBBU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ADDA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adda-extended-range.s",
            ".section .text, code\nADDA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ADDA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adda-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ADDA form was accepted"
    );
    return passed;
}

bool test_adda_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDA $00,X\n"
        "ADDA $20,x\n"
        "ADDA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adda-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ADDA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xABU, 0x00U,
                    0xABU, 0x20U,
                    0xABU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ADDA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adda-indexed-range.s",
            ".section .text, code\nADDA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ADDA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/adda-indexed-negative.s",
            ".section .text, code\nADDA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ADDA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ADDA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adda-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ADDA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xABU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ADDA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/adda-indexed-register.s",
            ".section .text, code\nADDA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ADDA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adda-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ADDA form was accepted"
    );
    return passed;
}

bool test_addb_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDB #$00\n"
        "ADDB #$7F\n"
        "ADDB #$80\n"
        "ADDB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/addb-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ADDB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xCBU, 0x00U,
                    0xCBU, 0x7FU,
                    0xCBU, 0x80U,
                    0xCBU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ADDB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern addend\n"
        ".section .text, code\n"
        "ADDB #addend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/addb-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate ADDB did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xCBU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ADDB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/addb-immediate-range.s",
            ".section .text, code\nADDB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ADDB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-addb-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ADDB form was accepted"
    );
    return passed;
}

bool test_addb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDB $00\n"
        "ADDB $20\n"
        "ADDB $FF\n"
        "ADDB $100\n"
        "ADDB $8123\n"
        "ADDB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/addb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed ADDB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xDBU, 0x00U,
                    0xDBU, 0x20U,
                    0xDBU, 0xFFU,
                    0xFBU, 0x01U, 0x00U,
                    0xFBU, 0x81U, 0x23U,
                    0xFBU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ADDB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "ADDB source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/addb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ADDB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xDBU, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ADDB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ADDB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/addb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ADDB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xFBU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ADDB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/addb-extended-range.s",
            ".section .text, code\nADDB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ADDB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-addb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ADDB form was accepted"
    );
    return passed;
}

bool test_addb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADDB $00,X\n"
        "ADDB $20,x\n"
        "ADDB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/addb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ADDB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xEBU, 0x00U,
                    0xEBU, 0x20U,
                    0xEBU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ADDB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/addb-indexed-range.s",
            ".section .text, code\nADDB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ADDB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/addb-indexed-negative.s",
            ".section .text, code\nADDB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ADDB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ADDB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/addb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ADDB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xEBU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ADDB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/addb-indexed-register.s",
            ".section .text, code\nADDB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ADDB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-addb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ADDB form was accepted"
    );
    return passed;
}

bool test_adca_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADCA #$00\n"
        "ADCA #$7F\n"
        "ADCA #$80\n"
        "ADCA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adca-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ADCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x89U, 0x00U,
                    0x89U, 0x7FU,
                    0x89U, 0x80U,
                    0x89U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ADCA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern addend\n"
        ".section .text, code\n"
        "ADCA #addend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adca-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate ADCA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x89U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ADCA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adca-immediate-range.s",
            ".section .text, code\nADCA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ADCA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adca-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ADCA form was accepted"
    );
    return passed;
}

bool test_adca_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADCA $00\n"
        "ADCA $20\n"
        "ADCA $FF\n"
        "ADCA $100\n"
        "ADCA $8123\n"
        "ADCA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adca-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed ADCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x99U, 0x00U,
                    0x99U, 0x20U,
                    0x99U, 0xFFU,
                    0xB9U, 0x01U, 0x00U,
                    0xB9U, 0x81U, 0x23U,
                    0xB9U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ADCA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "ADCA source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/adca-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ADCA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x99U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ADCA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ADCA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adca-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ADCA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB9U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ADCA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adca-extended-range.s",
            ".section .text, code\nADCA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ADCA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adca-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ADCA form was accepted"
    );
    return passed;
}

bool test_adca_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADCA $00,X\n"
        "ADCA $20,x\n"
        "ADCA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adca-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ADCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA9U, 0x00U,
                    0xA9U, 0x20U,
                    0xA9U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ADCA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adca-indexed-range.s",
            ".section .text, code\nADCA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ADCA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/adca-indexed-negative.s",
            ".section .text, code\nADCA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ADCA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ADCA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adca-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ADCA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA9U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ADCA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/adca-indexed-register.s",
            ".section .text, code\nADCA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ADCA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adca-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ADCA form was accepted"
    );
    return passed;
}

bool test_adcb_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADCB #$00\n"
        "ADCB #$7F\n"
        "ADCB #$80\n"
        "ADCB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adcb-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ADCB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC9U, 0x00U,
                    0xC9U, 0x7FU,
                    0xC9U, 0x80U,
                    0xC9U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ADCB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern addend\n"
        ".section .text, code\n"
        "ADCB #addend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adcb-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate ADCB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC9U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ADCB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adcb-immediate-range.s",
            ".section .text, code\nADCB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ADCB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adcb-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ADCB form was accepted"
    );
    return passed;
}

bool test_adcb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADCB $00\n"
        "ADCB $20\n"
        "ADCB $FF\n"
        "ADCB $100\n"
        "ADCB $8123\n"
        "ADCB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adcb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed ADCB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD9U, 0x00U,
                    0xD9U, 0x20U,
                    0xD9U, 0xFFU,
                    0xF9U, 0x01U, 0x00U,
                    0xF9U, 0x81U, 0x23U,
                    0xF9U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ADCB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "ADCB source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/adcb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ADCB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD9U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ADCB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ADCB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adcb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ADCB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF9U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ADCB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adcb-extended-range.s",
            ".section .text, code\nADCB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ADCB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adcb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ADCB form was accepted"
    );
    return passed;
}

bool test_adcb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ADCB $00,X\n"
        "ADCB $20,x\n"
        "ADCB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/adcb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ADCB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE9U, 0x00U,
                    0xE9U, 0x20U,
                    0xE9U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ADCB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/adcb-indexed-range.s",
            ".section .text, code\nADCB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ADCB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/adcb-indexed-negative.s",
            ".section .text, code\nADCB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ADCB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ADCB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/adcb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ADCB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE9U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ADCB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/adcb-indexed-register.s",
            ".section .text, code\nADCB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ADCB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-adcb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ADCB form was accepted"
    );
    return passed;
}

bool test_anda_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ANDA #$00\n"
        "ANDA #$7F\n"
        "ANDA #$80\n"
        "ANDA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/anda-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ANDA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x84U, 0x00U,
                    0x84U, 0x7FU,
                    0x84U, 0x80U,
                    0x84U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ANDA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "ANDA #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/anda-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate ANDA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x84U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ANDA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/anda-immediate-range.s",
            ".section .text, code\nANDA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ANDA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-anda-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ANDA form was accepted"
    );
    return passed;
}

bool test_anda_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ANDA $00\n"
        "ANDA $20\n"
        "ANDA $FF\n"
        "ANDA $100\n"
        "ANDA $8123\n"
        "ANDA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/anda-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(
        assembled,
        "Addressed ANDA did not assemble"
    );
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x94U, 0x00U,
                    0x94U, 0x20U,
                    0x94U, 0xFFU,
                    0xB4U, 0x01U, 0x00U,
                    0xB4U, 0x81U, 0x23U,
                    0xB4U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ANDA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "ANDA direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/anda-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ANDA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x94U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ANDA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ANDA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/anda-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ANDA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB4U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ANDA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/anda-extended-range.s",
            ".section .text, code\nANDA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ANDA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-anda-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ANDA form was accepted"
    );
    return passed;
}

bool test_anda_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ANDA $00,X\n"
        "ANDA $20,x\n"
        "ANDA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/anda-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ANDA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA4U, 0x00U,
                    0xA4U, 0x20U,
                    0xA4U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ANDA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/anda-indexed-range.s",
            ".section .text, code\nANDA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ANDA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/anda-indexed-negative.s",
            ".section .text, code\nANDA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ANDA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ANDA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/anda-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ANDA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA4U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ANDA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/anda-indexed-register.s",
            ".section .text, code\nANDA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ANDA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-anda-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ANDA form was accepted"
    );
    return passed;
}

bool test_andb_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ANDB #$00\n"
        "ANDB #$7F\n"
        "ANDB #$80\n"
        "ANDB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/andb-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ANDB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC4U, 0x00U,
                    0xC4U, 0x7FU,
                    0xC4U, 0x80U,
                    0xC4U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ANDB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "ANDB #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/andb-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate ANDB did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC4U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ANDB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/andb-immediate-range.s",
            ".section .text, code\nANDB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ANDB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-andb-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ANDB form was accepted"
    );
    return passed;
}

bool test_andb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ANDB $00\n"
        "ANDB $20\n"
        "ANDB $FF\n"
        "ANDB $100\n"
        "ANDB $8123\n"
        "ANDB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/andb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(
        assembled,
        "Addressed ANDB did not assemble"
    );
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD4U, 0x00U,
                    0xD4U, 0x20U,
                    0xD4U, 0xFFU,
                    0xF4U, 0x01U, 0x00U,
                    0xF4U, 0x81U, 0x23U,
                    0xF4U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ANDB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "ANDB direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/andb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ANDB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD4U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ANDB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ANDB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/andb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ANDB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF4U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ANDB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/andb-extended-range.s",
            ".section .text, code\nANDB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ANDB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-andb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ANDB form was accepted"
    );
    return passed;
}

bool test_andb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ANDB $00,X\n"
        "ANDB $20,x\n"
        "ANDB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/andb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ANDB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE4U, 0x00U,
                    0xE4U, 0x20U,
                    0xE4U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ANDB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/andb-indexed-range.s",
            ".section .text, code\nANDB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ANDB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/andb-indexed-negative.s",
            ".section .text, code\nANDB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ANDB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ANDB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/andb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ANDB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE4U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ANDB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/andb-indexed-register.s",
            ".section .text, code\nANDB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ANDB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-andb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ANDB form was accepted"
    );
    return passed;
}

bool test_bita_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "BITA #$00\n"
        "BITA #$7F\n"
        "BITA #$80\n"
        "BITA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/bita-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate BITA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x85U, 0x00U,
                    0x85U, 0x7FU,
                    0x85U, 0x80U,
                    0x85U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate BITA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "BITA #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/bita-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate BITA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x85U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate BITA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/bita-immediate-range.s",
            ".section .text, code\nBITA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate BITA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-bita-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate BITA form was accepted"
    );
    return passed;
}

bool test_bita_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "BITA $00\n"
        "BITA $20\n"
        "BITA $FF\n"
        "BITA $100\n"
        "BITA $8123\n"
        "BITA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/bita-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(
        assembled,
        "Addressed BITA did not assemble"
    );
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x95U, 0x00U,
                    0x95U, 0x20U,
                    0x95U, 0xFFU,
                    0xB5U, 0x01U, 0x00U,
                    0xB5U, 0x81U, 0x23U,
                    0xB5U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "BITA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "BITA direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/bita-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct BITA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x95U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct BITA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "BITA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/bita-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended BITA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB5U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended BITA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/bita-extended-range.s",
            ".section .text, code\nBITA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended BITA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-bita-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed BITA form was accepted"
    );
    return passed;
}

bool test_bita_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "BITA $00,X\n"
        "BITA $20,x\n"
        "BITA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/bita-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed BITA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA5U, 0x00U,
                    0xA5U, 0x20U,
                    0xA5U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed BITA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/bita-indexed-range.s",
            ".section .text, code\nBITA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed BITA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/bita-indexed-negative.s",
            ".section .text, code\nBITA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed BITA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "BITA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/bita-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed BITA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA5U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed BITA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/bita-indexed-register.s",
            ".section .text, code\nBITA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed BITA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-bita-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed BITA form was accepted"
    );
    return passed;
}

bool test_bitb_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "BITB #$00\n"
        "BITB #$7F\n"
        "BITB #$80\n"
        "BITB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/bitb-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate BITB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC5U, 0x00U,
                    0xC5U, 0x7FU,
                    0xC5U, 0x80U,
                    0xC5U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate BITB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "BITB #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/bitb-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate BITB did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC5U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate BITB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/bitb-immediate-range.s",
            ".section .text, code\nBITB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate BITB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-bitb-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate BITB form was accepted"
    );
    return passed;
}

bool test_bitb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "BITB $00\n"
        "BITB $20\n"
        "BITB $FF\n"
        "BITB $100\n"
        "BITB $8123\n"
        "BITB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/bitb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed BITB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD5U, 0x00U,
                    0xD5U, 0x20U,
                    0xD5U, 0xFFU,
                    0xF5U, 0x01U, 0x00U,
                    0xF5U, 0x81U, 0x23U,
                    0xF5U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "BITB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "BITB direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/bitb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct BITB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD5U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct BITB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "BITB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/bitb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended BITB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF5U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended BITB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/bitb-extended-range.s",
            ".section .text, code\nBITB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended BITB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-bitb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed BITB form was accepted"
    );
    return passed;
}

bool test_bitb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "BITB $00,X\n"
        "BITB $20,x\n"
        "BITB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/bitb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed BITB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE5U, 0x00U,
                    0xE5U, 0x20U,
                    0xE5U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed BITB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/bitb-indexed-range.s",
            ".section .text, code\nBITB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed BITB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/bitb-indexed-negative.s",
            ".section .text, code\nBITB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed BITB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "BITB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/bitb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed BITB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE5U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed BITB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/bitb-indexed-register.s",
            ".section .text, code\nBITB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed BITB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-bitb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed BITB form was accepted"
    );
    return passed;
}

bool test_immediate_subtract_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SUBA #$00\n"
        "SUBA #$7F\n"
        "SUBA #$80\n"
        "SUBA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/suba-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate SUBA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x80U, 0x00U,
                    0x80U, 0x7FU,
                    0x80U, 0x80U,
                    0x80U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate SUBA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern subtrahend\n"
        ".section .text, code\n"
        "SUBA #subtrahend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/suba-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate SUBA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x80U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate SUBA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/suba-immediate-range.s",
            ".section .text, code\nSUBA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate SUBA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-suba-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate SUBA form was accepted"
    );

    constexpr std::string_view subb_source =
        ".section .text, code\n"
        "SUBB #$00\n"
        "SUBB #$7F\n"
        "SUBB #$80\n"
        "SUBB #$FF\n";
    const auto subb_assembled = jr800::assembler::assemble(
        Source{"src/subb-immediate.s", std::string{subb_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        subb_assembled,
        "Immediate SUBB did not assemble"
    );
    if (subb_assembled.succeeded()) {
        const auto* text = find_section(subb_assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC0U, 0x00U,
                    0xC0U, 0x7FU,
                    0xC0U, 0x80U,
                    0xC0U, 0xFFU,
                }
                && subb_assembled.output->object.relocations.empty(),
            "Immediate SUBB encoding differs"
        );
    }

    constexpr std::string_view subb_relocated_source =
        ".extern subtrahend\n"
        ".section .text, code\n"
        "SUBB #subtrahend\n";
    const auto subb_relocated = jr800::assembler::assemble(
        Source{
            "src/subb-immediate-relocated.s",
            std::string{subb_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        subb_relocated,
        "Relocated immediate SUBB did not assemble"
    );
    if (subb_relocated.succeeded()) {
        const auto* text = find_section(subb_relocated.output->object, ".text");
        const auto& relocations = subb_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC0U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate SUBB encoding differs"
        );
    }

    const auto subb_out_of_range = jr800::assembler::assemble(
        Source{
            "src/subb-immediate-range.s",
            ".section .text, code\nSUBB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !subb_out_of_range.succeeded()
            && find_diagnostic(subb_out_of_range, "E3502") != nullptr,
        "Immediate SUBB accepted a value above 255"
    );

    const auto subb_unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-subb-immediate.s", std::string{subb_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !subb_unstaged_profile.succeeded()
            && find_diagnostic(subb_unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate SUBB form was accepted"
    );
    return passed;
}

bool test_suba_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SUBA $00\n"
        "SUBA $20\n"
        "SUBA $FF\n"
        "SUBA $100\n"
        "SUBA $8123\n"
        "SUBA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/suba-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed SUBA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x90U, 0x00U,
                    0x90U, 0x20U,
                    0x90U, 0xFFU,
                    0xB0U, 0x01U, 0x00U,
                    0xB0U, 0x81U, 0x23U,
                    0xB0U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "SUBA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ subtrahend, $20\n"
        ".section .text, code\n"
        "SUBA subtrahend\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/suba-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct SUBA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x90U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct SUBA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern subtrahend\n"
        ".section .text, code\n"
        "SUBA subtrahend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/suba-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended SUBA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB0U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended SUBA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/suba-extended-range.s",
            ".section .text, code\nSUBA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended SUBA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-suba-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed SUBA form was accepted"
    );
    return passed;
}

bool test_suba_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SUBA $00,X\n"
        "SUBA $20,x\n"
        "SUBA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/suba-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed SUBA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA0U, 0x00U,
                    0xA0U, 0x20U,
                    0xA0U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed SUBA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/suba-indexed-range.s",
            ".section .text, code\nSUBA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed SUBA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/suba-indexed-negative.s",
            ".section .text, code\nSUBA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed SUBA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "SUBA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/suba-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed SUBA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA0U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed SUBA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/suba-indexed-register.s",
            ".section .text, code\nSUBA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed SUBA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-suba-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed SUBA form was accepted"
    );
    return passed;
}

bool test_subb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SUBB $00\n"
        "SUBB $20\n"
        "SUBB $FF\n"
        "SUBB $100\n"
        "SUBB $8123\n"
        "SUBB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/subb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed SUBB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD0U, 0x00U,
                    0xD0U, 0x20U,
                    0xD0U, 0xFFU,
                    0xF0U, 0x01U, 0x00U,
                    0xF0U, 0x81U, 0x23U,
                    0xF0U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "SUBB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ subtrahend, $20\n"
        ".section .text, code\n"
        "SUBB subtrahend\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/subb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct SUBB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD0U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct SUBB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern subtrahend\n"
        ".section .text, code\n"
        "SUBB subtrahend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/subb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended SUBB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF0U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended SUBB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/subb-extended-range.s",
            ".section .text, code\nSUBB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended SUBB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-subb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed SUBB form was accepted"
    );
    return passed;
}

bool test_subb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SUBB $00,X\n"
        "SUBB $20,x\n"
        "SUBB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/subb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed SUBB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE0U, 0x00U,
                    0xE0U, 0x20U,
                    0xE0U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed SUBB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/subb-indexed-range.s",
            ".section .text, code\nSUBB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed SUBB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/subb-indexed-negative.s",
            ".section .text, code\nSUBB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed SUBB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "SUBB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/subb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed SUBB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE0U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed SUBB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/subb-indexed-register.s",
            ".section .text, code\nSUBB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed SUBB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-subb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed SUBB form was accepted"
    );
    return passed;
}

bool test_cmpa_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CMPA $00,X\n"
        "CMPA $20,x\n"
        "CMPA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cmpa-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed CMPA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA1U, 0x00U,
                    0xA1U, 0x20U,
                    0xA1U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed CMPA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cmpa-indexed-range.s",
            ".section .text, code\nCMPA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed CMPA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/cmpa-indexed-negative.s",
            ".section .text, code\nCMPA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed CMPA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "CMPA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cmpa-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed CMPA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA1U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed CMPA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/cmpa-indexed-register.s",
            ".section .text, code\nCMPA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed CMPA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cmpa-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed CMPA form was accepted"
    );
    return passed;
}

bool test_cmpa_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CMPA #$00\n"
        "CMPA #$7F\n"
        "CMPA #$80\n"
        "CMPA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cmpa-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate CMPA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x81U, 0x00U,
                    0x81U, 0x7FU,
                    0x81U, 0x80U,
                    0x81U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate CMPA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern compare_a\n"
        ".section .text, code\n"
        "CMPA #compare_a\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cmpa-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate CMPA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x81U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate CMPA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cmpa-immediate-range.s",
            ".section .text, code\nCMPA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate CMPA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cmpa-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate CMPA form was accepted"
    );
    return passed;
}

bool test_cmpa_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CMPA $00\n"
        "CMPA $20\n"
        "CMPA $FF\n"
        "CMPA $100\n"
        "CMPA $8123\n"
        "CMPA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cmpa-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed CMPA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x91U, 0x00U,
                    0x91U, 0x20U,
                    0x91U, 0xFFU,
                    0xB1U, 0x01U, 0x00U,
                    0xB1U, 0x81U, 0x23U,
                    0xB1U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "CMPA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "CMPA direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/cmpa-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct CMPA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x91U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct CMPA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "CMPA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cmpa-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended CMPA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB1U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended CMPA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cmpa-extended-range.s",
            ".section .text, code\nCMPA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended CMPA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cmpa-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed CMPA form was accepted"
    );
    return passed;
}

bool test_eora_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "EORA #$00\n"
        "EORA #$7F\n"
        "EORA #$80\n"
        "EORA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/eora-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate EORA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x88U, 0x00U,
                    0x88U, 0x7FU,
                    0x88U, 0x80U,
                    0x88U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate EORA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "EORA #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eora-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate EORA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x88U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate EORA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/eora-immediate-range.s",
            ".section .text, code\nEORA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate EORA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eora-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate EORA form was accepted"
    );
    return passed;
}

bool test_eora_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "EORA $00\n"
        "EORA $20\n"
        "EORA $FF\n"
        "EORA $100\n"
        "EORA $8123\n"
        "EORA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/eora-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed EORA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x98U, 0x00U,
                    0x98U, 0x20U,
                    0x98U, 0xFFU,
                    0xB8U, 0x01U, 0x00U,
                    0xB8U, 0x81U, 0x23U,
                    0xB8U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "EORA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "EORA source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/eora-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct EORA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x98U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct EORA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "EORA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eora-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended EORA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB8U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended EORA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/eora-extended-range.s",
            ".section .text, code\nEORA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended EORA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eora-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed EORA form was accepted"
    );
    return passed;
}

bool test_eora_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "EORA $00,X\n"
        "EORA $20,x\n"
        "EORA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/eora-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed EORA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA8U, 0x00U,
                    0xA8U, 0x20U,
                    0xA8U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed EORA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/eora-indexed-range.s",
            ".section .text, code\nEORA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed EORA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/eora-indexed-negative.s",
            ".section .text, code\nEORA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed EORA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "EORA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eora-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed EORA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA8U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed EORA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/eora-indexed-register.s",
            ".section .text, code\nEORA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed EORA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eora-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed EORA form was accepted"
    );
    return passed;
}

bool test_eorb_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "EORB #$00\n"
        "EORB #$7F\n"
        "EORB #$80\n"
        "EORB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/eorb-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate EORB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC8U, 0x00U,
                    0xC8U, 0x7FU,
                    0xC8U, 0x80U,
                    0xC8U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate EORB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "EORB #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eorb-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate EORB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC8U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate EORB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/eorb-immediate-range.s",
            ".section .text, code\nEORB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate EORB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eorb-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate EORB form was accepted"
    );
    return passed;
}

bool test_eorb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "EORB $00\n"
        "EORB $20\n"
        "EORB $FF\n"
        "EORB $100\n"
        "EORB $8123\n"
        "EORB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/eorb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed EORB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD8U, 0x00U,
                    0xD8U, 0x20U,
                    0xD8U, 0xFFU,
                    0xF8U, 0x01U, 0x00U,
                    0xF8U, 0x81U, 0x23U,
                    0xF8U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "EORB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "EORB source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/eorb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct EORB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD8U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct EORB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "EORB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eorb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended EORB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF8U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended EORB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/eorb-extended-range.s",
            ".section .text, code\nEORB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended EORB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eorb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed EORB form was accepted"
    );
    return passed;
}

bool test_eorb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "EORB $00,X\n"
        "EORB $20,x\n"
        "EORB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/eorb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed EORB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE8U, 0x00U,
                    0xE8U, 0x20U,
                    0xE8U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed EORB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/eorb-indexed-range.s",
            ".section .text, code\nEORB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed EORB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/eorb-indexed-negative.s",
            ".section .text, code\nEORB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed EORB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "EORB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/eorb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed EORB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE8U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed EORB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/eorb-indexed-register.s",
            ".section .text, code\nEORB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed EORB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-eorb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed EORB form was accepted"
    );
    return passed;
}

bool test_oraa_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ORAA #$00\n"
        "ORAA #$7F\n"
        "ORAA #$80\n"
        "ORAA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/oraa-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ORAA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x8AU, 0x00U,
                    0x8AU, 0x7FU,
                    0x8AU, 0x80U,
                    0x8AU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ORAA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "ORAA #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/oraa-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate ORAA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x8AU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ORAA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/oraa-immediate-range.s",
            ".section .text, code\nORAA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ORAA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-oraa-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ORAA form was accepted"
    );
    return passed;
}

bool test_oraa_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ORAA $00\n"
        "ORAA $20\n"
        "ORAA $FF\n"
        "ORAA $100\n"
        "ORAA $8123\n"
        "ORAA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/oraa-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed ORAA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x9AU, 0x00U,
                    0x9AU, 0x20U,
                    0x9AU, 0xFFU,
                    0xBAU, 0x01U, 0x00U,
                    0xBAU, 0x81U, 0x23U,
                    0xBAU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ORAA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "ORAA source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/oraa-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ORAA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x9AU, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ORAA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ORAA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/oraa-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ORAA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xBAU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ORAA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/oraa-extended-range.s",
            ".section .text, code\nORAA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ORAA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-oraa-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ORAA form was accepted"
    );
    return passed;
}

bool test_oraa_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ORAA $00,X\n"
        "ORAA $20,x\n"
        "ORAA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/oraa-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ORAA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xAAU, 0x00U,
                    0xAAU, 0x20U,
                    0xAAU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ORAA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/oraa-indexed-range.s",
            ".section .text, code\nORAA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ORAA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/oraa-indexed-negative.s",
            ".section .text, code\nORAA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ORAA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ORAA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/oraa-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ORAA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xAAU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ORAA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/oraa-indexed-register.s",
            ".section .text, code\nORAA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ORAA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-oraa-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ORAA form was accepted"
    );
    return passed;
}

bool test_orab_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ORAB #$00\n"
        "ORAB #$7F\n"
        "ORAB #$80\n"
        "ORAB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/orab-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate ORAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xCAU, 0x00U,
                    0xCAU, 0x7FU,
                    0xCAU, 0x80U,
                    0xCAU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate ORAB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern mask\n"
        ".section .text, code\n"
        "ORAB #mask\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/orab-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated immediate ORAB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xCAU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate ORAB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/orab-immediate-range.s",
            ".section .text, code\nORAB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate ORAB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-orab-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate ORAB form was accepted"
    );
    return passed;
}

bool test_orab_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ORAB $00\n"
        "ORAB $20\n"
        "ORAB $FF\n"
        "ORAB $100\n"
        "ORAB $8123\n"
        "ORAB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/orab-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed ORAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xDAU, 0x00U,
                    0xDAU, 0x20U,
                    0xDAU, 0xFFU,
                    0xFAU, 0x01U, 0x00U,
                    0xFAU, 0x81U, 0x23U,
                    0xFAU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "ORAB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_byte, $20\n"
        ".section .text, code\n"
        "ORAB source_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/orab-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct ORAB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xDAU, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct ORAB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "ORAB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/orab-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended ORAB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xFAU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended ORAB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/orab-extended-range.s",
            ".section .text, code\nORAB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended ORAB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-orab-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed ORAB form was accepted"
    );
    return passed;
}

bool test_orab_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "ORAB $00,X\n"
        "ORAB $20,x\n"
        "ORAB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/orab-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed ORAB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xEAU, 0x00U,
                    0xEAU, 0x20U,
                    0xEAU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed ORAB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/orab-indexed-range.s",
            ".section .text, code\nORAB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed ORAB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/orab-indexed-negative.s",
            ".section .text, code\nORAB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed ORAB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "ORAB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/orab-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed ORAB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xEAU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed ORAB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/orab-indexed-register.s",
            ".section .text, code\nORAB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed ORAB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-orab-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed ORAB form was accepted"
    );
    return passed;
}

bool test_cmpb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CMPB $00,X\n"
        "CMPB $20,x\n"
        "CMPB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cmpb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed CMPB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE1U, 0x00U,
                    0xE1U, 0x20U,
                    0xE1U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed CMPB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cmpb-indexed-range.s",
            ".section .text, code\nCMPB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed CMPB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/cmpb-indexed-negative.s",
            ".section .text, code\nCMPB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed CMPB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "CMPB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cmpb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed CMPB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE1U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed CMPB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/cmpb-indexed-register.s",
            ".section .text, code\nCMPB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed CMPB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cmpb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed CMPB form was accepted"
    );
    return passed;
}

bool test_cmpb_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CMPB #$00\n"
        "CMPB #$7F\n"
        "CMPB #$80\n"
        "CMPB #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cmpb-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate CMPB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC1U, 0x00U,
                    0xC1U, 0x7FU,
                    0xC1U, 0x80U,
                    0xC1U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate CMPB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern compare_b\n"
        ".section .text, code\n"
        "CMPB #compare_b\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cmpb-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate CMPB did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC1U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate CMPB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cmpb-immediate-range.s",
            ".section .text, code\nCMPB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate CMPB accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cmpb-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate CMPB form was accepted"
    );
    return passed;
}

bool test_cmpb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CMPB $00\n"
        "CMPB $20\n"
        "CMPB $FF\n"
        "CMPB $100\n"
        "CMPB $8123\n"
        "CMPB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cmpb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed CMPB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD1U, 0x00U,
                    0xD1U, 0x20U,
                    0xD1U, 0xFFU,
                    0xF1U, 0x01U, 0x00U,
                    0xF1U, 0x81U, 0x23U,
                    0xF1U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "CMPB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "CMPB direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/cmpb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct CMPB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD1U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct CMPB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "CMPB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cmpb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended CMPB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF1U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended CMPB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cmpb-extended-range.s",
            ".section .text, code\nCMPB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended CMPB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cmpb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed CMPB form was accepted"
    );
    return passed;
}

bool test_sbc_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SBCA #$00\n"
        "SBCA #$7F\n"
        "SBCA #$80\n"
        "SBCA #$FF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sbca-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate SBCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x82U, 0x00U,
                    0x82U, 0x7FU,
                    0x82U, 0x80U,
                    0x82U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate SBCA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern subtrahend\n"
        ".section .text, code\n"
        "SBCA #subtrahend\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/sbca-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate SBCA did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x82U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate SBCA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/sbca-immediate-range.s",
            ".section .text, code\nSBCA #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3502") != nullptr,
        "Immediate SBCA accepted a value above 255"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sbca-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate SBCA form was accepted"
    );

    constexpr std::string_view sbcb_source =
        ".section .text, code\n"
        "SBCB #$00\n"
        "SBCB #$7F\n"
        "SBCB #$80\n"
        "SBCB #$FF\n";
    const auto sbcb_assembled = jr800::assembler::assemble(
        Source{"src/sbcb-immediate.s", std::string{sbcb_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        sbcb_assembled,
        "Immediate SBCB did not assemble"
    );
    if (sbcb_assembled.succeeded()) {
        const auto* text = find_section(sbcb_assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xC2U, 0x00U,
                    0xC2U, 0x7FU,
                    0xC2U, 0x80U,
                    0xC2U, 0xFFU,
                }
                && sbcb_assembled.output->object.relocations.empty(),
            "Immediate SBCB encoding differs"
        );
    }

    constexpr std::string_view sbcb_relocated_source =
        ".extern subtrahend\n"
        ".section .text, code\n"
        "SBCB #subtrahend\n";
    const auto sbcb_relocated = jr800::assembler::assemble(
        Source{
            "src/sbcb-immediate-relocated.s",
            std::string{sbcb_relocated_source},
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        sbcb_relocated,
        "Relocated immediate SBCB did not assemble"
    );
    if (sbcb_relocated.succeeded()) {
        const auto* text = find_section(sbcb_relocated.output->object, ".text");
        const auto& relocations = sbcb_relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xC2U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated immediate SBCB encoding differs"
        );
    }

    const auto sbcb_out_of_range = jr800::assembler::assemble(
        Source{
            "src/sbcb-immediate-range.s",
            ".section .text, code\nSBCB #$100\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !sbcb_out_of_range.succeeded()
            && find_diagnostic(sbcb_out_of_range, "E3502") != nullptr,
        "Immediate SBCB accepted a value above 255"
    );

    const auto sbcb_unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sbcb-immediate.s", std::string{sbcb_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !sbcb_unstaged_profile.succeeded()
            && find_diagnostic(sbcb_unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate SBCB form was accepted"
    );

    return passed;
}

bool test_sbcb_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SBCB $00\n"
        "SBCB $20\n"
        "SBCB $FF\n"
        "SBCB $100\n"
        "SBCB $8123\n"
        "SBCB $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sbcb-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed SBCB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xD2U, 0x00U,
                    0xD2U, 0x20U,
                    0xD2U, 0xFFU,
                    0xF2U, 0x01U, 0x00U,
                    0xF2U, 0x81U, 0x23U,
                    0xF2U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "SBCB direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "SBCB direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/sbcb-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct SBCB did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xD2U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct SBCB encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "SBCB source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/sbcb-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended SBCB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xF2U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended SBCB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/sbcb-extended-range.s",
            ".section .text, code\nSBCB $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended SBCB accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sbcb-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed SBCB form was accepted"
    );
    return passed;
}

bool test_sbcb_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SBCB $00,X\n"
        "SBCB $20,x\n"
        "SBCB $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sbcb-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed SBCB did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xE2U, 0x00U,
                    0xE2U, 0x20U,
                    0xE2U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed SBCB encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/sbcb-indexed-range.s",
            ".section .text, code\nSBCB 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed SBCB accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/sbcb-indexed-negative.s",
            ".section .text, code\nSBCB -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed SBCB accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "SBCB offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/sbcb-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed SBCB did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xE2U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed SBCB encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/sbcb-indexed-register.s",
            ".section .text, code\nSBCB $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed SBCB accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sbcb-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed SBCB form was accepted"
    );
    return passed;
}

bool test_sbca_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SBCA $00\n"
        "SBCA $20\n"
        "SBCA $FF\n"
        "SBCA $100\n"
        "SBCA $8123\n"
        "SBCA $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sbca-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed SBCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0x92U, 0x00U,
                    0x92U, 0x20U,
                    0x92U, 0xFFU,
                    0xB2U, 0x01U, 0x00U,
                    0xB2U, 0x81U, 0x23U,
                    0xB2U, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "SBCA direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ direct_byte, $20\n"
        ".section .text, code\n"
        "SBCA direct_byte\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/sbca-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct SBCA did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0x92U, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct SBCA encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_byte\n"
        ".section .text, code\n"
        "SBCA source_byte\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/sbca-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended SBCA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xB2U, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended SBCA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/sbca-extended-range.s",
            ".section .text, code\nSBCA $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended SBCA accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sbca-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed SBCA form was accepted"
    );
    return passed;
}

bool test_sbca_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "SBCA $00,X\n"
        "SBCA $20,x\n"
        "SBCA $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/sbca-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed SBCA did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xA2U, 0x00U,
                    0xA2U, 0x20U,
                    0xA2U, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed SBCA encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/sbca-indexed-range.s",
            ".section .text, code\nSBCA 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed SBCA accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/sbca-indexed-negative.s",
            ".section .text, code\nSBCA -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed SBCA accepted a negative displacement"
    );

    constexpr std::string_view relocated_source =
        ".extern offset\n"
        ".section .text, code\n"
        "SBCA offset,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/sbca-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed SBCA did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xA2U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed SBCA encoding differs"
        );
    }

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/sbca-indexed-register.s",
            ".section .text, code\nSBCA $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed SBCA accepted a register other than X"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-sbca-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed SBCA form was accepted"
    );
    return passed;
}

bool test_ldd_immediate_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDD #$0000\n"
        "LDD #$7FFF\n"
        "LDD #$8000\n"
        "LDD #$FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldd-immediate.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Immediate LDD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xCCU, 0x00U, 0x00U,
                    0xCCU, 0x7FU, 0xFFU,
                    0xCCU, 0x80U, 0x00U,
                    0xCCU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Immediate LDD encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern initial_d\n"
        ".section .text, code\n"
        "LDD #initial_d\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldd-immediate-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated immediate LDD did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xCCU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated immediate LDD encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldd-immediate-range.s",
            ".section .text, code\nLDD #$10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Immediate LDD accepted a value above 65535"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldd-immediate.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 immediate LDD form was accepted"
    );
    return passed;
}

bool test_ldd_address_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDD $00\n"
        "LDD $20\n"
        "LDD $FF\n"
        "LDD $100\n"
        "LDD $8123\n"
        "LDD $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldd-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed LDD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xDCU, 0x00U,
                    0xDCU, 0x20U,
                    0xDCU, 0xFFU,
                    0xFCU, 0x01U, 0x00U,
                    0xFCU, 0x81U, 0x23U,
                    0xFCU, 0xFFU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "LDD direct/extended address selection differs"
        );
    }

    constexpr std::string_view symbol_source =
        ".equ source_word, $20\n"
        ".section .text, code\n"
        "LDD source_word\n";
    const auto symbol = jr800::assembler::assemble(
        Source{"src/ldd-direct-symbol.s", std::string{symbol_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(symbol, "Symbolic direct LDD did not assemble");
    if (symbol.succeeded()) {
        const auto* text = find_section(symbol.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xDCU, 0x20U}
                && symbol.output->object.relocations.empty(),
            "Symbolic direct LDD encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".extern source_word\n"
        ".section .text, code\n"
        "LDD source_word\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldd-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended LDD did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xFCU, 0x00U, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs16_be,
            "Relocated extended LDD encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldd-extended-range.s",
            ".section .text, code\nLDD $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended LDD accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldd-direct.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed LDD forms were accepted"
    );
    return passed;
}

bool test_ldd_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "LDD $00,X\n"
        "LDD $20,x\n"
        "LDD $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/ldd-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed LDD did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{
                    0xECU, 0x00U,
                    0xECU, 0x20U,
                    0xECU, 0xFFU,
                }
                && assembled.output->object.relocations.empty(),
            "Indexed LDD encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/ldd-indexed-range.s",
            ".section .text, code\nLDD 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed LDD accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/ldd-indexed-negative.s",
            ".section .text, code\nLDD -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed LDD accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/ldd-indexed-register.s",
            ".section .text, code\nLDD $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed LDD accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern SOURCE_DISPLACEMENT\n"
        ".section .text, code\n"
        "LDD SOURCE_DISPLACEMENT,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/ldd-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated indexed LDD did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xECU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed LDD encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-ldd-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed LDD form was accepted"
    );
    return passed;
}

bool test_immediate16_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view absolute_source =
        ".section .text, code\n"
        "LDS #$01FF\n";
    const auto absolute = jr800::assembler::assemble(
        Source{"src/lds-absolute.s", std::string{absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(absolute, "Absolute LDS did not assemble");
    if (absolute.succeeded()) {
        const auto* text = find_section(absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x8EU, 0x01U, 0xFFU}
                && absolute.output->object.relocations.empty(),
            "Absolute LDS encoding differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern initial_sp\n"
        "LDS #initial_sp\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/lds-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(relocated, "Relocated LDS did not assemble");
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x8EU, 0x00U, 0x00U}
                && relocated.output->object.relocations.size() == 1U
                && relocated.output->object.relocations.front().offset == 1U
                && relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated LDS encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-lds.s", std::string{absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 LDS form was accepted"
    );

    constexpr std::string_view ldx_absolute_source =
        ".section .text, code\n"
        "LDX #$1234\n";
    const auto ldx_absolute = jr800::assembler::assemble(
        Source{"src/ldx-absolute.s", std::string{ldx_absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(ldx_absolute, "Absolute LDX did not assemble");
    if (ldx_absolute.succeeded()) {
        const auto* text = find_section(ldx_absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xCEU, 0x12U, 0x34U}
                && ldx_absolute.output->object.relocations.empty(),
            "Absolute LDX encoding differs"
        );
    }

    constexpr std::string_view ldx_relocated_source =
        ".section .text, code\n"
        ".extern initial_x\n"
        "LDX #initial_x\n";
    const auto ldx_relocated = jr800::assembler::assemble(
        Source{"src/ldx-relocated.s", std::string{ldx_relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(ldx_relocated, "Relocated LDX did not assemble");
    if (ldx_relocated.succeeded()) {
        const auto* text = find_section(ldx_relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xCEU, 0x00U, 0x00U}
                && ldx_relocated.output->object.relocations.size() == 1U
                && ldx_relocated.output->object.relocations.front().offset == 1U
                && ldx_relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated LDX encoding differs"
        );
    }

    const auto unstaged_ldx = jr800::assembler::assemble(
        Source{"src/mc6801-ldx.s", std::string{ldx_absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_ldx.succeeded()
            && find_diagnostic(unstaged_ldx, "E3401") != nullptr,
        "Unreviewed MC6801 LDX form was accepted"
    );

    constexpr std::string_view cpx_absolute_source =
        ".section .text, code\n"
        "CPX #$1234\n";
    const auto cpx_absolute = jr800::assembler::assemble(
        Source{"src/cpx-absolute.s", std::string{cpx_absolute_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(cpx_absolute, "Absolute CPX did not assemble");
    if (cpx_absolute.succeeded()) {
        const auto* text = find_section(cpx_absolute.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x8CU, 0x12U, 0x34U}
                && cpx_absolute.output->object.relocations.empty(),
            "Absolute CPX encoding differs"
        );
    }

    constexpr std::string_view cpx_relocated_source =
        ".section .text, code\n"
        ".extern compare_x\n"
        "CPX #compare_x\n";
    const auto cpx_relocated = jr800::assembler::assemble(
        Source{"src/cpx-relocated.s", std::string{cpx_relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(cpx_relocated, "Relocated CPX did not assemble");
    if (cpx_relocated.succeeded()) {
        const auto* text = find_section(cpx_relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0x8CU, 0x00U, 0x00U}
                && cpx_relocated.output->object.relocations.size() == 1U
                && cpx_relocated.output->object.relocations.front().offset == 1U
                && cpx_relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated CPX encoding differs"
        );
    }

    const auto unstaged_cpx_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cpx.s", std::string{cpx_absolute_source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_cpx_profile.succeeded()
            && find_diagnostic(unstaged_cpx_profile, "E3401") != nullptr,
        "Unreviewed MC6801 CPX form was accepted"
    );
    return passed;
}

bool test_cpx_addressed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CPX $00\n"
        "CPX $20\n"
        "CPX $FF\n"
        "CPX $100\n"
        "CPX $8123\n"
        "CPX $FFFF\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cpx-addressed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Addressed CPX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0x9CU, 0x00U,
                        0x9CU, 0x20U,
                        0x9CU, 0xFFU,
                        0xBCU, 0x01U, 0x00U,
                        0xBCU, 0x81U, 0x23U,
                        0xBCU, 0xFFU, 0xFFU,
                    }
                && assembled.output->object.relocations.empty(),
            "CPX direct/extended address selection differs"
        );
    }

    constexpr std::string_view relocated_source =
        ".section .text, code\n"
        ".extern compare_x\n"
        "CPX compare_x\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cpx-extended-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated extended CPX did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{0xBCU, 0x00U, 0x00U}
                && relocated.output->object.relocations.size() == 1U
                && relocated.output->object.relocations.front().offset == 1U
                && relocated.output->object.relocations.front().type
                    == RelocationType::abs16_be,
            "Relocated extended CPX encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cpx-extended-range.s",
            ".section .text, code\nCPX $10000\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3504") != nullptr,
        "Extended CPX accepted an address above 16 bits"
    );

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cpx-addressed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 addressed CPX form was accepted"
    );
    return passed;
}

bool test_cpx_indexed_instruction() {
    using jr800::formats::jro::RelocationType;

    constexpr std::string_view source =
        ".section .text, code\n"
        "CPX $00,X\n"
        "CPX $7F,x\n"
        "CPX $FF,X\n";
    const auto assembled = jr800::assembler::assemble(
        Source{"src/cpx-indexed.s", std::string{source}},
        Options{"hd6301v1", "test-version"}
    );
    bool passed = expect_success(assembled, "Indexed CPX did not assemble");
    if (assembled.succeeded()) {
        const auto* text = find_section(assembled.output->object, ".text");
        passed &= expect(
            text != nullptr
                && text->data
                    == std::vector<std::uint8_t>{
                        0xACU, 0x00U,
                        0xACU, 0x7FU,
                        0xACU, 0xFFU,
                    }
                && assembled.output->object.relocations.empty(),
            "Indexed CPX encoding differs"
        );
    }

    const auto out_of_range = jr800::assembler::assemble(
        Source{
            "src/cpx-indexed-range.s",
            ".section .text, code\nCPX 256,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !out_of_range.succeeded()
            && find_diagnostic(out_of_range, "E3507") != nullptr,
        "Indexed CPX accepted a displacement above 255"
    );

    const auto negative = jr800::assembler::assemble(
        Source{
            "src/cpx-indexed-negative.s",
            ".section .text, code\nCPX -1,X\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !negative.succeeded()
            && find_diagnostic(negative, "E3507") != nullptr,
        "Indexed CPX accepted a negative displacement"
    );

    const auto wrong_index = jr800::assembler::assemble(
        Source{
            "src/cpx-indexed-register.s",
            ".section .text, code\nCPX $20,Y\n",
        },
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(
        !wrong_index.succeeded(),
        "Indexed CPX accepted a register other than X"
    );

    constexpr std::string_view relocated_source =
        ".extern DISP\n"
        ".section .text, code\n"
        "CPX DISP,X\n";
    const auto relocated = jr800::assembler::assemble(
        Source{"src/cpx-indexed-relocated.s", std::string{relocated_source}},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect_success(
        relocated,
        "Relocated indexed CPX did not assemble"
    );
    if (relocated.succeeded()) {
        const auto* text = find_section(relocated.output->object, ".text");
        const auto& relocations = relocated.output->object.relocations;
        passed &= expect(
            text != nullptr
                && text->data == std::vector<std::uint8_t>{0xACU, 0x00U}
                && relocations.size() == 1U
                && relocations.front().offset == 1U
                && relocations.front().type == RelocationType::abs8,
            "Relocated indexed CPX encoding differs"
        );
    }

    const auto unstaged_profile = jr800::assembler::assemble(
        Source{"src/mc6801-cpx-indexed.s", std::string{source}},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(
        !unstaged_profile.succeeded()
            && find_diagnostic(unstaged_profile, "E3401") != nullptr,
        "Unreviewed MC6801 indexed CPX form was accepted"
    );
    return passed;
}

bool test_diagnostics() {
    bool passed = true;
    const auto unavailable = jr800::assembler::assemble(
        Source{"src/mc6801.s", ".section .text, code\nAIM #$F0, $20\n"},
        Options{"mc6801", "test-version"}
    );
    passed &= expect(!unavailable.succeeded(), "MC6801 AIM must be rejected");
    const auto* unavailable_diagnostic = find_diagnostic(unavailable, "E3401");
    passed &= expect(unavailable_diagnostic != nullptr, "Unavailable instruction diagnostic missing");
    if (unavailable_diagnostic != nullptr) {
        passed &= expect(
            unavailable_diagnostic->path == "src/mc6801.s"
                && unavailable_diagnostic->line == 2U
                && unavailable_diagnostic->column == 1U,
            "Unavailable instruction location mismatch"
        );
    }

    const auto unknown_symbol = jr800::assembler::assemble(
        Source{"src/unknown.s", ".section .text, code\nSTAA missing\n"},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(!unknown_symbol.succeeded(), "Implicit external must be rejected");
    passed &= expect(
        find_diagnostic(unknown_symbol, "E3307") != nullptr,
        "Unknown symbol diagnostic missing"
    );

    const auto unresolved = jr800::assembler::assemble(
        Source{"src/unresolved.s", ""},
        Options{"jr800_unresolved", "test-version"}
    );
    passed &= expect(!unresolved.succeeded(), "Unreviewed JR-800 profile must be rejected");
    passed &= expect(
        find_diagnostic(unresolved, "E3002") != nullptr,
        "Unreviewed profile diagnostic missing"
    );

    const auto invalid_character = jr800::assembler::assemble(
        Source{"src/invalid.s", ".section .data, data\n.byte 5 % 1\n"},
        Options{"hd6301v1", "test-version"}
    );
    passed &= expect(!invalid_character.succeeded(), "Invalid percent operator must be rejected");
    const auto* lexer_diagnostic = find_diagnostic(invalid_character, "E1001");
    passed &= expect(lexer_diagnostic != nullptr, "Lexer diagnostic missing");
    if (lexer_diagnostic != nullptr) {
        passed &= expect(
            lexer_diagnostic->line == 2U && lexer_diagnostic->column == 9U,
            "Lexer diagnostic location mismatch"
        );
    }
    return passed;
}

bool test_expression_complexity_limit() {
    const Options options{"hd6301v1", "test-version"};
    bool passed = true;

    std::string unary_source = ".section .data, data\n.byte ";
    unary_source.append(50'000U, '+');
    unary_source += "1\n";
    const auto unary = jr800::assembler::assemble(
        Source{"src/deep-unary.s", std::move(unary_source)},
        options
    );
    passed &= expect(!unary.succeeded(), "Deep unary expression was accepted");
    passed &= expect(
        find_diagnostic(unary, "E2002") != nullptr,
        "Deep unary expression limit diagnostic missing"
    );

    std::string binary_source = ".section .data, data\n.byte 1";
    for (std::size_t index = 0; index < 50'000U; ++index) {
        binary_source += "+1";
    }
    binary_source += '\n';
    const auto binary = jr800::assembler::assemble(
        Source{"src/large-expression.s", std::move(binary_source)},
        options
    );
    passed &= expect(!binary.succeeded(), "Large binary expression was accepted");
    passed &= expect(
        find_diagnostic(binary, "E2002") != nullptr,
        "Large binary expression limit diagnostic missing"
    );

    std::string parenthesized_source = ".section .data, data\n.byte ";
    parenthesized_source.append(50'000U, '(');
    parenthesized_source += '1';
    parenthesized_source.append(50'000U, ')');
    parenthesized_source += '\n';
    const auto parenthesized = jr800::assembler::assemble(
        Source{"src/deep-parentheses.s", std::move(parenthesized_source)},
        options
    );
    passed &= expect(!parenthesized.succeeded(), "Deep parenthesized expression was accepted");
    passed &= expect(
        find_diagnostic(parenthesized, "E2002") != nullptr,
        "Parenthesized expression limit diagnostic missing"
    );
    return passed;
}

}  // namespace

int main() {
    bool passed = true;
    passed &= test_multi_source_objects();
    passed &= test_expressions_and_digest();
    passed &= test_extended_instruction();
    passed &= test_indexed_jump_instruction();
    passed &= test_jsr_instruction();
    passed &= test_tap_instruction();
    passed &= test_tpa_instruction();
    passed &= test_clv_instruction();
    passed &= test_sev_instruction();
    passed &= test_clc_instruction();
    passed &= test_sec_instruction();
    passed &= test_cli_instruction();
    passed &= test_tsx_instruction();
    passed &= test_ins_instruction();
    passed &= test_pula_instruction();
    passed &= test_pulb_instruction();
    passed &= test_des_instruction();
    passed &= test_txs_instruction();
    passed &= test_psha_instruction();
    passed &= test_pshb_instruction();
    passed &= test_pshx_instruction();
    passed &= test_pulx_instruction();
    passed &= test_conditional_branch_instruction();
    passed &= test_or_immediate_memory_instruction();
    passed &= test_and_exclusive_or_immediate_memory_instruction();
    passed &= test_indexed_test_immediate_instruction();
    passed &= test_direct_test_immediate_instruction();
    passed &= test_tst_memory_instruction();
    passed &= test_accumulator_test_instruction();
    passed &= test_load_instruction_address_selection();
    passed &= test_store_instruction_address_selection();
    passed &= test_stab_address_instruction();
    passed &= test_stab_indexed_instruction();
    passed &= test_indexed_store_instruction();
    passed &= test_indexed_load_instruction();
    passed &= test_clear_instruction();
    passed &= test_deca_instruction();
    passed &= test_clrb_instruction();
    passed &= test_decb_instruction();
    passed &= test_inca_instruction();
    passed &= test_incb_instruction();
    passed &= test_inc_memory_instruction();
    passed &= test_neg_memory_instruction();
    passed &= test_com_memory_instruction();
    passed &= test_lsr_memory_instruction();
    passed &= test_ror_memory_instruction();
    passed &= test_asr_memory_instruction();
    passed &= test_asl_memory_instruction();
    passed &= test_rol_memory_instruction();
    passed &= test_dec_memory_instruction();
    passed &= test_inx_instruction();
    passed &= test_dex_instruction();
    passed &= test_tab_instruction();
    passed &= test_tba_instruction();
    passed &= test_aba_instruction();
    passed &= test_cba_instruction();
    passed &= test_sba_instruction();
    passed &= test_lsrd_instruction();
    passed &= test_asld_instruction();
    passed &= test_abx_instruction();
    passed &= test_xgdx_instruction();
    passed &= test_slp_instruction();
    passed &= test_wai_instruction();
    passed &= test_swi_instruction();
    passed &= test_daa_instruction();
    passed &= test_rti_instruction();
    passed &= test_mul_instruction();
    passed &= test_nega_instruction();
    passed &= test_negb_instruction();
    passed &= test_coma_instruction();
    passed &= test_comb_instruction();
    passed &= test_lsra_instruction();
    passed &= test_lsrb_instruction();
    passed &= test_rola_instruction();
    passed &= test_rolb_instruction();
    passed &= test_rora_instruction();
    passed &= test_rorb_instruction();
    passed &= test_asla_instruction();
    passed &= test_aslb_instruction();
    passed &= test_asra_instruction();
    passed &= test_asrb_instruction();
    passed &= test_addd_instruction();
    passed &= test_subd_instruction();
    passed &= test_stx_instruction();
    passed &= test_sts_address_instruction();
    passed &= test_std_instruction();
    passed &= test_lds_address_instruction();
    passed &= test_ldx_address_instruction();
    passed &= test_ldx_indexed_instruction();
    passed &= test_adda_immediate_instruction();
    passed &= test_adda_addressed_instruction();
    passed &= test_adda_indexed_instruction();
    passed &= test_addb_immediate_instruction();
    passed &= test_addb_addressed_instruction();
    passed &= test_addb_indexed_instruction();
    passed &= test_adca_immediate_instruction();
    passed &= test_adca_addressed_instruction();
    passed &= test_adca_indexed_instruction();
    passed &= test_adcb_immediate_instruction();
    passed &= test_adcb_addressed_instruction();
    passed &= test_adcb_indexed_instruction();
    passed &= test_anda_immediate_instruction();
    passed &= test_anda_addressed_instruction();
    passed &= test_anda_indexed_instruction();
    passed &= test_andb_immediate_instruction();
    passed &= test_andb_addressed_instruction();
    passed &= test_andb_indexed_instruction();
    passed &= test_bita_immediate_instruction();
    passed &= test_bita_addressed_instruction();
    passed &= test_bita_indexed_instruction();
    passed &= test_bitb_immediate_instruction();
    passed &= test_bitb_addressed_instruction();
    passed &= test_bitb_indexed_instruction();
    passed &= test_immediate_subtract_instruction();
    passed &= test_suba_addressed_instruction();
    passed &= test_suba_indexed_instruction();
    passed &= test_subb_addressed_instruction();
    passed &= test_subb_indexed_instruction();
    passed &= test_cmpa_immediate_instruction();
    passed &= test_cmpa_addressed_instruction();
    passed &= test_cmpa_indexed_instruction();
    passed &= test_eora_immediate_instruction();
    passed &= test_eora_addressed_instruction();
    passed &= test_eora_indexed_instruction();
    passed &= test_eorb_immediate_instruction();
    passed &= test_eorb_addressed_instruction();
    passed &= test_eorb_indexed_instruction();
    passed &= test_oraa_immediate_instruction();
    passed &= test_oraa_addressed_instruction();
    passed &= test_oraa_indexed_instruction();
    passed &= test_orab_immediate_instruction();
    passed &= test_orab_addressed_instruction();
    passed &= test_orab_indexed_instruction();
    passed &= test_cmpb_immediate_instruction();
    passed &= test_cmpb_addressed_instruction();
    passed &= test_cmpb_indexed_instruction();
    passed &= test_sbc_immediate_instruction();
    passed &= test_sbcb_addressed_instruction();
    passed &= test_sbcb_indexed_instruction();
    passed &= test_sbca_addressed_instruction();
    passed &= test_sbca_indexed_instruction();
    passed &= test_ldab_immediate_instruction();
    passed &= test_ldab_address_instruction();
    passed &= test_ldd_immediate_instruction();
    passed &= test_ldd_address_instruction();
    passed &= test_ldd_indexed_instruction();
    passed &= test_immediate16_instruction();
    passed &= test_cpx_addressed_instruction();
    passed &= test_cpx_indexed_instruction();
    passed &= test_diagnostics();
    passed &= test_expression_complexity_limit();
    return passed ? 0 : 1;
}
