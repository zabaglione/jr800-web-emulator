// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/formats/linked_error.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::formats::linked::detail {

inline constexpr std::size_t max_file_size = 64U * 1024U * 1024U;
inline constexpr std::size_t max_string_size = 65'535U;
inline constexpr std::size_t max_record_count = 65'535U;

[[noreturn]] inline void fail(
    ErrorCode code,
    std::string message,
    std::optional<std::size_t> byte_offset = std::nullopt
) {
    throw Error(code, std::move(message), byte_offset);
}

inline bool is_valid_utf8(std::string_view text) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = bytes[index];
        if (first == 0U) {
            return false;
        }
        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        const auto continuation = [&](std::size_t position) {
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
        if ((first >= 0xE1U && first <= 0xECU)
            || (first >= 0xEEU && first <= 0xEFU)) {
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

inline void validate_text(
    std::string_view text,
    std::string_view field,
    bool allow_empty = false,
    std::optional<std::size_t> byte_offset = std::nullopt
) {
    if (!allow_empty && text.empty()) {
        fail(ErrorCode::invalid_value, std::string(field) + " must not be empty", byte_offset);
    }
    if (text.size() > max_string_size) {
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

inline void validate_profile_identifier(std::string_view profile) {
    validate_text(profile, "target profile");
    const auto valid_first = [](char value) { return value >= 'a' && value <= 'z'; };
    const auto valid_rest = [&](char value) {
        return valid_first(value) || (value >= '0' && value <= '9') || value == '_';
    };
    if (!valid_first(profile.front())
        || !std::all_of(profile.begin() + 1, profile.end(), valid_rest)) {
        fail(ErrorCode::invalid_value, "target profile must be a lower-case identifier");
    }
}

inline void validate_count(std::size_t count, std::string_view field) {
    if (count > max_record_count) {
        fail(ErrorCode::limit_exceeded, std::string(field) + " exceeds the record limit");
    }
}

class Writer {
public:
    explicit Writer(std::string_view format_name) : format_name_(format_name) {}

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

    void digest(const Sha256Digest& value) {
        raw(value);
    }

    [[nodiscard]] std::vector<std::uint8_t> finish() && {
        return std::move(bytes_);
    }

private:
    void ensure_capacity(std::size_t additional) const {
        if (additional > max_file_size - bytes_.size()) {
            fail(
                ErrorCode::limit_exceeded,
                std::string(format_name_) + " exceeds the file size limit"
            );
        }
    }

    std::string_view format_name_;
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    Reader(std::span<const std::uint8_t> bytes, std::string_view format_name)
        : bytes_(bytes), format_name_(format_name) {
        if (bytes.size() > max_file_size) {
            fail(
                ErrorCode::limit_exceeded,
                std::string(format_name_) + " exceeds the file size limit",
                0U
            );
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

    [[nodiscard]] std::span<const std::uint8_t> raw(std::size_t size) {
        require(size);
        const auto result = bytes_.subspan(offset_, size);
        offset_ += size;
        return result;
    }

    [[nodiscard]] std::string text(std::string_view field) {
        const auto length_offset = offset_;
        const auto length = u32();
        if (length > max_string_size) {
            fail(ErrorCode::limit_exceeded, std::string(field) + " is too long", length_offset);
        }
        const auto text_offset = offset_;
        const auto values = raw(length);
        std::string result(reinterpret_cast<const char*>(values.data()), values.size());
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
            fail(
                ErrorCode::truncated,
                "truncated " + std::string(format_name_) + " input",
                offset_
            );
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::string_view format_name_;
    std::size_t offset_{};
};

inline std::uint32_t read_count(Reader& reader, std::string_view field) {
    const auto offset = reader.offset();
    const auto count = reader.u32();
    if (count > max_record_count) {
        fail(ErrorCode::limit_exceeded, std::string(field) + " exceeds the record limit", offset);
    }
    return count;
}

}  // namespace jr800::formats::linked::detail
