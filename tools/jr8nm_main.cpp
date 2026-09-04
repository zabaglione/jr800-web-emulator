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
#include <vector>

#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/jro.hpp"
#include "jr800/formats/linked_error.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

void print_usage(std::ostream& stream) {
    stream << "Usage: jr8nm <input.jro|input.j8d>\n";
}

std::optional<std::filesystem::path> parse_input(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "jr8nm: exactly one JRO or JR8DBG input is required\n";
        return std::nullopt;
    }
    const std::string_view argument{argv[1]};
    if (!argument.empty() && argument.front() == '-') {
        std::cerr << "jr8nm: unknown option: " << argument << '\n';
        return std::nullopt;
    }
    return std::filesystem::path{argument};
}

std::optional<std::vector<std::uint8_t>> read_binary(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8nm: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

std::string_view binding_name(
    jr800::formats::jro::SymbolBinding binding
) noexcept {
    using jr800::formats::jro::SymbolBinding;
    switch (binding) {
    case SymbolBinding::local:
        return "local";
    case SymbolBinding::global:
        return "global";
    }
    return "unknown";
}

std::string_view definition_name(
    jr800::formats::jro::SymbolDefinition definition
) noexcept {
    using jr800::formats::jro::SymbolDefinition;
    switch (definition) {
    case SymbolDefinition::section:
        return "section";
    case SymbolDefinition::absolute:
        return "absolute";
    case SymbolDefinition::undefined:
        return "undefined";
    }
    return "unknown";
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

void print_symbols(const jr800::formats::jro::ObjectFile& object) {
    std::cout << "JRO 1.0 target=" << object.target_profile << '\n'
              << "INDEX\tBINDING\tDEFINITION\tVALUE\tSIZE\tSECTION\tNAME\n";
    for (std::size_t index = 0U; index < object.symbols.size(); ++index) {
        const auto& symbol = object.symbols[index];
        std::cout << index << '\t' << binding_name(symbol.binding) << '\t'
                  << definition_name(symbol.definition) << '\t';
        if (symbol.definition
            == jr800::formats::jro::SymbolDefinition::undefined) {
            std::cout << '-';
        } else {
            std::cout << hex_value(symbol.value, 8);
        }
        std::cout << '\t' << symbol.size << '\t';
        if (symbol.section_index.has_value()) {
            std::cout << quoted_text(
                object.sections[*symbol.section_index].name
            );
        } else {
            std::cout << '-';
        }
        std::cout << '\t' << quoted_text(symbol.name) << '\n';
    }
}

std::string_view binding_name(
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

std::string_view kind_name(
    jr800::formats::jr8dbg::SymbolKind kind
) noexcept {
    using jr800::formats::jr8dbg::SymbolKind;
    switch (kind) {
    case SymbolKind::address:
        return "address";
    case SymbolKind::absolute:
        return "absolute";
    }
    return "unknown";
}

void print_symbols(const jr800::formats::jr8dbg::DebugInfo& debug_info) {
    std::cout << "JR8DBG 1.0 target=" << debug_info.target_profile << '\n'
              << "INDEX\tBINDING\tKIND\tVALUE\tSIZE\tSOURCE\tNAME\n";
    for (std::size_t index = 0U; index < debug_info.symbols.size(); ++index) {
        const auto& symbol = debug_info.symbols[index];
        std::cout << index << '\t' << binding_name(symbol.binding) << '\t'
                  << kind_name(symbol.kind) << '\t'
                  << hex_value(symbol.value, 4) << '\t' << symbol.size << '\t';
        if (symbol.source_file_index.has_value()) {
            std::cout << quoted_text(
                debug_info.source_files[*symbol.source_file_index].path
            );
        } else {
            std::cout << '-';
        }
        std::cout << '\t' << quoted_text(symbol.name) << '\n';
    }
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
        std::cout << "jr8nm " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }

    const auto input_path = parse_input(argc, argv);
    if (!input_path.has_value()) {
        print_usage(std::cerr);
        return 2;
    }
    const auto bytes = read_binary(*input_path);
    if (!bytes.has_value()) {
        return 2;
    }

    constexpr std::array<std::uint8_t, 4U> jro_magic{'J', 'R', 'O', 0U};
    constexpr std::array<std::uint8_t, 8U> debug_magic{
        'J', 'R', '8', 'D', 'B', 'G', 0U, 0U,
    };
    if (starts_with(*bytes, jro_magic)) {
        try {
            print_symbols(jr800::formats::jro::read(*bytes));
        } catch (const jr800::formats::jro::Error& error) {
            std::cerr << "jr8nm: invalid JRO: " << error.what();
            if (error.byte_offset().has_value()) {
                std::cerr << " at byte " << *error.byte_offset();
            }
            std::cerr << '\n';
            return 1;
        }
        return 0;
    }
    if (starts_with(*bytes, debug_magic)) {
        try {
            print_symbols(jr800::formats::jr8dbg::read(*bytes));
        } catch (const jr800::formats::linked::Error& error) {
            std::cerr << "jr8nm: invalid JR8DBG: " << error.what();
            if (error.byte_offset().has_value()) {
                std::cerr << " at byte " << *error.byte_offset();
            }
            std::cerr << '\n';
            return 1;
        }
        return 0;
    }
    std::cerr << "jr8nm: unsupported input format\n";
    return 1;
}
