// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/formats/jro.hpp"

namespace {

using jr800::formats::jro::ErrorCode;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

template <typename Callable>
bool expect_error(Callable&& callable, ErrorCode expected, const char* message) {
    try {
        std::forward<Callable>(callable)();
    } catch (const jr800::formats::jro::Error& error) {
        if (error.code() == expected) {
            return true;
        }
        std::cerr << message << ": unexpected error code\n";
        return false;
    }
    std::cerr << message << ": no error\n";
    return false;
}

jr800::formats::Sha256Digest digest(std::uint8_t seed) {
    jr800::formats::Sha256Digest result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(seed + index);
    }
    return result;
}

jr800::formats::jro::ObjectFile make_object() {
    using namespace jr800::formats::jro;

    ObjectFile object;
    object.target_profile = "hd6301v1";
    object.build = BuildIdentity{"jr8as", "0.1.0", digest(0x10)};
    object.source_files = {
        SourceFile{"src/main.s", digest(0x20)},
        SourceFile{"src/lib.s", digest(0x40)},
    };
    object.sections = {
        Section{
            ".text",
            SectionType::program_bits,
            SectionAttributes::allocate | SectionAttributes::execute,
            2,
            std::nullopt,
            10,
            {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x42, 0x97, 0x00},
        },
        Section{
            ".bss",
            SectionType::no_bits,
            SectionAttributes::allocate | SectionAttributes::write,
            16,
            0x2000,
            16,
            {},
        },
    };
    object.symbols = {
        Symbol{"start", SymbolBinding::local, SymbolDefinition::section, 0, 0, 0},
        Symbol{"buffer", SymbolBinding::global, SymbolDefinition::section, 1, 0, 16},
        Symbol{
            "external",
            SymbolBinding::global,
            SymbolDefinition::undefined,
            std::nullopt,
            0,
            0,
        },
        Symbol{
            "constant",
            SymbolBinding::global,
            SymbolDefinition::absolute,
            std::nullopt,
            0x42,
            0,
        },
    };
    object.relocations = {
        Relocation{0, 1, RelocationType::rel8, 0, -1},
        Relocation{0, 2, RelocationType::direct8, 1, 0},
        Relocation{0, 3, RelocationType::abs16_be, 2, 4},
        Relocation{0, 5, RelocationType::abs8, 3, 0},
    };
    object.source_lines = {
        SourceLineMapping{0, 0, 3, 0, 1, 1},
        SourceLineMapping{0, 3, 3, 0, 2, 1},
        SourceLineMapping{1, 0, 16, 1, 4, 0},
    };
    return object;
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3U]);
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xFFU);
}

std::size_t skip_string(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return offset + 4U + read_u32(bytes, offset);
}

std::size_t first_section_type_offset(const std::vector<std::uint8_t>& bytes) {
    std::size_t offset = 12U;
    offset = skip_string(bytes, offset);
    offset = skip_string(bytes, offset);
    offset = skip_string(bytes, offset);
    offset += 32U;
    const auto source_count = read_u32(bytes, offset);
    offset += 20U;
    for (std::uint32_t index = 0; index < source_count; ++index) {
        offset = skip_string(bytes, offset);
        offset += 32U;
    }
    return skip_string(bytes, offset);
}

}  // namespace

int main() {
    using namespace jr800::formats::jro;

    bool passed = true;
    const auto object = make_object();
    const auto bytes = write(object);
    passed &= expect(bytes == write(object), "Repeated serialization is not deterministic");
    passed &= expect(bytes.size() > 12U, "Serialized JRO is unexpectedly short");
    passed &= expect(
        bytes[0] == 0x4A && bytes[1] == 0x52 && bytes[2] == 0x4F && bytes[3] == 0x00,
        "JRO magic mismatch"
    );
    passed &= expect(
        bytes[4] == 0x00 && bytes[5] == 0x01 && bytes[6] == 0x00 && bytes[7] == 0x00,
        "JRO big-endian version encoding mismatch"
    );
    passed &= expect(read(bytes) == object, "JRO semantic round trip failed");

    auto reordered = object;
    std::reverse(reordered.relocations.begin(), reordered.relocations.end());
    std::reverse(reordered.source_lines.begin(), reordered.source_lines.end());
    passed &= expect(
        write(reordered) == bytes,
        "Order-insensitive JRO tables were not serialized canonically"
    );

    auto invalid_object = object;
    invalid_object.sections[0].logical_size += 1U;
    passed &= expect_error(
        [&] { static_cast<void>(write(invalid_object)); },
        ErrorCode::invalid_value,
        "PROGRAM_BITS size validation failed"
    );

    invalid_object = object;
    invalid_object.symbols[2].binding = SymbolBinding::local;
    passed &= expect_error(
        [&] { static_cast<void>(write(invalid_object)); },
        ErrorCode::invalid_value,
        "Undefined-local validation failed"
    );

    invalid_object = object;
    invalid_object.relocations[0].symbol_index = 99U;
    passed &= expect_error(
        [&] { static_cast<void>(write(invalid_object)); },
        ErrorCode::invalid_reference,
        "Relocation symbol-reference validation failed"
    );

    invalid_object = object;
    invalid_object.relocations[1].offset = 1U;
    passed &= expect_error(
        [&] { static_cast<void>(write(invalid_object)); },
        ErrorCode::invalid_value,
        "Overlapping relocation validation failed"
    );

    invalid_object = object;
    invalid_object.source_lines[0].length = 0U;
    passed &= expect_error(
        [&] { static_cast<void>(write(invalid_object)); },
        ErrorCode::invalid_value,
        "Source-line range validation failed"
    );

    for (std::size_t length = 0; length < bytes.size(); ++length) {
        const auto truncated = std::span<const std::uint8_t>{bytes.data(), length};
        if (!expect_error(
                [&] { static_cast<void>(read(truncated)); },
                ErrorCode::truncated,
                "Truncation validation failed"
            )) {
            std::cerr << "Truncation length: " << length << '\n';
            passed = false;
            break;
        }
    }

    auto malformed = bytes;
    malformed[0] = 0U;
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::invalid_magic,
        "Magic validation failed"
    );

    malformed = bytes;
    malformed[5] = 2U;
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::unsupported_version,
        "Version validation failed"
    );

    malformed = bytes;
    malformed[11] = 1U;
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::invalid_encoding,
        "Header-reserved validation failed"
    );

    malformed = bytes;
    malformed[16] = 0xFFU;
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::invalid_encoding,
        "UTF-8 validation failed"
    );

    malformed = bytes;
    malformed.push_back(0U);
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::trailing_data,
        "Trailing-data validation failed"
    );

    malformed = bytes;
    malformed[first_section_type_offset(bytes)] = 0xFFU;
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::invalid_encoding,
        "Section-enum validation failed"
    );

    malformed = bytes;
    const auto relocation_start = bytes.size() - object.source_lines.size() * 24U
        - object.relocations.size() * 20U;
    write_u32(malformed, relocation_start + 12U, 0xFFFFFFFFU);
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::invalid_reference,
        "Malformed relocation reference validation failed"
    );

    malformed = bytes;
    const auto source_line_start = bytes.size() - object.source_lines.size() * 24U;
    write_u32(malformed, source_line_start + 12U, 0xFFFFFFFFU);
    passed &= expect_error(
        [&] { static_cast<void>(read(malformed)); },
        ErrorCode::invalid_reference,
        "Malformed source-file reference validation failed"
    );

    return passed ? 0 : 1;
}
