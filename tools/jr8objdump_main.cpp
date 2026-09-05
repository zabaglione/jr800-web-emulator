// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "jr800/disassembler/disassembler.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/jro.hpp"
#include "jr800/isa/instruction_metadata.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

using jr800::formats::jro::ObjectFile;

struct CliOptions {
    std::filesystem::path input;
    std::optional<std::filesystem::path> debug;
};

void print_usage(std::ostream& stream) {
    stream << "Usage: jr8objdump [--debug <input.j8d>] "
              "<input.jro|input.j8a>\n";
}

std::optional<CliOptions> parse_options(int argc, char* argv[]) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--debug") {
            if (options.debug.has_value()) {
                std::cerr << "jr8objdump: --debug may be specified only once\n";
                return std::nullopt;
            }
            if (
                index + 1 >= argc
                || std::string_view{argv[index + 1]}.starts_with('-')
            ) {
                std::cerr << "jr8objdump: missing value for --debug\n";
                return std::nullopt;
            }
            ++index;
            options.debug = std::filesystem::path{argv[index]};
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8objdump: unknown option: " << argument << '\n';
            return std::nullopt;
        } else if (!options.input.empty()) {
            std::cerr << "jr8objdump: exactly one JRO or JR8APP input is required\n";
            return std::nullopt;
        } else {
            options.input = std::filesystem::path{argument};
        }
    }
    if (options.input.empty()) {
        std::cerr << "jr8objdump: exactly one JRO or JR8APP input is required\n";
        return std::nullopt;
    }
    return options;
}

std::optional<std::vector<std::uint8_t>> read_binary(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8objdump: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

std::string hex_value(std::uint32_t value, int width) {
    std::ostringstream stream;
    stream << '$' << std::uppercase << std::hex << std::setfill('0')
           << std::setw(width) << value;
    return stream.str();
}

std::string quoted_text(std::string_view text) {
    std::ostringstream stream;
    stream << '"';
    for (const auto character : text) {
        const auto byte = static_cast<unsigned char>(character);
        switch (byte) {
        case '"':
            stream << "\\\"";
            break;
        case '\\':
            stream << "\\\\";
            break;
        case '\t':
            stream << "\\t";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        default:
            if (byte < 0x20U || byte >= 0x7FU) {
                stream << "\\x" << std::uppercase << std::hex
                       << std::setfill('0') << std::setw(2)
                       << static_cast<unsigned int>(byte) << std::dec;
            } else {
                stream << character;
            }
            break;
        }
    }
    stream << '"';
    return stream.str();
}

std::string bytes_text(std::span<const std::uint8_t> bytes) {
    std::ostringstream stream;
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        if (index != 0U) {
            stream << ' ';
        }
        stream << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2) << static_cast<unsigned int>(bytes[index]);
    }
    return stream.str();
}

std::string digest_text(const jr800::formats::Sha256Digest& digest) {
    std::ostringstream stream;
    for (const auto byte : digest) {
        stream << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

std::string_view section_type_name(
    jr800::formats::jro::SectionType type
) noexcept {
    using jr800::formats::jro::SectionType;
    switch (type) {
    case SectionType::program_bits:
        return "PROGRAM_BITS";
    case SectionType::no_bits:
        return "NO_BITS";
    }
    return "UNKNOWN";
}

std::string section_attributes(
    jr800::formats::jro::SectionAttributes attributes
) {
    using jr800::formats::jro::SectionAttributes;
    std::string text;
    if (jr800::formats::jro::has_attribute(attributes, SectionAttributes::allocate)) {
        text += 'A';
    }
    if (jr800::formats::jro::has_attribute(attributes, SectionAttributes::write)) {
        text += 'W';
    }
    if (jr800::formats::jro::has_attribute(attributes, SectionAttributes::execute)) {
        text += 'X';
    }
    return text.empty() ? "-" : text;
}

std::string placement_text(const jr800::formats::jro::Section& section) {
    if (!section.fixed_address.has_value()) {
        return "relocatable";
    }
    return "fixed:" + hex_value(*section.fixed_address, 4);
}

std::string_view status_name(jr800::disassembler::Status status) noexcept {
    using jr800::disassembler::Status;
    switch (status) {
    case Status::decoded:
        return "decoded";
    case Status::unknown_opcode:
        return "unknown-opcode";
    case Status::truncated_instruction:
        return "truncated-instruction";
    case Status::end_of_input:
        return "end-of-input";
    }
    return "unknown";
}

std::string_view relocation_type_name(
    jr800::formats::jro::RelocationType type
) noexcept {
    using jr800::formats::jro::RelocationType;
    switch (type) {
    case RelocationType::abs8:
        return "ABS8";
    case RelocationType::abs16_be:
        return "ABS16_BE";
    case RelocationType::rel8:
        return "REL8";
    case RelocationType::direct8:
        return "DIRECT8";
    }
    return "UNKNOWN";
}

std::string_view segment_kind_name(
    jr800::formats::jr8app::SegmentKind kind
) noexcept {
    using jr800::formats::jr8app::SegmentKind;
    switch (kind) {
    case SegmentKind::data:
        return "DATA";
    case SegmentKind::zero_fill:
        return "ZERO_FILL";
    }
    return "UNKNOWN";
}

std::string_view debug_binding_name(
    jr800::formats::jr8dbg::SymbolBinding binding
) noexcept {
    using jr800::formats::jr8dbg::SymbolBinding;
    switch (binding) {
    case SymbolBinding::local:
        return "local";
    case SymbolBinding::global:
        return "global";
    }
    return "unknown";
}

struct DebugAnnotationIndex {
    const jr800::formats::jr8dbg::DebugInfo* debug_info{};
    std::vector<const jr800::formats::jr8dbg::Symbol*> address_symbols;
    std::vector<const jr800::formats::jr8dbg::LineMapping*> line_mappings;
};

DebugAnnotationIndex make_debug_annotation_index(
    const jr800::formats::jr8dbg::DebugInfo& debug_info
) {
    DebugAnnotationIndex index;
    index.debug_info = &debug_info;
    index.address_symbols.reserve(debug_info.symbols.size());
    for (const auto& symbol : debug_info.symbols) {
        if (symbol.kind == jr800::formats::jr8dbg::SymbolKind::address) {
            index.address_symbols.push_back(&symbol);
        }
    }
    std::sort(
        index.address_symbols.begin(),
        index.address_symbols.end(),
        [](const auto* left, const auto* right) {
            return std::tie(
                       left->value,
                       left->name,
                       left->binding,
                       left->source_file_index,
                       left->size
                   )
                < std::tie(
                       right->value,
                       right->name,
                       right->binding,
                       right->source_file_index,
                       right->size
                   );
        }
    );

    index.line_mappings.reserve(debug_info.line_mappings.size());
    for (const auto& mapping : debug_info.line_mappings) {
        index.line_mappings.push_back(&mapping);
    }
    std::sort(
        index.line_mappings.begin(),
        index.line_mappings.end(),
        [](const auto* left, const auto* right) {
            return std::tie(
                       left->address,
                       left->length,
                       left->source_file_index,
                       left->line,
                       left->column
                   )
                < std::tie(
                       right->address,
                       right->length,
                       right->source_file_index,
                       right->line,
                       right->column
                   );
        }
    );
    return index;
}

void append_debug_annotations(
    std::ostream& output,
    const DebugAnnotationIndex& index,
    std::uint32_t row_begin,
    std::uint32_t row_end
) {
    const auto first_symbol = std::lower_bound(
        index.address_symbols.begin(),
        index.address_symbols.end(),
        row_begin,
        [](const auto* symbol, std::uint32_t address) {
            return symbol->value < address;
        }
    );
    auto symbol = first_symbol;
    output << '\t';
    while (
        symbol != index.address_symbols.end()
        && (*symbol)->value < row_end
    ) {
        if (symbol != first_symbol) {
            output << ',';
        }
        output << "@+" << ((*symbol)->value - row_begin) << ':'
               << debug_binding_name((*symbol)->binding) << ':'
               << quoted_text((*symbol)->name);
        ++symbol;
    }
    if (symbol == first_symbol) {
        output << '-';
    }

    const auto first_mapping = std::partition_point(
        index.line_mappings.begin(),
        index.line_mappings.end(),
        [row_begin](const auto* mapping) {
            return static_cast<std::uint32_t>(mapping->address) + mapping->length
                <= row_begin;
        }
    );
    auto mapping = first_mapping;
    output << '\t';
    while (
        mapping != index.line_mappings.end()
        && (*mapping)->address < row_end
    ) {
        if (mapping != first_mapping) {
            output << ',';
        }
        const auto mapping_begin = static_cast<std::uint32_t>(
            (*mapping)->address
        );
        const auto annotation_address = std::max(mapping_begin, row_begin);
        const auto& source = index.debug_info
            ->source_files[(*mapping)->source_file_index];
        output << "@+" << (annotation_address - row_begin) << ':'
               << quoted_text(source.path)
               << ':' << (*mapping)->line << ':' << (*mapping)->column;
        ++mapping;
    }
    if (mapping == first_mapping) {
        output << '-';
    }
}

bool has_relocation(
    const ObjectFile& object,
    std::size_t section_index,
    std::size_t begin,
    std::size_t end
) {
    const auto width = [](jr800::formats::jro::RelocationType type) {
        return type == jr800::formats::jro::RelocationType::abs16_be ? 2U : 1U;
    };
    return std::any_of(
        object.relocations.begin(),
        object.relocations.end(),
        [&](const auto& relocation) {
            const auto relocation_begin = static_cast<std::uint64_t>(
                relocation.offset
            );
            const auto relocation_end = relocation_begin + width(relocation.type);
            return relocation.section_index == section_index
                && relocation_begin < end
                && relocation_end > begin;
        }
    );
}

void append_disassembly(
    std::ostream& output,
    const ObjectFile& object,
    std::size_t section_index,
    jr800::isa::CpuProfile profile
) {
    const auto& section = object.sections[section_index];
    output << "DISASSEMBLY stored-byte-decode\n"
           << "LOCATION\tBYTES\tTEXT\tSTATUS\n";
    std::size_t offset = 0U;
    while (offset < section.data.size()) {
        const auto address = static_cast<std::uint16_t>(
            section.fixed_address.value_or(0U) + offset
        );
        const auto remaining = std::span{section.data}.subspan(offset);
        const auto result = jr800::disassembler::disassemble_one(
            profile,
            address,
            remaining
        );
        const auto consumed = result.consumed_bytes;
        const auto line_bytes = remaining.first(consumed);
        output << hex_value(
                      section.fixed_address.has_value()
                          ? static_cast<std::uint32_t>(address)
                          : static_cast<std::uint32_t>(offset),
                      section.fixed_address.has_value() ? 4 : 8
                  )
               << '\t' << bytes_text(line_bytes) << '\t' << result.text << '\t'
               << status_name(result.status);
        if (has_relocation(object, section_index, offset, offset + consumed)) {
            output << "+relocation";
        }
        output << '\n';
        offset += consumed;
    }
}

void append_contents(
    std::ostream& output,
    const jr800::formats::jro::Section& section
) {
    output << "CONTENTS\nOFFSET\tBYTES\n";
    constexpr std::size_t bytes_per_row = 16U;
    for (std::size_t offset = 0U; offset < section.data.size(); offset += bytes_per_row) {
        const auto count = std::min(bytes_per_row, section.data.size() - offset);
        output << hex_value(static_cast<std::uint32_t>(offset), 8) << '\t'
               << bytes_text(std::span{section.data}.subspan(offset, count)) << '\n';
    }
}

void append_relocations(std::ostream& output, const ObjectFile& object) {
    output << "RELOCATIONS " << object.relocations.size() << '\n'
           << "INDEX\tSECTION\tOFFSET\tTYPE\tSYMBOL\tADDEND\n";
    for (std::size_t index = 0U; index < object.relocations.size(); ++index) {
        const auto& relocation = object.relocations[index];
        output << index << '\t' << relocation.section_index << '\t'
               << hex_value(relocation.offset, 8) << '\t'
               << relocation_type_name(relocation.type) << '\t'
               << quoted_text(object.symbols[relocation.symbol_index].name) << '\t'
               << relocation.addend << '\n';
    }
}

void append_application_disassembly(
    std::ostream& output,
    const jr800::formats::jr8app::Segment& segment,
    jr800::isa::CpuProfile profile,
    std::uint16_t entry_point,
    const DebugAnnotationIndex* annotations
) {
    output << "DISASSEMBLY linear-stored-byte-decode\n"
           << "ADDRESS\tBYTES\tTEXT\tSTATUS";
    if (annotations != nullptr) {
        output << "\tSYMBOLS\tSOURCE";
    }
    output << '\n';
    std::size_t offset = 0U;
    while (offset < segment.data.size()) {
        const auto address = static_cast<std::uint16_t>(segment.address + offset);
        const auto remaining = std::span{segment.data}.subspan(offset);
        const auto result = jr800::disassembler::disassemble_one(
            profile,
            address,
            remaining
        );
        const auto consumed = result.consumed_bytes;
        output << hex_value(address, 4) << '\t'
               << bytes_text(remaining.first(consumed)) << '\t'
               << result.text << '\t' << status_name(result.status);
        if (entry_point == address) {
            output << "+entry";
        } else if (
            entry_point > address
            && static_cast<std::uint32_t>(entry_point)
                < static_cast<std::uint32_t>(address) + consumed
        ) {
            output << "+entry-inside";
        }
        if (annotations != nullptr) {
            const auto row_begin = static_cast<std::uint32_t>(address);
            const auto row_end = row_begin + consumed;
            append_debug_annotations(output, *annotations, row_begin, row_end);
        }
        output << '\n';
        offset += consumed;
    }
}

void render_object(
    std::ostream& output,
    const ObjectFile& object,
    jr800::isa::CpuProfile profile
) {
    output << "JRO 1.0 target=" << object.target_profile << '\n';
    for (std::size_t index = 0U; index < object.sections.size(); ++index) {
        const auto& section = object.sections[index];
        output << "SECTION " << index << " name=" << quoted_text(section.name)
               << " type=" << section_type_name(section.type)
               << " attributes=" << section_attributes(section.attributes)
               << " placement=" << placement_text(section)
               << " alignment=" << section.alignment
               << " size=" << hex_value(section.logical_size, 8) << '\n';
        if (section.type == jr800::formats::jro::SectionType::program_bits) {
            if (jr800::formats::jro::has_attribute(
                    section.attributes,
                    jr800::formats::jro::SectionAttributes::execute
                )) {
                append_disassembly(output, object, index, profile);
            } else {
                append_contents(output, section);
            }
        }
    }
    append_relocations(output, object);
}

void render_application(
    std::ostream& output,
    const jr800::formats::jr8app::Application& application,
    jr800::isa::CpuProfile profile,
    const jr800::formats::jr8dbg::DebugInfo* debug_info
) {
    std::optional<DebugAnnotationIndex> annotations;
    if (debug_info != nullptr) {
        annotations = make_debug_annotation_index(*debug_info);
    }
    output << "JR8APP " << jr800::formats::jr8app::format_major_version
           << '.' << jr800::formats::jr8app::format_minor_version
           << " target=" << application.target_profile
           << " kind=" << jr800::formats::jr8app::program_kind_name(application.kind);
    if (application.kind == jr800::formats::jr8app::ProgramKind::machine_code) {
        output << " entry=" << hex_value(application.entry_point, 4);
    }
    output << " integrity=" << digest_text(application.integrity_sha256) << '\n';
    if (application.kind != jr800::formats::jr8app::ProgramKind::machine_code) {
        output << "BASIC size=" << application.basic_data.size() << '\n';
        return;
    }
    if (debug_info != nullptr) {
        output << "DEBUG JR8DBG 1.0 matched sources="
               << debug_info->source_files.size()
               << " symbols=" << debug_info->symbols.size()
               << " lines=" << debug_info->line_mappings.size() << '\n';
    }
    for (std::size_t index = 0U; index < application.segments.size(); ++index) {
        const auto& segment = application.segments[index];
        const auto segment_end = static_cast<std::uint32_t>(segment.address)
            + segment.logical_size;
        const auto contains_entry = application.entry_point >= segment.address
            && application.entry_point < segment_end;
        output << "SEGMENT " << index
               << " kind=" << segment_kind_name(segment.kind)
               << " address=" << hex_value(segment.address, 4)
               << " size=" << hex_value(segment.logical_size, 8)
               << " entry-offset=";
        if (contains_entry) {
            output << hex_value(application.entry_point - segment.address, 8);
        } else {
            output << '-';
        }
        output << '\n';
        if (segment.kind == jr800::formats::jr8app::SegmentKind::data) {
            append_application_disassembly(
                output,
                segment,
                profile,
                application.entry_point,
                annotations.has_value() ? &*annotations : nullptr
            );
        }
    }
}

bool profile_has_instructions(jr800::isa::CpuProfile profile) {
    const auto instructions = jr800::isa::all_instructions();
    return std::any_of(
        instructions.begin(),
        instructions.end(),
        [profile](const auto& instruction) {
            return jr800::isa::instruction_applies_to(instruction, profile);
        }
    );
}

template<std::size_t Size>
bool starts_with(
    std::span<const std::uint8_t> bytes,
    const std::array<std::uint8_t, Size>& magic
) noexcept {
    return bytes.size() >= magic.size()
        && std::equal(magic.begin(), magic.end(), bytes.begin());
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "jr8objdump " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }

    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        print_usage(std::cerr);
        return 2;
    }
    const auto bytes = read_binary(options->input);
    if (!bytes.has_value()) {
        return 2;
    }

    constexpr std::array<std::uint8_t, 4U> jro_magic{'J', 'R', 'O', 0U};
    constexpr std::array<std::uint8_t, 8U> application_magic{
        'J', 'R', '8', 'A', 'P', 'P', 0U, 0U,
    };
    if (starts_with(*bytes, jro_magic)) {
        if (options->debug.has_value()) {
            std::cerr << "jr8objdump: --debug is only valid with JR8APP input\n";
            return 2;
        }
        try {
            const auto object = jr800::formats::jro::read(*bytes);
            const auto profile = jr800::isa::find_profile(object.target_profile);
            if (!profile.has_value() || !profile_has_instructions(*profile)) {
                std::cerr << "jr8objdump: unsupported target profile: "
                          << object.target_profile << '\n';
                return 1;
            }
            render_object(std::cout, object, *profile);
        } catch (const jr800::formats::jro::Error& error) {
            std::cerr << "jr8objdump: invalid JRO: " << error.what();
            if (error.byte_offset().has_value()) {
                std::cerr << " at byte " << *error.byte_offset();
            }
            std::cerr << '\n';
            return 1;
        }
        return 0;
    }
    if (starts_with(*bytes, application_magic)) {
        try {
            const auto application = jr800::formats::jr8app::read(*bytes);
            const auto profile = jr800::isa::find_profile(
                application.target_profile
            );
            if (!profile.has_value() || !profile_has_instructions(*profile)) {
                std::cerr << "jr8objdump: unsupported target profile: "
                          << application.target_profile << '\n';
                return 1;
            }
            std::optional<jr800::formats::jr8dbg::DebugInfo> debug_info;
            if (options->debug.has_value()) {
                if (application.kind != jr800::formats::jr8app::ProgramKind::machine_code) {
                    std::cerr << "jr8objdump: --debug requires a machine-code JR8APP\n";
                    return 1;
                }
                const auto debug_bytes = read_binary(*options->debug);
                if (!debug_bytes.has_value()) {
                    return 2;
                }
                try {
                    debug_info = jr800::formats::jr8dbg::read(*debug_bytes);
                } catch (const jr800::formats::linked::Error& error) {
                    std::cerr << "jr8objdump: invalid JR8DBG: " << error.what();
                    if (error.byte_offset().has_value()) {
                        std::cerr << " at byte " << *error.byte_offset();
                    }
                    std::cerr << '\n';
                    return 1;
                }
                if (debug_info->target_profile != application.target_profile) {
                    std::cerr << "jr8objdump: JR8DBG target profile does not match "
                                 "JR8APP\n";
                    return 1;
                }
                if (
                    debug_info->application_integrity_sha256
                    != application.integrity_sha256
                ) {
                    std::cerr << "jr8objdump: JR8DBG application integrity does not "
                                 "match JR8APP\n";
                    return 1;
                }
            }
            render_application(
                std::cout,
                application,
                *profile,
                debug_info.has_value() ? &*debug_info : nullptr
            );
        } catch (const jr800::formats::linked::Error& error) {
            std::cerr << "jr8objdump: invalid JR8APP: " << error.what();
            if (error.byte_offset().has_value()) {
                std::cerr << " at byte " << *error.byte_offset();
            }
            std::cerr << '\n';
            return 1;
        }
        return 0;
    }
    std::cerr << "jr8objdump: unsupported input format\n";
    return 1;
}
