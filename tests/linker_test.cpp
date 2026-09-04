// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/assembler/assembler.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/jro.hpp"
#include "jr800/linker/linker.hpp"

namespace {

using jr800::assembler::Options;
using jr800::assembler::Source;
using jr800::linker::Diagnostic;
using jr800::linker::InputObject;
using jr800::linker::LinkScript;
using jr800::linker::Result;

constexpr std::string_view kMainSource =
    ".section .text, code\n"
    ".global entry\n"
    ".global buffer\n"
    ".extern helper\n"
    ".local done\n"
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

constexpr std::string_view kLibrarySource =
    ".section .text, code\n"
    ".global helper\n"
    ".extern buffer\n"
    "helper:\n"
    "    AIM #$F0, buffer\n"
    "    RTS\n";

constexpr std::string_view kLinkScript =
    "; vertical slice placement\n"
    "target hd6301v1\n"
    "entry entry\n"
    "region ZP $0000 $0100\n"
    "region CODE $0200 $0100\n"
    "region DATA $2000 $0100\n"
    "place .text CODE\n"
    "place .data DATA\n"
    "place .bss ZP\n";

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

const Diagnostic* find_diagnostic(const Result& result, std::string_view code) {
    const auto found = std::find_if(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [&](const Diagnostic& diagnostic) { return diagnostic.code == code; }
    );
    return found == result.diagnostics.end() ? nullptr : &*found;
}

std::vector<InputObject> assemble_inputs() {
    const Options options{"hd6301v1", "test-version"};
    const auto main_result = jr800::assembler::assemble(
        Source{"src/main.s", std::string{kMainSource}},
        options
    );
    const auto library_result = jr800::assembler::assemble(
        Source{"src/lib.s", std::string{kLibrarySource}},
        options
    );
    if (!main_result.succeeded() || !library_result.succeeded()) {
        return {};
    }
    return {
        InputObject{"obj/main.jro", main_result.output->object},
        InputObject{"obj/lib.jro", library_result.output->object},
    };
}

LinkScript parse_valid_script() {
    const auto result = jr800::linker::parse_script(
        jr800::linker::ScriptSource{"memory.j8l", std::string{kLinkScript}}
    );
    return result.script.value_or(LinkScript{});
}

bool test_vertical_slice() {
    const auto inputs = assemble_inputs();
    bool passed = expect(inputs.size() == 2U, "Test inputs did not assemble");
    const auto script_result = jr800::linker::parse_script(
        jr800::linker::ScriptSource{"memory.j8l", std::string{kLinkScript}}
    );
    passed &= expect(script_result.succeeded(), "Valid link script was rejected");
    if (inputs.size() != 2U || !script_result.succeeded()) {
        return false;
    }

    const auto result = jr800::linker::link_objects(
        inputs,
        *script_result.script,
        jr800::linker::Options{"test-version"}
    );
    passed &= expect(result.succeeded(), "Multi-object link failed");
    if (!result.succeeded()) {
        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.path << ' ' << diagnostic.code << ' '
                      << diagnostic.message << '\n';
        }
        return false;
    }

    const auto& output = *result.output;
    const auto& application = output.application;
    passed &= expect(application.entry_point == 0x0200U, "Entry address mismatch");
    passed &= expect(application.segments.size() == 3U, "Application segment count mismatch");
    if (application.segments.size() == 3U) {
        passed &= expect(
            application.segments[0].kind == jr800::formats::jr8app::SegmentKind::zero_fill
                && application.segments[0].address == 0x0000U
                && application.segments[0].logical_size == 1U,
            "Zero-page segment mismatch"
        );
        passed &= expect(
            application.segments[1].address == 0x0200U
                && application.segments[1].data == std::vector<std::uint8_t>{
                    0x86, 0x2A, 0xB7, 0x00, 0x00,
                    0x8D, 0x03, 0x20, 0x00, 0x01,
                },
            "Main linked bytes mismatch"
        );
        passed &= expect(
            application.segments[2].address == 0x020AU
                && application.segments[2].data
                    == std::vector<std::uint8_t>{0x71, 0xF0, 0x00, 0x39},
            "Library linked bytes mismatch"
        );
    }
    const auto application_bytes = jr800::formats::jr8app::write(application);
    passed &= expect(
        jr800::formats::jr8app::read(application_bytes) == application,
        "Linked JR8APP round trip failed"
    );

    const auto debug_bytes = jr800::formats::jr8dbg::write(output.debug_info);
    passed &= expect(
        jr800::formats::jr8dbg::read(debug_bytes) == output.debug_info,
        "Linked JR8DBG round trip failed"
    );
    passed &= expect(
        output.debug_info.application_integrity_sha256 == application.integrity_sha256,
        "Debug/application integrity binding mismatch"
    );
    const auto* main_line = jr800::formats::jr8dbg::find_line(output.debug_info, 0x0200);
    const auto* library_line = jr800::formats::jr8dbg::find_line(output.debug_info, 0x020A);
    passed &= expect(
        main_line != nullptr && main_line->source_file_index == 0U && main_line->line == 7U,
        "Main source lookup mismatch"
    );
    passed &= expect(
        library_line != nullptr && library_line->source_file_index == 1U
            && library_line->line == 5U,
        "Library source lookup mismatch"
    );
    const auto entry_symbols = jr800::formats::jr8dbg::find_symbols(
        output.debug_info,
        0x0200
    );
    passed &= expect(
        std::any_of(entry_symbols.begin(), entry_symbols.end(), [](const auto* symbol) {
            return symbol->name == "entry";
        }),
        "Entry symbol lookup failed"
    );
    passed &= expect(
        output.link_map.find("Entry: entry = $0200") != std::string::npos
            && output.link_map.find("obj/lib.jro:.text") != std::string::npos,
        "Link map content mismatch"
    );
    passed &= expect(
        output.symbol_output.find("helper") != std::string::npos
            && output.symbol_output.find("src/lib.s") != std::string::npos,
        "Symbol output content mismatch"
    );

    const auto repeated = jr800::linker::link_objects(
        inputs,
        *script_result.script,
        jr800::linker::Options{"test-version"}
    );
    passed &= expect(repeated.succeeded(), "Repeated link failed");
    if (repeated.succeeded()) {
        passed &= expect(
            jr800::formats::jr8app::write(repeated.output->application)
                == application_bytes,
            "JR8APP link output is not deterministic"
        );
        passed &= expect(
            jr800::formats::jr8dbg::write(repeated.output->debug_info) == debug_bytes,
            "JR8DBG link output is not deterministic"
        );
        passed &= expect(
            repeated.output->link_map == output.link_map
                && repeated.output->symbol_output == output.symbol_output,
            "Text link output is not deterministic"
        );
    }
    return passed;
}

bool test_beq_relative_boundaries() {
    const auto script = parse_valid_script();
    bool passed = expect(
        !script.target_profile.empty(),
        "BEQ boundary link script did not parse"
    );
    if (script.target_profile.empty()) {
        return false;
    }

    const auto assemble_and_link = [&](
        std::string logical_path,
        std::string source
    ) -> std::optional<Result> {
        const auto assembled = jr800::assembler::assemble(
            Source{logical_path, std::move(source)},
            Options{"hd6301v1", "test-version"}
        );
        if (!assembled.succeeded()) {
            std::cerr << logical_path << " did not assemble\n";
            return std::nullopt;
        }
        return jr800::linker::link_objects(
            {InputObject{
                "obj/" + logical_path + ".jro",
                assembled.output->object,
            }},
            script,
            jr800::linker::Options{"test-version"}
        );
    };

    const auto zero = assemble_and_link(
        "beq-zero.s",
        ".section .text, code\n"
        ".global entry\n"
        "entry:\n"
        "    BEQ target\n"
        "target:\n"
        "    NOP\n"
    );
    passed &= expect(
        zero.has_value() && zero->succeeded()
            && zero->output->application.segments.size() == 1U
            && zero->output->application.segments.front().data
                == std::vector<std::uint8_t>{0x27U, 0x00U, 0x01U},
        "BEQ zero displacement link differs"
    );

    const auto backward_two = assemble_and_link(
        "beq-backward-two.s",
        ".section .text, code\n"
        ".global entry\n"
        "entry:\n"
        "    BEQ entry\n"
    );
    passed &= expect(
        backward_two.has_value() && backward_two->succeeded()
            && backward_two->output->application.segments.size() == 1U
            && backward_two->output->application.segments.front().data
                == std::vector<std::uint8_t>{0x27U, 0xFEU},
        "BEQ negative-two displacement link differs"
    );

    const auto positive_limit = assemble_and_link(
        "beq-positive-limit.s",
        ".section .text, code\n"
        ".global entry\n"
        "entry:\n"
        "    BEQ target\n"
        "    .space 127\n"
        "target:\n"
        "    NOP\n"
    );
    passed &= expect(
        positive_limit.has_value() && positive_limit->succeeded()
            && positive_limit->output->application.segments.size() == 1U
            && positive_limit->output->application.segments.front().data.size()
                == 130U
            && positive_limit->output->application.segments.front().data[0U]
                == 0x27U
            && positive_limit->output->application.segments.front().data[1U]
                == 0x7FU,
        "BEQ positive displacement limit link differs"
    );

    const auto negative_limit = assemble_and_link(
        "beq-negative-limit.s",
        ".section .text, code\n"
        ".global entry\n"
        "entry:\n"
        "target:\n"
        "    .space 126\n"
        "    BEQ target\n"
    );
    passed &= expect(
        negative_limit.has_value() && negative_limit->succeeded()
            && negative_limit->output->application.segments.size() == 1U
            && negative_limit->output->application.segments.front().data.size()
                == 128U
            && negative_limit->output->application.segments.front().data[126U]
                == 0x27U
            && negative_limit->output->application.segments.front().data[127U]
                == 0x80U,
        "BEQ negative displacement limit link differs"
    );

    const auto positive_overflow = assemble_and_link(
        "beq-positive-overflow.s",
        ".section .text, code\n"
        ".global entry\n"
        "entry:\n"
        "    BEQ target\n"
        "    .space 128\n"
        "target:\n"
        "    NOP\n"
    );
    passed &= expect(
        positive_overflow.has_value() && !positive_overflow->succeeded()
            && find_diagnostic(*positive_overflow, "L2302") != nullptr,
        "BEQ accepted a positive displacement above 127"
    );
    if (positive_overflow.has_value()) {
        const auto* diagnostic = find_diagnostic(*positive_overflow, "L2302");
        passed &= expect(
            diagnostic != nullptr
                && diagnostic->message.find("REL8") != std::string::npos,
            "BEQ positive overflow did not identify REL8"
        );
    }

    const auto negative_overflow = assemble_and_link(
        "beq-negative-overflow.s",
        ".section .text, code\n"
        ".global entry\n"
        "entry:\n"
        "target:\n"
        "    .space 127\n"
        "    BEQ target\n"
    );
    passed &= expect(
        negative_overflow.has_value() && !negative_overflow->succeeded()
            && find_diagnostic(*negative_overflow, "L2302") != nullptr,
        "BEQ accepted a negative displacement below -128"
    );
    if (negative_overflow.has_value()) {
        const auto* diagnostic = find_diagnostic(*negative_overflow, "L2302");
        passed &= expect(
            diagnostic != nullptr
                && diagnostic->message.find("REL8") != std::string::npos,
            "BEQ negative overflow did not identify REL8"
        );
    }
    return passed;
}

bool test_diagnostics() {
    bool passed = true;
    const auto bad_script = jr800::linker::parse_script(jr800::linker::ScriptSource{
        "bad.j8l",
        "target hd6301v1\n"
        "entry entry\n"
        "region A $0000 $0100\n"
        "region B $0080 $0100\n"
        "place .text MISSING\n",
    });
    passed &= expect(!bad_script.succeeded(), "Invalid link script was accepted");
    passed &= expect(
        std::any_of(
            bad_script.diagnostics.begin(),
            bad_script.diagnostics.end(),
            [](const Diagnostic& diagnostic) { return diagnostic.code == "L1018"; }
        ),
        "Region overlap diagnostic missing"
    );
    passed &= expect(
        std::any_of(
            bad_script.diagnostics.begin(),
            bad_script.diagnostics.end(),
            [](const Diagnostic& diagnostic) { return diagnostic.code == "L1019"; }
        ),
        "Unknown placement region diagnostic missing"
    );

    const auto inputs = assemble_inputs();
    const auto script = parse_valid_script();
    if (inputs.size() != 2U || script.target_profile.empty()) {
        return false;
    }

    const auto undefined = jr800::linker::link_objects(
        {inputs.front()},
        script,
        jr800::linker::Options{"test-version"}
    );
    passed &= expect(!undefined.succeeded(), "Undefined symbol link was accepted");
    passed &= expect(
        find_diagnostic(undefined, "L2301") != nullptr,
        "Undefined symbol diagnostic missing"
    );

    auto duplicate_inputs = inputs;
    duplicate_inputs.push_back(InputObject{"obj/lib-copy.jro", inputs[1].object});
    const auto duplicate = jr800::linker::link_objects(
        duplicate_inputs,
        script,
        jr800::linker::Options{"test-version"}
    );
    passed &= expect(!duplicate.succeeded(), "Duplicate global link was accepted");
    passed &= expect(
        find_diagnostic(duplicate, "L2202") != nullptr,
        "Duplicate global diagnostic missing"
    );

    auto overflow_script = script;
    for (auto& placement : overflow_script.placements) {
        if (placement.section_name == ".bss") {
            placement.region_name = "DATA";
        }
    }
    const auto overflow = jr800::linker::link_objects(
        inputs,
        overflow_script,
        jr800::linker::Options{"test-version"}
    );
    passed &= expect(!overflow.succeeded(), "DIRECT8 overflow link was accepted");
    const auto* overflow_diagnostic = find_diagnostic(overflow, "L2302");
    passed &= expect(overflow_diagnostic != nullptr, "Relocation overflow diagnostic missing");
    if (overflow_diagnostic != nullptr) {
        passed &= expect(
            overflow_diagnostic->message.find("DIRECT8") != std::string::npos,
            "Relocation overflow kind missing"
        );
    }
    return passed;
}

}  // namespace

int main() {
    bool passed = true;
    passed &= test_vertical_slice();
    passed &= test_beq_relative_boundaries();
    passed &= test_diagnostics();
    return passed ? 0 : 1;
}
