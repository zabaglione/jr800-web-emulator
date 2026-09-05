// SPDX-License-Identifier: MIT

#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/basic_program.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>

#include "linked_io.hpp"

namespace jr800::formats::jr8app {
namespace {

using linked::ErrorCode;
using linked::detail::Reader;
using linked::detail::Writer;
using linked::detail::fail;

constexpr std::array<std::uint8_t, 8> kMagic{
    0x4A, 0x52, 0x38, 0x41, 0x50, 0x50, 0x00, 0x00,
};
constexpr std::string_view kIntegrityDomain = "JR8APP-INTEGRITY-V1";

bool segment_less(const Segment& left, const Segment& right) {
    return std::tie(left.address, left.kind, left.logical_size, left.data)
        < std::tie(right.address, right.kind, right.logical_size, right.data);
}

std::vector<Segment> canonical_segments(const Application& application) {
    auto segments = application.segments;
    std::sort(segments.begin(), segments.end(), segment_less);
    return segments;
}

void validate_structure(const Application& application) {
    linked::detail::validate_profile_identifier(application.target_profile);
    if (application.name.size() > 16U
        || std::any_of(application.name.begin(), application.name.end(),
            [](auto byte) { return byte < 0x20U; })) {
        fail(ErrorCode::invalid_value, "invalid JR8APP program name");
    }
    if (application.kind == ProgramKind::basic_text
        || application.kind == ProgramKind::basic_binary) {
        if (!application.segments.empty() || application.entry_point != 0U
            || application.basic_data.size() > 32'768U) {
            fail(ErrorCode::invalid_value, "invalid JR8APP BASIC payload");
        }
        static_cast<void>(application.kind == ProgramKind::basic_text
            ? basic_text_line_numbers(application.basic_data)
            : basic_binary_line_numbers(application.basic_data));
        return;
    }
    if (application.kind != ProgramKind::machine_code || !application.basic_data.empty()) {
        fail(ErrorCode::invalid_value, "invalid JR8APP program kind or payload");
    }
    linked::detail::validate_count(application.segments.size(), "segment count");
    if (application.segments.empty()) {
        fail(ErrorCode::invalid_value, "JR8APP must contain at least one segment");
    }

    const auto segments = canonical_segments(application);
    std::uint64_t previous_end = 0U;
    bool first = true;
    for (const auto& segment : segments) {
        if (segment.logical_size == 0U) {
            fail(ErrorCode::invalid_value, "segment logical size must be nonzero");
        }
        switch (segment.kind) {
        case SegmentKind::data:
            if (segment.data.size() != segment.logical_size) {
                fail(ErrorCode::invalid_value, "data segment size mismatch");
            }
            break;
        case SegmentKind::zero_fill:
            if (!segment.data.empty()) {
                fail(ErrorCode::invalid_value, "zero-fill segment must not store data");
            }
            break;
        default:
            fail(ErrorCode::invalid_encoding, "unknown JR8APP segment kind");
        }
        const auto begin = static_cast<std::uint64_t>(segment.address);
        const auto end = begin + segment.logical_size;
        if (end > 65'536U) {
            fail(ErrorCode::invalid_value, "segment exceeds the 16-bit address space");
        }
        if (!first && begin < previous_end) {
            fail(ErrorCode::invalid_value, "JR8APP segments must not overlap");
        }
        first = false;
        previous_end = end;
    }
}

SegmentKind read_segment_kind(Reader& reader) {
    const auto offset = reader.offset();
    switch (reader.byte()) {
    case static_cast<std::uint8_t>(SegmentKind::data):
        return SegmentKind::data;
    case static_cast<std::uint8_t>(SegmentKind::zero_fill):
        return SegmentKind::zero_fill;
    default:
        fail(ErrorCode::invalid_encoding, "unknown JR8APP segment kind", offset);
    }
}

}  // namespace

namespace {
void write_identity(Writer& writer, const Application& application) {
    writer.byte(static_cast<std::uint8_t>(application.kind));
    writer.text(application.target_profile);
    writer.byte(static_cast<std::uint8_t>(application.name.size()));
    writer.raw(application.name);
}

void write_body(Writer& writer, const Application& application) {
    if (application.kind != ProgramKind::machine_code) {
        writer.u32(static_cast<std::uint32_t>(application.basic_data.size()));
        writer.raw(application.basic_data);
        return;
    }
    writer.u16(application.entry_point);
    const auto segments = canonical_segments(application);
    writer.u32(static_cast<std::uint32_t>(segments.size()));
    for (const auto& segment : segments) {
        writer.byte(static_cast<std::uint8_t>(segment.kind));
        writer.u16(segment.address);
        writer.u32(segment.logical_size);
        writer.u32(static_cast<std::uint32_t>(segment.data.size()));
        writer.raw(segment.data);
    }
}
}  // namespace

std::string_view program_kind_name(ProgramKind kind) noexcept {
    switch (kind) {
    case ProgramKind::machine_code: return "machine-code";
    case ProgramKind::basic_text: return "basic-text";
    case ProgramKind::basic_binary: return "basic-binary";
    }
    return "unknown";
}

bool entry_point_is_loaded(const Application& application) noexcept {
    return std::any_of(application.segments.begin(), application.segments.end(),
        [&](const auto& segment) {
            return application.entry_point >= segment.address
                && static_cast<std::uint32_t>(application.entry_point)
                    < static_cast<std::uint32_t>(segment.address) + segment.logical_size;
        });
}

Sha256Digest compute_integrity(const Application& application) {
    validate_structure(application);
    Writer writer{"JR8APP integrity material"};
    writer.raw(std::span{
        reinterpret_cast<const std::uint8_t*>(kIntegrityDomain.data()),
        kIntegrityDomain.size(),
    });
    writer.byte(0U);
    write_identity(writer, application);
    write_body(writer, application);
    return sha256(std::move(writer).finish());
}

std::vector<std::uint8_t> write(const Application& application) {
    if (application.integrity_sha256 != compute_integrity(application)) {
        fail(ErrorCode::integrity_mismatch, "JR8APP integrity SHA-256 mismatch");
    }
    Writer writer{"JR8APP"};
    writer.raw(kMagic);
    writer.u16(format_major_version);
    writer.u16(format_minor_version);
    write_identity(writer, application);
    writer.digest(application.integrity_sha256);
    write_body(writer, application);
    return std::move(writer).finish();
}

Application read(std::span<const std::uint8_t> bytes) {
    Reader reader{bytes, "JR8APP"};
    const auto magic = reader.raw(kMagic.size());
    if (!std::equal(magic.begin(), magic.end(), kMagic.begin())) {
        fail(ErrorCode::invalid_magic, "invalid JR8APP magic", 0U);
    }
    const auto version_offset = reader.offset();
    if (reader.u16() != format_major_version || reader.u16() != format_minor_version) {
        fail(ErrorCode::unsupported_version, "unsupported JR8APP version", version_offset);
    }
    Application application;
    application.kind = static_cast<ProgramKind>(reader.byte());
    if (application.kind != ProgramKind::machine_code
        && application.kind != ProgramKind::basic_text
        && application.kind != ProgramKind::basic_binary) {
        fail(ErrorCode::invalid_encoding, "unknown JR8APP program kind");
    }
    application.target_profile = reader.text("target profile");
    const auto name = reader.raw(reader.byte());
    application.name.assign(name.begin(), name.end());
    application.integrity_sha256 = reader.digest();
    if (application.kind == ProgramKind::machine_code) {
        application.entry_point = reader.u16();
        const auto count = linked::detail::read_count(reader, "segment count");
        for (std::uint32_t index = 0; index < count; ++index) {
            Segment segment;
            segment.kind = read_segment_kind(reader);
            segment.address = reader.u16();
            segment.logical_size = reader.u32();
            const auto stored_size = reader.u32();
            if (stored_size > 65'536U) {
                fail(ErrorCode::limit_exceeded, "JR8APP segment data exceeds the limit");
            }
            const auto data = reader.raw(stored_size);
            segment.data.assign(data.begin(), data.end());
            application.segments.push_back(std::move(segment));
        }
    } else {
        const auto length = reader.u32();
        if (length > 32'768U) {
            fail(ErrorCode::limit_exceeded, "JR8APP BASIC payload exceeds the limit");
        }
        const auto data = reader.raw(length);
        application.basic_data.assign(data.begin(), data.end());
    }
    if (!reader.at_end()) {
        fail(ErrorCode::trailing_data, "trailing data after JR8APP", reader.offset());
    }
    validate_structure(application);
    if (application.integrity_sha256 != compute_integrity(application)) {
        fail(ErrorCode::integrity_mismatch, "JR8APP integrity SHA-256 mismatch");
    }
    return application;
}

}  // namespace jr800::formats::jr8app
