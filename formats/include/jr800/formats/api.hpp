// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace jr800::formats {

struct ApiVersion {
    std::uint16_t major;
    std::uint16_t minor;
};

inline constexpr ApiVersion api_version{0, 1};

}  // namespace jr800::formats
