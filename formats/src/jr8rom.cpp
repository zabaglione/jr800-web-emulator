// SPDX-License-Identifier: MIT

#include "jr800/formats/jr8rom.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "linked_io.hpp"

namespace jr800::formats::jr8rom {
namespace {

using linked::ErrorCode;
using linked::detail::Reader;
using linked::detail::Writer;
using linked::detail::fail;

constexpr std::array<std::uint8_t, 8> kMagic{
    0x4A, 0x52, 0x38, 0x52, 0x4F, 0x4D, 0x00, 0x00,
};
constexpr std::string_view kIntegrityDomain = "JR8ROM-INTEGRITY-V1";

std::vector<Segment> canonical_segments(const Image& image) {
    auto segments = image.segments;
    std::sort(
        segments.begin(),
        segments.end(),
        [](const Segment& left, const Segment& right) {
            return left.address < right.address;
        }
    );
    return segments;
}

void validate_structure(const Image& image) {
    linked::detail::validate_count(image.segments.size(), "segment count");
    if (image.segments.empty()) {
        fail(ErrorCode::invalid_value, "JR8ROM must contain at least one segment");
    }

    const auto segments = canonical_segments(image);
    std::uint64_t previous_end = 0U;
    bool first = true;
    for (const auto& segment : segments) {
        if (segment.data.empty()) {
            fail(ErrorCode::invalid_value, "JR8ROM segment size must be nonzero");
        }
        const auto begin = static_cast<std::uint64_t>(segment.address);
        const auto end = begin + segment.data.size();
        if (end > maximum_segment_size) {
            fail(
                ErrorCode::invalid_value,
                "JR8ROM segment exceeds the 16-bit address space"
            );
        }
        if (!first && begin < previous_end) {
            fail(ErrorCode::invalid_value, "JR8ROM segments must not overlap");
        }
        first = false;
        previous_end = end;
    }
}

}  // namespace

Sha256Digest compute_integrity(const Image& image) {
    validate_structure(image);
    const auto segments = canonical_segments(image);

    Writer writer{"JR8ROM integrity material"};
    writer.raw(std::span{
        reinterpret_cast<const std::uint8_t*>(kIntegrityDomain.data()),
        kIntegrityDomain.size(),
    });
    writer.byte(0U);
    writer.u32(static_cast<std::uint32_t>(segments.size()));
    for (const auto& segment : segments) {
        writer.u16(segment.address);
        writer.u32(static_cast<std::uint32_t>(segment.data.size()));
        writer.raw(segment.data);
    }
    const auto material = std::move(writer).finish();
    return sha256(material);
}

std::vector<std::uint8_t> write(const Image& image) {
    validate_structure(image);
    if (image.integrity_sha256 != compute_integrity(image)) {
        fail(ErrorCode::integrity_mismatch, "JR8ROM integrity SHA-256 mismatch");
    }
    const auto segments = canonical_segments(image);

    Writer writer{"JR8ROM"};
    writer.raw(kMagic);
    writer.u16(format_major_version);
    writer.u16(format_minor_version);
    writer.u32(0U);
    writer.digest(image.integrity_sha256);
    writer.u32(static_cast<std::uint32_t>(segments.size()));
    for (const auto& segment : segments) {
        writer.u16(segment.address);
        writer.u32(static_cast<std::uint32_t>(segment.data.size()));
        writer.raw(segment.data);
    }
    return std::move(writer).finish();
}

Image read(std::span<const std::uint8_t> bytes) {
    Reader reader{bytes, "JR8ROM"};
    const auto magic = reader.raw(kMagic.size());
    if (!std::equal(magic.begin(), magic.end(), kMagic.begin())) {
        fail(ErrorCode::invalid_magic, "invalid JR8ROM magic", 0U);
    }
    const auto version_offset = reader.offset();
    if (reader.u16() != format_major_version
        || reader.u16() != format_minor_version) {
        fail(
            ErrorCode::unsupported_version,
            "unsupported JR8ROM version",
            version_offset
        );
    }
    const auto flags_offset = reader.offset();
    if (reader.u32() != 0U) {
        fail(ErrorCode::invalid_encoding, "nonzero JR8ROM header flags", flags_offset);
    }

    Image image;
    image.integrity_sha256 = reader.digest();
    const auto segment_count = linked::detail::read_count(reader, "segment count");
    image.segments.reserve(segment_count);
    std::uint16_t previous_address = 0U;
    for (std::uint32_t index = 0; index < segment_count; ++index) {
        const auto address_offset = reader.offset();
        Segment segment;
        segment.address = reader.u16();
        if (index != 0U && segment.address <= previous_address) {
            fail(
                ErrorCode::invalid_encoding,
                "JR8ROM segments are not in canonical address order",
                address_offset
            );
        }
        previous_address = segment.address;

        const auto size_offset = reader.offset();
        const auto size = reader.u32();
        if (size > maximum_segment_size) {
            fail(
                ErrorCode::limit_exceeded,
                "JR8ROM segment data exceeds the address-space limit",
                size_offset
            );
        }
        const auto data = reader.raw(size);
        segment.data.assign(data.begin(), data.end());
        image.segments.push_back(std::move(segment));
    }
    if (!reader.at_end()) {
        fail(ErrorCode::trailing_data, "trailing data after JR8ROM", reader.offset());
    }
    validate_structure(image);
    if (image.integrity_sha256 != compute_integrity(image)) {
        fail(ErrorCode::integrity_mismatch, "JR8ROM integrity SHA-256 mismatch");
    }
    return image;
}

std::optional<std::vector<std::uint8_t>> extract_range(
    const Image& image,
    std::uint16_t address,
    std::size_t size
) {
    validate_structure(image);
    if (image.integrity_sha256 != compute_integrity(image)) {
        fail(ErrorCode::integrity_mismatch, "JR8ROM integrity SHA-256 mismatch");
    }
    if (size == 0U) {
        fail(ErrorCode::invalid_value, "JR8ROM extraction size must be nonzero");
    }

    const auto range_begin = static_cast<std::size_t>(address);
    const auto range_end = range_begin + size;
    if (range_end > maximum_segment_size) {
        fail(
            ErrorCode::invalid_value,
            "JR8ROM extraction range exceeds the 16-bit address space"
        );
    }

    const auto segments = canonical_segments(image);
    auto cursor = range_begin;
    std::vector<std::uint8_t> result;
    result.reserve(size);
    for (const auto& segment : segments) {
        const auto segment_begin = static_cast<std::size_t>(segment.address);
        const auto segment_end = segment_begin + segment.data.size();
        if (segment_end <= cursor) {
            continue;
        }
        if (segment_begin > cursor) {
            return std::nullopt;
        }

        const auto copy_end = std::min(range_end, segment_end);
        const auto offset = cursor - segment_begin;
        const auto count = copy_end - cursor;
        result.insert(
            result.end(),
            segment.data.begin() + static_cast<std::ptrdiff_t>(offset),
            segment.data.begin() + static_cast<std::ptrdiff_t>(offset + count)
        );
        cursor = copy_end;
        if (cursor == range_end) {
            return result;
        }
    }
    return std::nullopt;
}

}  // namespace jr800::formats::jr8rom
