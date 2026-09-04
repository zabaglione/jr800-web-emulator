// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/jro.hpp"

namespace jr800::linker {

struct ScriptSource {
    std::string logical_path;
    std::string text;
};

struct MemoryRegion {
    std::string name;
    std::uint16_t origin{};
    std::uint32_t length{};
};

struct SectionPlacement {
    std::string section_name;
    std::string region_name;
};

struct LinkScript {
    std::string target_profile;
    std::string entry_symbol;
    std::vector<MemoryRegion> regions;
    std::vector<SectionPlacement> placements;
};

struct Diagnostic {
    std::string code;
    std::string message;
    std::string path;
    std::size_t line{};
    std::size_t column{};
};

struct ScriptResult {
    std::optional<LinkScript> script;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept {
        return script.has_value();
    }
};

[[nodiscard]] ScriptResult parse_script(const ScriptSource& source);

struct InputObject {
    std::string logical_path;
    formats::jro::ObjectFile object;
};

struct Options {
    std::string producer_version;
};

struct Output {
    formats::jr8app::Application application;
    formats::jr8dbg::DebugInfo debug_info;
    std::string link_map;
    std::string symbol_output;
};

struct Result {
    std::optional<Output> output;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept {
        return output.has_value();
    }
};

[[nodiscard]] Result link_objects(
    const std::vector<InputObject>& inputs,
    const LinkScript& script,
    const Options& options
);

}  // namespace jr800::linker
