// SPDX-License-Identifier: MIT

#include "jr800/formats/jro.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace jr800::formats::jro {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{0x4A, 0x52, 0x4F, 0x00};
constexpr std::size_t kMaxFileSize = 64U * 1024U * 1024U;
constexpr std::size_t kMaxStringSize = 65'535U;
constexpr std::size_t kMaxRecordCount = 65'535U;
constexpr std::uint32_t kMaxSectionSize = 65'536U;
constexpr std::uint32_t kNoIndex = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint8_t kKnownSectionAttributes = 0x07U;

[[noreturn]] void fail(
    ErrorCode code,
    std::string message,
    std::optional<std::size_t> byte_offset = std::nullopt
) {
    throw Error(code, std::move(message), byte_offset);
}

bool is_valid_utf8(std::string_view text) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = bytes[index];
        if (first == 0) {
            return false;
        }
        if (first <= 0x7FU) {
            ++index;
            continue;
        }

        auto continuation = [&](std::size_t position) {
            return position < text.size() && bytes[position] >= 0x80U
                && bytes[position] <= 0xBFU;
        };

        if (first >= 0xC2U && first <= 0xDFU) {
            if (!continuation(index + 1U)) {
                return false;
            }
            index += 2U;
            continue;
        }
        if (first == 0xE0U) {
            if (index + 2U >= text.size() || bytes[index + 1U] < 0xA0U
                || bytes[index + 1U] > 0xBFU || !continuation(index + 2U)) {
                return false;
            }
            index += 3U;
            continue;
        }
        if ((first >= 0xE1U && first <= 0xECU) || (first >= 0xEEU && first <= 0xEFU)) {
            if (!continuation(index + 1U) || !continuation(index + 2U)) {
                return false;
            }
            index += 3U;
            continue;
        }
        if (first == 0xEDU) {
            if (index + 2U >= text.size() || bytes[index + 1U] < 0x80U
                || bytes[index + 1U] > 0x9FU || !continuation(index + 2U)) {
                return false;
            }
            index += 3U;
            continue;
        }
        if (first == 0xF0U) {
            if (index + 3U >= text.size() || bytes[index + 1U] < 0x90U
                || bytes[index + 1U] > 0xBFU || !continuation(index + 2U)
                || !continuation(index + 3U)) {
                return false;
            }
            index += 4U;
            continue;
        }
        if (first >= 0xF1U && first <= 0xF3U) {
            if (!continuation(index + 1U) || !continuation(index + 2U)
                || !continuation(index + 3U)) {
                return false;
            }
            index += 4U;
            continue;
        }
        if (first == 0xF4U) {
            if (index + 3U >= text.size() || bytes[index + 1U] < 0x80U
                || bytes[index + 1U] > 0x8FU || !continuation(index + 2U)
                || !continuation(index + 3U)) {
                return false;
            }
            index += 4U;
            continue;
        }
        return false;
    }
    return true;
}

void validate_text(
    std::string_view text,
    std::string_view field,
    bool allow_empty,
    std::optional<std::size_t> byte_offset = std::nullopt
) {
    if (!allow_empty && text.empty()) {
        fail(ErrorCode::invalid_value, std::string(field) + " must not be empty", byte_offset);
    }
    if (text.size() > kMaxStringSize) {
        fail(ErrorCode::limit_exceeded, std::string(field) + " is too long", byte_offset);
    }
    if (!is_valid_utf8(text)) {
        fail(
            ErrorCode::invalid_encoding,
            std::string(field) + " must be UTF-8 without NUL",
            byte_offset
        );
    }
}

void validate_profile_identifier(std::string_view profile) {
    validate_text(profile, "target profile", false);
    const auto valid_first = [](char value) { return value >= 'a' && value <= 'z'; };
    const auto valid_rest = [&](char value) {
        return valid_first(value) || (value >= '0' && value <= '9') || value == '_';
    };
    if (!valid_first(profile.front())
        || !std::all_of(profile.begin() + 1, profile.end(), valid_rest)) {
        fail(
            ErrorCode::invalid_value,
            "target profile must be a lower-case identifier"
        );
    }
}

void validate_count(std::size_t count, std::string_view field) {
    if (count > kMaxRecordCount) {
        fail(ErrorCode::limit_exceeded, std::string(field) + " exceeds the record limit");
    }
}

bool is_power_of_two(std::uint32_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

std::uint32_t relocation_width(RelocationType type) {
    switch (type) {
    case RelocationType::abs8:
    case RelocationType::rel8:
    case RelocationType::direct8:
        return 1U;
    case RelocationType::abs16_be:
        return 2U;
    }
    fail(ErrorCode::invalid_encoding, "unknown relocation type");
}

bool relocation_less(const Relocation& left, const Relocation& right) {
    return std::tie(
               left.section_index,
               left.offset,
               left.type,
               left.symbol_index,
               left.addend
           )
        < std::tie(
               right.section_index,
               right.offset,
               right.type,
               right.symbol_index,
               right.addend
           );
}

bool source_line_less(const SourceLineMapping& left, const SourceLineMapping& right) {
    return std::tie(
               left.section_index,
               left.offset,
               left.length,
               left.source_file_index,
               left.line,
               left.column
           )
        < std::tie(
               right.section_index,
               right.offset,
               right.length,
               right.source_file_index,
               right.line,
               right.column
           );
}

void validate_object(const ObjectFile& object) {
    validate_profile_identifier(object.target_profile);
    validate_text(object.build.producer, "build producer", false);
    validate_text(object.build.producer_version, "build producer version", false);
    validate_count(object.source_files.size(), "source-file count");
    validate_count(object.sections.size(), "section count");
    validate_count(object.symbols.size(), "symbol count");
    validate_count(object.relocations.size(), "relocation count");
    validate_count(object.source_lines.size(), "source-line count");

    std::unordered_set<std::string> source_paths;
    for (const auto& source : object.source_files) {
        validate_text(source.path, "source path", false);
        if (!source_paths.insert(source.path).second) {
            fail(ErrorCode::invalid_value, "source paths must be unique");
        }
    }

    std::unordered_set<std::string> section_names;
    for (const auto& section : object.sections) {
        validate_text(section.name, "section name", false);
        if (!section_names.insert(section.name).second) {
            fail(ErrorCode::invalid_value, "section names must be unique");
        }
        switch (section.type) {
        case SectionType::program_bits:
            if (section.data.size() != section.logical_size) {
                fail(
                    ErrorCode::invalid_value,
                    "PROGRAM_BITS data size must equal logical size"
                );
            }
            break;
        case SectionType::no_bits:
            if (!section.data.empty()) {
                fail(ErrorCode::invalid_value, "NO_BITS sections must not contain data");
            }
            break;
        default:
            fail(ErrorCode::invalid_encoding, "unknown section type");
        }
        if ((static_cast<std::uint8_t>(section.attributes) & ~kKnownSectionAttributes) != 0U) {
            fail(ErrorCode::invalid_encoding, "unknown section attribute bit");
        }
        if (!is_power_of_two(section.alignment) || section.alignment > kMaxSectionSize) {
            fail(ErrorCode::invalid_value, "section alignment must be a supported power of two");
        }
        if (section.logical_size > kMaxSectionSize) {
            fail(ErrorCode::limit_exceeded, "section exceeds the 16-bit logical size limit");
        }
        if (section.fixed_address.has_value()) {
            const auto end = static_cast<std::uint64_t>(*section.fixed_address)
                + section.logical_size;
            if (end > kMaxSectionSize) {
                fail(ErrorCode::invalid_value, "fixed section exceeds the 16-bit address space");
            }
        }
    }

    std::unordered_set<std::string> symbol_names;
    for (const auto& symbol : object.symbols) {
        validate_text(symbol.name, "symbol name", false);
        if (!symbol_names.insert(symbol.name).second) {
            fail(ErrorCode::invalid_value, "symbol names must be unique");
        }
        switch (symbol.binding) {
        case SymbolBinding::local:
        case SymbolBinding::global:
            break;
        default:
            fail(ErrorCode::invalid_encoding, "unknown symbol binding");
        }

        switch (symbol.definition) {
        case SymbolDefinition::section: {
            if (!symbol.section_index.has_value()
                || *symbol.section_index >= object.sections.size()) {
                fail(ErrorCode::invalid_reference, "section symbol has an invalid section index");
            }
            const auto section_size = object.sections[*symbol.section_index].logical_size;
            const auto end = static_cast<std::uint64_t>(symbol.value) + symbol.size;
            if (symbol.value > section_size || end > section_size) {
                fail(ErrorCode::invalid_value, "section symbol exceeds its section");
            }
            break;
        }
        case SymbolDefinition::absolute:
            if (symbol.section_index.has_value() || symbol.value > 0xFFFFU
                || symbol.size != 0U) {
                fail(ErrorCode::invalid_value, "absolute symbol fields are inconsistent");
            }
            break;
        case SymbolDefinition::undefined:
            if (symbol.binding != SymbolBinding::global || symbol.section_index.has_value()
                || symbol.value != 0U || symbol.size != 0U) {
                fail(ErrorCode::invalid_value, "undefined symbol fields are inconsistent");
            }
            break;
        default:
            fail(ErrorCode::invalid_encoding, "unknown symbol definition");
        }
    }

    auto relocations = object.relocations;
    std::sort(relocations.begin(), relocations.end(), relocation_less);
    std::optional<std::uint32_t> previous_relocation_section;
    std::uint64_t previous_relocation_end = 0;
    for (const auto& relocation : relocations) {
        if (relocation.section_index >= object.sections.size()) {
            fail(ErrorCode::invalid_reference, "relocation has an invalid section index");
        }
        if (relocation.symbol_index >= object.symbols.size()) {
            fail(ErrorCode::invalid_reference, "relocation has an invalid symbol index");
        }
        const auto& section = object.sections[relocation.section_index];
        if (section.type != SectionType::program_bits) {
            fail(ErrorCode::invalid_value, "relocation must target PROGRAM_BITS");
        }
        const auto width = relocation_width(relocation.type);
        const auto end = static_cast<std::uint64_t>(relocation.offset) + width;
        if (end > section.data.size()) {
            fail(ErrorCode::invalid_value, "relocation field exceeds section data");
        }
        if (previous_relocation_section == relocation.section_index
            && relocation.offset < previous_relocation_end) {
            fail(ErrorCode::invalid_value, "relocation fields must not overlap");
        }
        previous_relocation_section = relocation.section_index;
        previous_relocation_end = end;
    }

    auto source_lines = object.source_lines;
    std::sort(source_lines.begin(), source_lines.end(), source_line_less);
    std::optional<std::uint32_t> previous_line_section;
    std::uint64_t previous_line_end = 0;
    for (const auto& mapping : source_lines) {
        if (mapping.section_index >= object.sections.size()) {
            fail(ErrorCode::invalid_reference, "source line has an invalid section index");
        }
        if (mapping.source_file_index >= object.source_files.size()) {
            fail(ErrorCode::invalid_reference, "source line has an invalid file index");
        }
        if (mapping.length == 0U || mapping.line == 0U) {
            fail(ErrorCode::invalid_value, "source line length and line must be nonzero");
        }
        const auto end = static_cast<std::uint64_t>(mapping.offset) + mapping.length;
        if (end > object.sections[mapping.section_index].logical_size) {
            fail(ErrorCode::invalid_value, "source line range exceeds its section");
        }
        if (previous_line_section == mapping.section_index
            && mapping.offset < previous_line_end) {
            fail(ErrorCode::invalid_value, "source line ranges must not overlap");
        }
        previous_line_section = mapping.section_index;
        previous_line_end = end;
    }
}

class Writer {
public:
    void byte(std::uint8_t value) {
        ensure_capacity(1U);
        bytes_.push_back(value);
    }

    void u16(std::uint16_t value) {
        byte(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        byte(static_cast<std::uint8_t>(value & 0xFFU));
    }

    void u32(std::uint32_t value) {
        byte(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
        byte(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        byte(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        byte(static_cast<std::uint8_t>(value & 0xFFU));
    }

    void i32(std::int32_t value) {
        u32(std::bit_cast<std::uint32_t>(value));
    }

    void raw(std::span<const std::uint8_t> values) {
        ensure_capacity(values.size());
        bytes_.insert(bytes_.end(), values.begin(), values.end());
    }

    void text(std::string_view value) {
        u32(static_cast<std::uint32_t>(value.size()));
        raw(std::span{
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size(),
        });
    }

    [[nodiscard]] std::vector<std::uint8_t> finish() && {
        return std::move(bytes_);
    }

private:
    void ensure_capacity(std::size_t additional) const {
        if (additional > kMaxFileSize - bytes_.size()) {
            fail(ErrorCode::limit_exceeded, "serialized JRO exceeds the file size limit");
        }
    }

    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {
        if (bytes.size() > kMaxFileSize) {
            fail(ErrorCode::limit_exceeded, "JRO exceeds the file size limit", 0U);
        }
    }

    [[nodiscard]] std::size_t offset() const noexcept {
        return offset_;
    }

    [[nodiscard]] bool at_end() const noexcept {
        return offset_ == bytes_.size();
    }

    [[nodiscard]] std::uint8_t byte() {
        require(1U);
        return bytes_[offset_++];
    }

    [[nodiscard]] std::uint16_t u16() {
        const auto first = byte();
        const auto second = byte();
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(first) << 8U) | second
        );
    }

    [[nodiscard]] std::uint32_t u32() {
        const auto first = byte();
        const auto second = byte();
        const auto third = byte();
        const auto fourth = byte();
        return (static_cast<std::uint32_t>(first) << 24U)
            | (static_cast<std::uint32_t>(second) << 16U)
            | (static_cast<std::uint32_t>(third) << 8U)
            | static_cast<std::uint32_t>(fourth);
    }

    [[nodiscard]] std::int32_t i32() {
        return std::bit_cast<std::int32_t>(u32());
    }

    [[nodiscard]] std::span<const std::uint8_t> raw(std::size_t size) {
        require(size);
        const auto result = bytes_.subspan(offset_, size);
        offset_ += size;
        return result;
    }

    [[nodiscard]] std::string text(std::string_view field) {
        const auto length_offset = offset_;
        const auto length = u32();
        if (length > kMaxStringSize) {
            fail(ErrorCode::limit_exceeded, std::string(field) + " is too long", length_offset);
        }
        const auto text_offset = offset_;
        const auto values = raw(length);
        std::string result(
            reinterpret_cast<const char*>(values.data()),
            values.size()
        );
        validate_text(result, field, false, text_offset);
        return result;
    }

    [[nodiscard]] Sha256Digest digest() {
        Sha256Digest result{};
        const auto values = raw(result.size());
        std::copy(values.begin(), values.end(), result.begin());
        return result;
    }

private:
    void require(std::size_t size) const {
        if (size > bytes_.size() - offset_) {
            fail(ErrorCode::truncated, "truncated JRO input", offset_);
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

std::uint32_t read_count(Reader& reader, std::string_view field) {
    const auto offset = reader.offset();
    const auto count = reader.u32();
    if (count > kMaxRecordCount) {
        fail(ErrorCode::limit_exceeded, std::string(field) + " exceeds the record limit", offset);
    }
    return count;
}

SectionType read_section_type(Reader& reader) {
    const auto offset = reader.offset();
    switch (reader.byte()) {
    case static_cast<std::uint8_t>(SectionType::program_bits):
        return SectionType::program_bits;
    case static_cast<std::uint8_t>(SectionType::no_bits):
        return SectionType::no_bits;
    default:
        fail(ErrorCode::invalid_encoding, "unknown section type", offset);
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
        fail(ErrorCode::invalid_encoding, "unknown symbol binding", offset);
    }
}

SymbolDefinition read_symbol_definition(Reader& reader) {
    const auto offset = reader.offset();
    switch (reader.byte()) {
    case static_cast<std::uint8_t>(SymbolDefinition::section):
        return SymbolDefinition::section;
    case static_cast<std::uint8_t>(SymbolDefinition::absolute):
        return SymbolDefinition::absolute;
    case static_cast<std::uint8_t>(SymbolDefinition::undefined):
        return SymbolDefinition::undefined;
    default:
        fail(ErrorCode::invalid_encoding, "unknown symbol definition", offset);
    }
}

RelocationType read_relocation_type(Reader& reader) {
    const auto offset = reader.offset();
    switch (reader.byte()) {
    case static_cast<std::uint8_t>(RelocationType::abs8):
        return RelocationType::abs8;
    case static_cast<std::uint8_t>(RelocationType::abs16_be):
        return RelocationType::abs16_be;
    case static_cast<std::uint8_t>(RelocationType::rel8):
        return RelocationType::rel8;
    case static_cast<std::uint8_t>(RelocationType::direct8):
        return RelocationType::direct8;
    default:
        fail(ErrorCode::invalid_encoding, "unknown relocation type", offset);
    }
}

}  // namespace

Error::Error(
    ErrorCode code,
    std::string message,
    std::optional<std::size_t> byte_offset
)
    : std::runtime_error(std::move(message)), code_(code), byte_offset_(byte_offset) {}

ErrorCode Error::code() const noexcept {
    return code_;
}

std::optional<std::size_t> Error::byte_offset() const noexcept {
    return byte_offset_;
}

std::vector<std::uint8_t> write(const ObjectFile& object) {
    validate_object(object);

    auto relocations = object.relocations;
    std::sort(relocations.begin(), relocations.end(), relocation_less);
    auto source_lines = object.source_lines;
    std::sort(source_lines.begin(), source_lines.end(), source_line_less);

    Writer writer;
    writer.raw(kMagic);
    writer.u16(format_major_version);
    writer.u16(format_minor_version);
    writer.u32(0U);
    writer.text(object.target_profile);
    writer.text(object.build.producer);
    writer.text(object.build.producer_version);
    writer.raw(object.build.build_id);
    writer.u32(static_cast<std::uint32_t>(object.source_files.size()));
    writer.u32(static_cast<std::uint32_t>(object.sections.size()));
    writer.u32(static_cast<std::uint32_t>(object.symbols.size()));
    writer.u32(static_cast<std::uint32_t>(relocations.size()));
    writer.u32(static_cast<std::uint32_t>(source_lines.size()));

    for (const auto& source : object.source_files) {
        writer.text(source.path);
        writer.raw(source.content_sha256);
    }

    for (const auto& section : object.sections) {
        writer.text(section.name);
        writer.byte(static_cast<std::uint8_t>(section.type));
        writer.byte(static_cast<std::uint8_t>(section.attributes));
        writer.byte(section.fixed_address.has_value() ? 1U : 0U);
        writer.byte(0U);
        writer.u32(section.alignment);
        writer.u32(section.fixed_address.value_or(0U));
        writer.u32(section.logical_size);
        writer.u32(static_cast<std::uint32_t>(section.data.size()));
        writer.raw(section.data);
    }

    for (const auto& symbol : object.symbols) {
        writer.text(symbol.name);
        writer.byte(static_cast<std::uint8_t>(symbol.binding));
        writer.byte(static_cast<std::uint8_t>(symbol.definition));
        writer.u16(0U);
        writer.u32(symbol.section_index.value_or(kNoIndex));
        writer.u32(symbol.value);
        writer.u32(symbol.size);
    }

    for (const auto& relocation : relocations) {
        writer.u32(relocation.section_index);
        writer.u32(relocation.offset);
        writer.byte(static_cast<std::uint8_t>(relocation.type));
        writer.byte(0U);
        writer.byte(0U);
        writer.byte(0U);
        writer.u32(relocation.symbol_index);
        writer.i32(relocation.addend);
    }

    for (const auto& mapping : source_lines) {
        writer.u32(mapping.section_index);
        writer.u32(mapping.offset);
        writer.u32(mapping.length);
        writer.u32(mapping.source_file_index);
        writer.u32(mapping.line);
        writer.u32(mapping.column);
    }

    return std::move(writer).finish();
}

ObjectFile read(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    const auto magic = reader.raw(kMagic.size());
    if (!std::equal(magic.begin(), magic.end(), kMagic.begin())) {
        fail(ErrorCode::invalid_magic, "invalid JRO magic", 0U);
    }

    const auto major_offset = reader.offset();
    const auto major = reader.u16();
    const auto minor = reader.u16();
    if (major != format_major_version || minor != format_minor_version) {
        fail(ErrorCode::unsupported_version, "unsupported JRO version", major_offset);
    }
    const auto flags_offset = reader.offset();
    if (reader.u32() != 0U) {
        fail(ErrorCode::invalid_encoding, "nonzero JRO header flags", flags_offset);
    }

    ObjectFile object;
    object.target_profile = reader.text("target profile");
    object.build.producer = reader.text("build producer");
    object.build.producer_version = reader.text("build producer version");
    object.build.build_id = reader.digest();

    const auto source_count = read_count(reader, "source-file count");
    const auto section_count = read_count(reader, "section count");
    const auto symbol_count = read_count(reader, "symbol count");
    const auto relocation_count = read_count(reader, "relocation count");
    const auto source_line_count = read_count(reader, "source-line count");

    object.source_files.reserve(source_count);
    for (std::uint32_t index = 0; index < source_count; ++index) {
        object.source_files.push_back(SourceFile{
            reader.text("source path"),
            reader.digest(),
        });
    }

    object.sections.reserve(section_count);
    for (std::uint32_t index = 0; index < section_count; ++index) {
        Section section;
        section.name = reader.text("section name");
        section.type = read_section_type(reader);
        const auto attributes_offset = reader.offset();
        const auto attributes = reader.byte();
        if ((attributes & ~kKnownSectionAttributes) != 0U) {
            fail(ErrorCode::invalid_encoding, "unknown section attribute bit", attributes_offset);
        }
        section.attributes = static_cast<SectionAttributes>(attributes);
        const auto placement_offset = reader.offset();
        const auto placement = reader.byte();
        const auto reserved_offset = reader.offset();
        if (reader.byte() != 0U) {
            fail(ErrorCode::invalid_encoding, "nonzero section reserved field", reserved_offset);
        }
        section.alignment = reader.u32();
        const auto fixed_address_offset = reader.offset();
        const auto fixed_address = reader.u32();
        if (placement == 0U) {
            if (fixed_address != 0U) {
                fail(
                    ErrorCode::invalid_value,
                    "relocatable section has a fixed address",
                    fixed_address_offset
                );
            }
        } else if (placement == 1U) {
            if (fixed_address > 0xFFFFU) {
                fail(
                    ErrorCode::invalid_value,
                    "fixed section address exceeds 16 bits",
                    fixed_address_offset
                );
            }
            section.fixed_address = static_cast<std::uint16_t>(fixed_address);
        } else {
            fail(ErrorCode::invalid_encoding, "unknown section placement", placement_offset);
        }
        section.logical_size = reader.u32();
        const auto data_size_offset = reader.offset();
        const auto data_size = reader.u32();
        if (data_size > kMaxSectionSize) {
            fail(ErrorCode::limit_exceeded, "section data exceeds the limit", data_size_offset);
        }
        const auto data = reader.raw(data_size);
        section.data.assign(data.begin(), data.end());
        object.sections.push_back(std::move(section));
    }

    object.symbols.reserve(symbol_count);
    for (std::uint32_t index = 0; index < symbol_count; ++index) {
        Symbol symbol;
        symbol.name = reader.text("symbol name");
        symbol.binding = read_symbol_binding(reader);
        symbol.definition = read_symbol_definition(reader);
        const auto reserved_offset = reader.offset();
        if (reader.u16() != 0U) {
            fail(ErrorCode::invalid_encoding, "nonzero symbol reserved field", reserved_offset);
        }
        const auto section_index = reader.u32();
        if (section_index != kNoIndex) {
            symbol.section_index = section_index;
        }
        symbol.value = reader.u32();
        symbol.size = reader.u32();
        object.symbols.push_back(std::move(symbol));
    }

    object.relocations.reserve(relocation_count);
    for (std::uint32_t index = 0; index < relocation_count; ++index) {
        Relocation relocation;
        relocation.section_index = reader.u32();
        relocation.offset = reader.u32();
        relocation.type = read_relocation_type(reader);
        const auto reserved_offset = reader.offset();
        if (reader.byte() != 0U || reader.byte() != 0U || reader.byte() != 0U) {
            fail(
                ErrorCode::invalid_encoding,
                "nonzero relocation reserved field",
                reserved_offset
            );
        }
        relocation.symbol_index = reader.u32();
        relocation.addend = reader.i32();
        object.relocations.push_back(relocation);
    }

    object.source_lines.reserve(source_line_count);
    for (std::uint32_t index = 0; index < source_line_count; ++index) {
        object.source_lines.push_back(SourceLineMapping{
            reader.u32(),
            reader.u32(),
            reader.u32(),
            reader.u32(),
            reader.u32(),
            reader.u32(),
        });
    }

    if (!reader.at_end()) {
        fail(ErrorCode::trailing_data, "trailing data after JRO tables", reader.offset());
    }

    validate_object(object);
    std::sort(object.relocations.begin(), object.relocations.end(), relocation_less);
    std::sort(object.source_lines.begin(), object.source_lines.end(), source_line_less);
    return object;
}

}  // namespace jr800::formats::jro
