// SPDX-License-Identifier: MIT

#include "jr800/formats/jr8app.hpp"

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
    linked::detail::validate_count(application.segments.size(), "segment count");
    if (application.segments.empty()) {
        fail(ErrorCode::invalid_value, "JR8APP must contain at least one segment");
    }

    const auto segments = canonical_segments(application);
    std::uint64_t previous_end = 0U;
    bool first = true;
    bool entry_is_loaded = false;
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
        if (application.entry_point >= begin && application.entry_point < end) {
            entry_is_loaded = true;
        }
    }
    if (!entry_is_loaded) {
        fail(ErrorCode::invalid_value, "entry point is outside all segments");
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

Sha256Digest compute_integrity(const Application& application) {
    validate_structure(application);
    const auto segments = canonical_segments(application);
    Writer writer{"JR8APP integrity material"};
    writer.raw(std::span{
        reinterpret_cast<const std::uint8_t*>(kIntegrityDomain.data()),
        kIntegrityDomain.size(),
    });
    writer.byte(0U);
    writer.text(application.target_profile);
    writer.u16(application.entry_point);
    writer.u32(static_cast<std::uint32_t>(segments.size()));
    for (const auto& segment : segments) {
        writer.byte(static_cast<std::uint8_t>(segment.kind));
        writer.u16(segment.address);
        writer.u32(segment.logical_size);
        writer.raw(segment.data);
    }
    const auto material = std::move(writer).finish();
    return sha256(material);
}

std::vector<std::uint8_t> write(const Application& application) {
    validate_structure(application);
    const auto expected_integrity = compute_integrity(application);
    if (application.integrity_sha256 != expected_integrity) {
        fail(ErrorCode::integrity_mismatch, "JR8APP integrity SHA-256 mismatch");
    }
    const auto segments = canonical_segments(application);

    Writer writer{"JR8APP"};
    writer.raw(kMagic);
    writer.u16(format_major_version);
    writer.u16(format_minor_version);
    writer.u32(0U);
    writer.text(application.target_profile);
    writer.u16(application.entry_point);
    writer.u16(0U);
    writer.digest(application.integrity_sha256);
    writer.u32(static_cast<std::uint32_t>(segments.size()));
    for (const auto& segment : segments) {
        writer.byte(static_cast<std::uint8_t>(segment.kind));
        writer.byte(0U);
        writer.u16(segment.address);
        writer.u32(segment.logical_size);
        writer.u32(static_cast<std::uint32_t>(segment.data.size()));
        writer.raw(segment.data);
    }
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
    const auto flags_offset = reader.offset();
    if (reader.u32() != 0U) {
        fail(ErrorCode::invalid_encoding, "nonzero JR8APP header flags", flags_offset);
    }

    Application application;
    application.target_profile = reader.text("target profile");
    application.entry_point = reader.u16();
    const auto reserved_offset = reader.offset();
    if (reader.u16() != 0U) {
        fail(ErrorCode::invalid_encoding, "nonzero JR8APP reserved field", reserved_offset);
    }
    application.integrity_sha256 = reader.digest();
    const auto segment_count = linked::detail::read_count(reader, "segment count");
    application.segments.reserve(segment_count);
    for (std::uint32_t index = 0; index < segment_count; ++index) {
        Segment segment;
        segment.kind = read_segment_kind(reader);
        const auto segment_reserved_offset = reader.offset();
        if (reader.byte() != 0U) {
            fail(
                ErrorCode::invalid_encoding,
                "nonzero JR8APP segment reserved field",
                segment_reserved_offset
            );
        }
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
