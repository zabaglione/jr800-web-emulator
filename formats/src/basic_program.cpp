// SPDX-License-Identifier: MIT
#include "jr800/formats/basic_program.hpp"
#include "jr800/formats/linked_error.hpp"
#include <cstddef>

namespace jr800::formats {
namespace {
[[noreturn]] void invalid() {
    throw linked::Error(linked::ErrorCode::invalid_value, "Invalid native BASIC program", std::nullopt);
}
void add_line(std::vector<std::uint16_t>& lines, std::uint32_t number) {
    if (number == 0U || number > 65535U || (!lines.empty() && number <= lines.back())) invalid();
    lines.push_back(static_cast<std::uint16_t>(number));
}
std::uint16_t word(std::span<const std::uint8_t> bytes, std::size_t at) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[at]) << 8U) | bytes[at + 1U]);
}
}
std::vector<std::uint16_t> basic_text_line_numbers(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > 32768U) invalid();
    std::vector<std::uint16_t> lines;
    std::size_t start = 0U;
    while (start < bytes.size()) {
        auto end = start;
        while (end < bytes.size() && bytes[end] != 13U) {
            if (bytes[end] < 32U || end - start >= 255U) invalid();
            ++end;
        }
        if (end == bytes.size()) invalid();
        auto at = start;
        std::uint32_t number = 0U;
        while (at < end && bytes[at] >= '0' && bytes[at] <= '9') {
            number = number * 10U + bytes[at++] - '0';
            if (number > 65535U) invalid();
        }
        if (at == start) invalid();
        while (at < end && bytes[at] == ' ') ++at;
        if (at == end) invalid();
        add_line(lines, number);
        start = end + 1U;
    }
    return lines;
}
std::vector<std::uint16_t> basic_binary_line_numbers(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 2U || bytes.size() > 32768U) invalid();
    std::vector<std::uint16_t> lines;
    std::size_t at = 0U;
    while (at + 2U <= bytes.size()) {
        const auto length = word(bytes, at);
        if (length == 0U) {
            if (at + 2U != bytes.size()) invalid();
            return lines;
        }
        if (length < 6U || length > bytes.size() - at
            || bytes[at + length - 1U] != 0U) invalid();
        add_line(lines, word(bytes, at + 2U));
        at += length;
    }
    invalid();
}
}
