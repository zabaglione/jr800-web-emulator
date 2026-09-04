// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "jr800/formats/jro.hpp"

namespace jr800::assembler {

struct Source {
    std::string logical_path;
    std::string text;
};

struct Options {
    std::string target_profile;
    std::string producer_version;
};

struct Diagnostic {
    std::string code;
    std::string message;
    std::string path;
    std::size_t line{};
    std::size_t column{};
};

struct Output {
    formats::jro::ObjectFile object;
    std::string listing;
};

struct Result {
    std::optional<Output> output;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept {
        return output.has_value();
    }
};

[[nodiscard]] Result assemble(const Source& source, const Options& options);

}  // namespace jr800::assembler
