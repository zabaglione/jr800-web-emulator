// SPDX-License-Identifier: MIT

#include "jr800/formats/linked_error.hpp"

#include <utility>

namespace jr800::formats::linked {

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

}  // namespace jr800::formats::linked
