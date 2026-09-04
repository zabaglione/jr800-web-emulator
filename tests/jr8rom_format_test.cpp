// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/formats/jr8rom.hpp"
#include "jr800/formats/linked_error.hpp"

namespace {

using jr800::formats::linked::ErrorCode;

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

template <typename Callable>
bool expect_error(Callable&& callable, ErrorCode expected, std::string_view message) {
    try {
        std::forward<Callable>(callable)();
    } catch (const jr800::formats::linked::Error& error) {
        if (error.code() == expected) {
            return true;
        }
        std::cerr << message << ": unexpected error code\n";
        return false;
    }
    std::cerr << message << ": no error\n";
    return false;
}

jr800::formats::jr8rom::Image make_image() {
    using namespace jr800::formats::jr8rom;

    Image image;
    image.segments = {
        Segment{0xF000U, {0x31U, 0x32U}},
        Segment{0x8000U, {0x11U, 0x12U}},
    };
    image.integrity_sha256 = compute_integrity(image);
    return image;
}

}  // namespace

int main() {
    using namespace jr800::formats;
    using namespace jr800::formats::jr8rom;

    bool passed = true;
    const auto image = make_image();
    const auto bytes = write(image);

    auto canonical_image = image;
    std::reverse(canonical_image.segments.begin(), canonical_image.segments.end());
    passed &= expect(
        read(bytes) == canonical_image,
        "JR8ROM semantic round trip differs"
    );
    passed &= expect(
        write(canonical_image) == bytes,
        "JR8ROM serialization is not canonical"
    );
    passed &= expect(
        bytes.size() == 68U
            && std::equal(bytes.begin(), bytes.begin() + 6, "JR8ROM")
            && bytes[8U] == 0U && bytes[9U] == 1U
            && bytes[10U] == 0U && bytes[11U] == 0U
            && bytes[48U] == 0U && bytes[49U] == 0U
            && bytes[50U] == 0U && bytes[51U] == 2U
            && bytes[52U] == 0x80U && bytes[53U] == 0x00U
            && bytes[60U] == 0xF0U && bytes[61U] == 0x00U,
        "JR8ROM canonical binary layout differs"
    );

    auto tampered = bytes;
    tampered.back() ^= 0x01U;
    passed &= expect_error(
        [&] { static_cast<void>(read(tampered)); },
        ErrorCode::integrity_mismatch,
        "JR8ROM payload tamper was not detected"
    );
    for (std::size_t length = 0; length < bytes.size(); ++length) {
        passed &= expect_error(
            [&] { static_cast<void>(read(std::span{bytes}.first(length))); },
            ErrorCode::truncated,
            "JR8ROM truncation was not rejected"
        );
    }

    auto bad_magic = bytes;
    bad_magic.front() ^= 0xFFU;
    passed &= expect_error(
        [&] { static_cast<void>(read(bad_magic)); },
        ErrorCode::invalid_magic,
        "JR8ROM bad magic was not rejected"
    );
    auto bad_version = bytes;
    bad_version[9U] = 2U;
    passed &= expect_error(
        [&] { static_cast<void>(read(bad_version)); },
        ErrorCode::unsupported_version,
        "JR8ROM unsupported version was not rejected"
    );
    auto bad_flags = bytes;
    bad_flags[15U] = 1U;
    passed &= expect_error(
        [&] { static_cast<void>(read(bad_flags)); },
        ErrorCode::invalid_encoding,
        "JR8ROM nonzero flags were not rejected"
    );
    auto trailing = bytes;
    trailing.push_back(0U);
    passed &= expect_error(
        [&] { static_cast<void>(read(trailing)); },
        ErrorCode::trailing_data,
        "JR8ROM trailing data was not rejected"
    );

    auto noncanonical = bytes;
    std::swap_ranges(
        noncanonical.begin() + 52,
        noncanonical.begin() + 60,
        noncanonical.begin() + 60
    );
    passed &= expect_error(
        [&] { static_cast<void>(read(noncanonical)); },
        ErrorCode::invalid_encoding,
        "JR8ROM noncanonical segment order was accepted"
    );

    Image empty;
    passed &= expect_error(
        [&] { static_cast<void>(compute_integrity(empty)); },
        ErrorCode::invalid_value,
        "JR8ROM empty image was accepted"
    );
    Image empty_segment;
    empty_segment.segments = {Segment{0x8000U, {}}};
    passed &= expect_error(
        [&] { static_cast<void>(compute_integrity(empty_segment)); },
        ErrorCode::invalid_value,
        "JR8ROM empty segment was accepted"
    );
    Image overlap;
    overlap.segments = {
        Segment{0x8000U, {0x01U, 0x02U}},
        Segment{0x8001U, {0x03U, 0x04U}},
    };
    passed &= expect_error(
        [&] { static_cast<void>(compute_integrity(overlap)); },
        ErrorCode::invalid_value,
        "JR8ROM overlapping segments were accepted"
    );
    Image overflow;
    overflow.segments = {Segment{0xFFFFU, {0x01U, 0x02U}}};
    passed &= expect_error(
        [&] { static_cast<void>(compute_integrity(overflow)); },
        ErrorCode::invalid_value,
        "JR8ROM overflowing segment was accepted"
    );

    Image adjacent;
    adjacent.segments = {
        Segment{0x8000U, {0x01U, 0x02U}},
        Segment{0x8002U, {0x03U}},
    };
    adjacent.integrity_sha256 = compute_integrity(adjacent);
    passed &= expect(
        read(write(adjacent)) == adjacent,
        "JR8ROM adjacent segment boundary was not preserved"
    );
    passed &= expect(
        extract_range(adjacent, 0x8001U, 2U)
            == std::optional<std::vector<std::uint8_t>>{{0x02U, 0x03U}},
        "JR8ROM extraction did not cross an adjacent boundary"
    );
    passed &= expect(
        extract_range(adjacent, 0x8000U, 1U)
            == std::optional<std::vector<std::uint8_t>>{{0x01U}},
        "JR8ROM extraction did not retain a partial segment"
    );
    passed &= expect(
        !extract_range(adjacent, 0x7FFFU, 2U).has_value(),
        "JR8ROM extraction filled a leading gap"
    );
    passed &= expect(
        !extract_range(adjacent, 0x8000U, 4U).has_value(),
        "JR8ROM extraction filled a trailing gap"
    );
    passed &= expect_error(
        [&] { static_cast<void>(extract_range(adjacent, 0x8000U, 0U)); },
        ErrorCode::invalid_value,
        "JR8ROM zero-size extraction was accepted"
    );
    passed &= expect_error(
        [&] { static_cast<void>(extract_range(adjacent, 0xFFFFU, 2U)); },
        ErrorCode::invalid_value,
        "JR8ROM overflowing extraction was accepted"
    );

    auto stale_adjacent = adjacent;
    stale_adjacent.segments.front().data.front() ^= 0xFFU;
    passed &= expect_error(
        [&] {
            static_cast<void>(extract_range(stale_adjacent, 0x8000U, 1U));
        },
        ErrorCode::integrity_mismatch,
        "JR8ROM extraction accepted a stale integrity digest"
    );

    Image full_address_space;
    full_address_space.segments = {
        Segment{0x0000U, std::vector<std::uint8_t>(65'536U, 0xA5U)},
    };
    full_address_space.integrity_sha256 = compute_integrity(full_address_space);
    passed &= expect(
        read(write(full_address_space)) == full_address_space,
        "JR8ROM full address-space segment was rejected"
    );
    passed &= expect(
        extract_range(full_address_space, 0U, 65'536U)
            == std::optional<std::vector<std::uint8_t>>{
                std::vector<std::uint8_t>(65'536U, 0xA5U)
            },
        "JR8ROM full address-space extraction failed"
    );

    auto wrong_digest = canonical_image;
    wrong_digest.integrity_sha256.front() ^= 0x01U;
    passed &= expect_error(
        [&] { static_cast<void>(write(wrong_digest)); },
        ErrorCode::integrity_mismatch,
        "JR8ROM writer accepted a stale integrity digest"
    );

    return passed ? 0 : 1;
}
