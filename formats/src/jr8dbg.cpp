// SPDX-License-Identifier: MIT

#include "jr800/formats/jr8dbg.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "linked_io.hpp"

namespace jr800::formats::jr8dbg {
namespace {

using linked::ErrorCode;
using linked::detail::Reader;
using linked::detail::Writer;
using linked::detail::fail;

constexpr std::array<std::uint8_t, 8> kMagic{
    0x4A, 0x52, 0x38, 0x44, 0x42, 0x47, 0x00, 0x00,
};
constexpr std::uint32_t kNoIndex = std::numeric_limits<std::uint32_t>::max();

bool symbol_less(const Symbol& left, const Symbol& right) {
    return std::tie(
               left.value,
               left.kind,
               left.name,
               left.binding,
               left.source_file_index,
               left.size
           )
        < std::tie(
               right.value,
               right.kind,
               right.name,
               right.binding,
               right.source_file_index,
               right.size
           );
}

bool line_less(const LineMapping& left, const LineMapping& right) {
    return std::tie(
               left.address,
               left.length,
               left.source_file_index,
               left.line,
               left.column
           )
        < std::tie(
               right.address,
               right.length,
               right.source_file_index,
               right.line,
               right.column
           );
}

void validate(const DebugInfo& debug_info) {
    linked::detail::validate_profile_identifier(debug_info.target_profile);
    linked::detail::validate_count(debug_info.source_files.size(), "source-file count");
    linked::detail::validate_count(debug_info.symbols.size(), "symbol count");
    linked::detail::validate_count(debug_info.line_mappings.size(), "line-mapping count");

    std::unordered_set<std::string> source_paths;
    for (const auto& source : debug_info.source_files) {
        linked::detail::validate_text(source.path, "source path");
        if (!source_paths.insert(source.path).second) {
            fail(ErrorCode::invalid_value, "JR8DBG source paths must be unique");
        }
    }

    std::unordered_set<std::string> global_names;
    for (const auto& symbol : debug_info.symbols) {
        linked::detail::validate_text(symbol.name, "symbol name");
        switch (symbol.binding) {
        case SymbolBinding::local:
            break;
        case SymbolBinding::global:
            if (!global_names.insert(symbol.name).second) {
                fail(ErrorCode::invalid_value, "JR8DBG global symbol names must be unique");
            }
            break;
        default:
            fail(ErrorCode::invalid_encoding, "unknown JR8DBG symbol binding");
        }
        switch (symbol.kind) {
        case SymbolKind::address:
            if (static_cast<std::uint64_t>(symbol.value) + symbol.size > 65'536U) {
                fail(ErrorCode::invalid_value, "JR8DBG address symbol exceeds address space");
            }
            break;
        case SymbolKind::absolute:
            if (symbol.size != 0U) {
                fail(ErrorCode::invalid_value, "JR8DBG absolute symbol size must be zero");
            }
            break;
        default:
            fail(ErrorCode::invalid_encoding, "unknown JR8DBG symbol kind");
        }
        if (symbol.source_file_index.has_value()
            && *symbol.source_file_index >= debug_info.source_files.size()) {
            fail(ErrorCode::invalid_reference, "JR8DBG symbol has an invalid source index");
        }
    }

    auto mappings = debug_info.line_mappings;
    std::sort(mappings.begin(), mappings.end(), line_less);
    std::uint64_t previous_end = 0U;
    bool first = true;
    for (const auto& mapping : mappings) {
        if (mapping.source_file_index >= debug_info.source_files.size()) {
            fail(ErrorCode::invalid_reference, "JR8DBG line has an invalid source index");
        }
        if (mapping.length == 0U || mapping.line == 0U) {
            fail(ErrorCode::invalid_value, "JR8DBG line length and line must be nonzero");
        }
        const auto begin = static_cast<std::uint64_t>(mapping.address);
        const auto end = begin + mapping.length;
        if (end > 65'536U) {
            fail(ErrorCode::invalid_value, "JR8DBG line range exceeds address space");
        }
        if (!first && begin < previous_end) {
            fail(ErrorCode::invalid_value, "JR8DBG line ranges must not overlap");
        }
        first = false;
        previous_end = end;
    }
}

SymbolBinding read_symbol_binding(Reader& reader) {
    const auto offset = reader.offset();
    switch (reader.byte()) {
    case static_cast<std::uint8_t>(SymbolBinding::local):
        return SymbolBinding::local;
    case static_cast<std::uint8_t>(SymbolBinding::global):
        return SymbolBinding::global;
    default:
        fail(ErrorCode::invalid_encoding, "unknown JR8DBG symbol binding", offset);
    }
}

SymbolKind read_symbol_kind(Reader& reader) {
    const auto offset = reader.offset();
    switch (reader.byte()) {
    case static_cast<std::uint8_t>(SymbolKind::address):
        return SymbolKind::address;
    case static_cast<std::uint8_t>(SymbolKind::absolute):
        return SymbolKind::absolute;
    default:
        fail(ErrorCode::invalid_encoding, "unknown JR8DBG symbol kind", offset);
    }
}

}  // namespace

std::vector<std::uint8_t> write(const DebugInfo& debug_info) {
    validate(debug_info);
    auto symbols = debug_info.symbols;
    std::sort(symbols.begin(), symbols.end(), symbol_less);
    auto mappings = debug_info.line_mappings;
    std::sort(mappings.begin(), mappings.end(), line_less);

    Writer writer{"JR8DBG"};
    writer.raw(kMagic);
    writer.u16(format_major_version);
    writer.u16(format_minor_version);
    writer.u32(0U);
    writer.text(debug_info.target_profile);
    writer.digest(debug_info.application_integrity_sha256);
    writer.u32(static_cast<std::uint32_t>(debug_info.source_files.size()));
    writer.u32(static_cast<std::uint32_t>(symbols.size()));
    writer.u32(static_cast<std::uint32_t>(mappings.size()));
    for (const auto& source : debug_info.source_files) {
        writer.text(source.path);
        writer.digest(source.content_sha256);
    }
    for (const auto& symbol : symbols) {
        writer.text(symbol.name);
        writer.byte(static_cast<std::uint8_t>(symbol.binding));
        writer.byte(static_cast<std::uint8_t>(symbol.kind));
        writer.u16(symbol.value);
        writer.u16(0U);
        writer.u32(symbol.size);
        writer.u32(symbol.source_file_index.value_or(kNoIndex));
    }
    for (const auto& mapping : mappings) {
        writer.u16(mapping.address);
        writer.u16(0U);
        writer.u32(mapping.length);
        writer.u32(mapping.source_file_index);
        writer.u32(mapping.line);
        writer.u32(mapping.column);
    }
    return std::move(writer).finish();
}

DebugInfo read(std::span<const std::uint8_t> bytes) {
    Reader reader{bytes, "JR8DBG"};
    const auto magic = reader.raw(kMagic.size());
    if (!std::equal(magic.begin(), magic.end(), kMagic.begin())) {
        fail(ErrorCode::invalid_magic, "invalid JR8DBG magic", 0U);
    }
    const auto version_offset = reader.offset();
    if (reader.u16() != format_major_version || reader.u16() != format_minor_version) {
        fail(ErrorCode::unsupported_version, "unsupported JR8DBG version", version_offset);
    }
    const auto flags_offset = reader.offset();
    if (reader.u32() != 0U) {
        fail(ErrorCode::invalid_encoding, "nonzero JR8DBG header flags", flags_offset);
    }

    DebugInfo debug_info;
    debug_info.target_profile = reader.text("target profile");
    debug_info.application_integrity_sha256 = reader.digest();
    const auto source_count = linked::detail::read_count(reader, "source-file count");
    const auto symbol_count = linked::detail::read_count(reader, "symbol count");
    const auto mapping_count = linked::detail::read_count(reader, "line-mapping count");
    debug_info.source_files.reserve(source_count);
    for (std::uint32_t index = 0; index < source_count; ++index) {
        debug_info.source_files.push_back(SourceFile{
            reader.text("source path"),
            reader.digest(),
        });
    }
    debug_info.symbols.reserve(symbol_count);
    for (std::uint32_t index = 0; index < symbol_count; ++index) {
        Symbol symbol;
        symbol.name = reader.text("symbol name");
        symbol.binding = read_symbol_binding(reader);
        symbol.kind = read_symbol_kind(reader);
        symbol.value = reader.u16();
        const auto reserved_offset = reader.offset();
        if (reader.u16() != 0U) {
            fail(
                ErrorCode::invalid_encoding,
                "nonzero JR8DBG symbol reserved field",
                reserved_offset
            );
        }
        symbol.size = reader.u32();
        const auto source_index = reader.u32();
        if (source_index != kNoIndex) {
            symbol.source_file_index = source_index;
        }
        debug_info.symbols.push_back(std::move(symbol));
    }
    debug_info.line_mappings.reserve(mapping_count);
    for (std::uint32_t index = 0; index < mapping_count; ++index) {
        LineMapping mapping;
        mapping.address = reader.u16();
        const auto reserved_offset = reader.offset();
        if (reader.u16() != 0U) {
            fail(
                ErrorCode::invalid_encoding,
                "nonzero JR8DBG line reserved field",
                reserved_offset
            );
        }
        mapping.length = reader.u32();
        mapping.source_file_index = reader.u32();
        mapping.line = reader.u32();
        mapping.column = reader.u32();
        debug_info.line_mappings.push_back(mapping);
    }
    if (!reader.at_end()) {
        fail(ErrorCode::trailing_data, "trailing data after JR8DBG", reader.offset());
    }
    validate(debug_info);
    return debug_info;
}

const LineMapping* find_line(const DebugInfo& debug_info, std::uint16_t address) noexcept {
    const auto numeric_address = static_cast<std::uint32_t>(address);
    const auto found = std::find_if(
        debug_info.line_mappings.begin(),
        debug_info.line_mappings.end(),
        [&](const LineMapping& mapping) {
            const auto begin = static_cast<std::uint32_t>(mapping.address);
            return numeric_address >= begin && numeric_address < begin + mapping.length;
        }
    );
    return found == debug_info.line_mappings.end() ? nullptr : &*found;
}

const LineMapping* find_source_line(
    const DebugInfo& debug_info,
    std::string_view source_path,
    std::uint32_t line
) noexcept {
    const auto source = std::find_if(
        debug_info.source_files.begin(),
        debug_info.source_files.end(),
        [source_path](const SourceFile& candidate) {
            return candidate.path == source_path;
        }
    );
    if (source == debug_info.source_files.end() || line == 0U) {
        return nullptr;
    }
    const auto source_file_index = static_cast<std::uint32_t>(
        std::distance(debug_info.source_files.begin(), source)
    );
    const LineMapping* result = nullptr;
    for (const auto& mapping : debug_info.line_mappings) {
        if (mapping.source_file_index == source_file_index
            && mapping.line == line
            && (result == nullptr || mapping.address < result->address)) {
            result = &mapping;
        }
    }
    return result;
}

std::vector<const Symbol*> find_symbols(
    const DebugInfo& debug_info,
    std::uint16_t address
) {
    std::vector<const Symbol*> result;
    const auto numeric_address = static_cast<std::uint32_t>(address);
    for (const auto& symbol : debug_info.symbols) {
        if (symbol.kind != SymbolKind::address) {
            continue;
        }
        const auto begin = static_cast<std::uint32_t>(symbol.value);
        const auto matches = symbol.size == 0U ? numeric_address == begin
                                               : numeric_address >= begin
                && numeric_address < begin + symbol.size;
        if (matches) {
            result.push_back(&symbol);
        }
    }
    return result;
}

}  // namespace jr800::formats::jr8dbg
