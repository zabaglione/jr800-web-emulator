// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace jr800::formats {

using Sha256Digest = std::array<std::uint8_t, 32>;

[[nodiscard]] Sha256Digest sha256(std::span<const std::uint8_t> input);

}  // namespace jr800::formats
