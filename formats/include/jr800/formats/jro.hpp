// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "jr800/formats/sha256.hpp"

namespace jr800::formats::jro {

inline constexpr std::uint16_t format_major_version = 1;
inline constexpr std::uint16_t format_minor_version = 0;

struct BuildIdentity {
    std::string producer;
    std::string producer_version;
    Sha256Digest build_id{};

    bool operator==(const BuildIdentity&) const = default;
};

struct SourceFile {
    std::string path;
    Sha256Digest content_sha256{};

    bool operator==(const SourceFile&) const = default;
};

enum class SectionType : std::uint8_t {
    program_bits = 1,
    no_bits = 2,
};

enum class SectionAttributes : std::uint8_t {
    none = 0,
    allocate = 1U << 0U,
    write = 1U << 1U,
    execute = 1U << 2U,
};

[[nodiscard]] constexpr SectionAttributes operator|(
    SectionAttributes left,
    SectionAttributes right
) noexcept {
    return static_cast<SectionAttributes>(
        static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right)
    );
}

[[nodiscard]] constexpr bool has_attribute(
    SectionAttributes attributes,
    SectionAttributes expected
) noexcept {
    return (
        static_cast<std::uint8_t>(attributes) & static_cast<std::uint8_t>(expected)
    ) == static_cast<std::uint8_t>(expected);
}

struct Section {
    std::string name;
    SectionType type{SectionType::program_bits};
    SectionAttributes attributes{SectionAttributes::none};
    std::uint32_t alignment{1};
    std::optional<std::uint16_t> fixed_address;
    std::uint32_t logical_size{};
    std::vector<std::uint8_t> data;

    bool operator==(const Section&) const = default;
};

enum class SymbolBinding : std::uint8_t {
    local = 1,
    global = 2,
};

enum class SymbolDefinition : std::uint8_t {
    section = 1,
    absolute = 2,
    undefined = 3,
};

struct Symbol {
    std::string name;
    SymbolBinding binding{SymbolBinding::local};
    SymbolDefinition definition{SymbolDefinition::undefined};
    std::optional<std::uint32_t> section_index;
    std::uint32_t value{};
    std::uint32_t size{};

    bool operator==(const Symbol&) const = default;
};

enum class RelocationType : std::uint8_t {
    abs8 = 1,
    abs16_be = 2,
    rel8 = 3,
    direct8 = 4,
};

struct Relocation {
    std::uint32_t section_index{};
    std::uint32_t offset{};
    RelocationType type{RelocationType::abs8};
    std::uint32_t symbol_index{};
    std::int32_t addend{};

    bool operator==(const Relocation&) const = default;
};

struct SourceLineMapping {
    std::uint32_t section_index{};
    std::uint32_t offset{};
    std::uint32_t length{};
    std::uint32_t source_file_index{};
    std::uint32_t line{};
    std::uint32_t column{};

    bool operator==(const SourceLineMapping&) const = default;
};

struct ObjectFile {
    std::string target_profile;
    BuildIdentity build;
    std::vector<SourceFile> source_files;
    std::vector<Section> sections;
    std::vector<Symbol> symbols;
    std::vector<Relocation> relocations;
    std::vector<SourceLineMapping> source_lines;

    bool operator==(const ObjectFile&) const = default;
};

enum class ErrorCode : std::uint8_t {
    invalid_magic,
    unsupported_version,
    truncated,
    invalid_encoding,
    invalid_value,
    invalid_reference,
    limit_exceeded,
    trailing_data,
};

class Error final : public std::runtime_error {
public:
    Error(ErrorCode code, std::string message, std::optional<std::size_t> byte_offset);

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] std::optional<std::size_t> byte_offset() const noexcept;

private:
    ErrorCode code_;
    std::optional<std::size_t> byte_offset_;
};

[[nodiscard]] std::vector<std::uint8_t> write(const ObjectFile& object);
[[nodiscard]] ObjectFile read(std::span<const std::uint8_t> bytes);

}  // namespace jr800::formats::jro
