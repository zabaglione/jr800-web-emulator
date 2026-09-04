// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "jr800/formats/linked_error.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::formats::jr8dbg {

inline constexpr std::uint16_t format_major_version = 1;
inline constexpr std::uint16_t format_minor_version = 0;

struct SourceFile {
    std::string path;
    Sha256Digest content_sha256{};

    bool operator==(const SourceFile&) const = default;
};

enum class SymbolBinding : std::uint8_t {
    local = 1,
    global = 2,
};

enum class SymbolKind : std::uint8_t {
    address = 1,
    absolute = 2,
};

struct Symbol {
    std::string name;
    SymbolBinding binding{SymbolBinding::local};
    SymbolKind kind{SymbolKind::address};
    std::uint16_t value{};
    std::uint32_t size{};
    std::optional<std::uint32_t> source_file_index;

    bool operator==(const Symbol&) const = default;
};

struct LineMapping {
    std::uint16_t address{};
    std::uint32_t length{};
    std::uint32_t source_file_index{};
    std::uint32_t line{};
    std::uint32_t column{};

    bool operator==(const LineMapping&) const = default;
};

struct DebugInfo {
    std::string target_profile;
    Sha256Digest application_integrity_sha256{};
    std::vector<SourceFile> source_files;
    std::vector<Symbol> symbols;
    std::vector<LineMapping> line_mappings;

    bool operator==(const DebugInfo&) const = default;
};

[[nodiscard]] std::vector<std::uint8_t> write(const DebugInfo& debug_info);
[[nodiscard]] DebugInfo read(std::span<const std::uint8_t> bytes);
[[nodiscard]] const LineMapping* find_line(
    const DebugInfo& debug_info,
    std::uint16_t address
) noexcept;
[[nodiscard]] const LineMapping* find_source_line(
    const DebugInfo& debug_info,
    std::string_view source_path,
    std::uint32_t line
) noexcept;
[[nodiscard]] std::vector<const Symbol*> find_symbols(
    const DebugInfo& debug_info,
    std::uint16_t address
);

}  // namespace jr800::formats::jr8dbg
