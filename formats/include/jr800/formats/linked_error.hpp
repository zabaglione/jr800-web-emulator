// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace jr800::formats::linked {

enum class ErrorCode : std::uint8_t {
    invalid_magic,
    unsupported_version,
    truncated,
    invalid_encoding,
    invalid_value,
    invalid_reference,
    integrity_mismatch,
    limit_exceeded,
    trailing_data,
};

class Error final : public std::runtime_error {
public:
    Error(ErrorCode code, std::string message, std::optional<std::size_t> byte_offset);

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] std::optional<std::size_t> byte_offset() const noexcept;

private:
    ErrorCode code_;
    std::optional<std::size_t> byte_offset_;
};

}  // namespace jr800::formats::linked
