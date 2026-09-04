// SPDX-License-Identifier: MIT

#include "jr800/linker/linker.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jr800::linker {
namespace {

struct Word {
    std::string text;
    std::size_t column{};
};

struct RegionRecord {
    MemoryRegion region;
    std::size_t line{};
    std::size_t column{};
};

struct PlacementRecord {
    SectionPlacement placement;
    std::size_t line{};
    std::size_t column{};
};

void diagnose(
    ScriptResult& result,
    const ScriptSource& source,
    std::string code,
    std::string message,
    std::size_t line,
    std::size_t column
) {
    result.diagnostics.push_back(Diagnostic{
        std::move(code),
        std::move(message),
        source.logical_path,
        line,
        column,
    });
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '\n' && text[index] != '\r') {
            continue;
        }
        lines.push_back(text.substr(begin, index - begin));
        if (text[index] == '\r' && index + 1U < text.size() && text[index + 1U] == '\n') {
            ++index;
        }
        begin = index + 1U;
    }
    if (begin < text.size() || text.empty()) {
        lines.push_back(text.substr(begin));
    }
    return lines;
}

std::vector<Word> tokenize_line(std::string_view line) {
    const auto comment = line.find(';');
    if (comment != std::string_view::npos) {
        line = line.substr(0, comment);
    }
    std::vector<Word> words;
    std::size_t index = 0;
    while (index < line.size()) {
        while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
            ++index;
        }
        if (index == line.size()) {
            break;
        }
        const auto begin = index;
        while (index < line.size() && line[index] != ' ' && line[index] != '\t') {
            ++index;
        }
        words.push_back(Word{std::string{line.substr(begin, index - begin)}, begin + 1U});
    }
    return words;
}

bool is_ascii_identifier(std::string_view value, bool allow_dot) {
    if (value.empty()) {
        return false;
    }
    const auto is_letter = [](char character) {
        return (character >= 'A' && character <= 'Z')
            || (character >= 'a' && character <= 'z');
    };
    const auto valid_first = [&](char character) {
        return is_letter(character) || character == '_'
            || (allow_dot && character == '.');
    };
    const auto valid_rest = [&](char character) {
        return valid_first(character) || (character >= '0' && character <= '9');
    };
    return valid_first(value.front())
        && std::all_of(value.begin() + 1, value.end(), valid_rest);
}

bool is_profile_identifier(std::string_view value) {
    if (value.empty() || value.front() < 'a' || value.front() > 'z') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9') || character == '_';
    });
}

std::optional<std::uint32_t> parse_number(std::string_view text) {
    int base = 10;
    if (!text.empty() && text.front() == '$') {
        base = 16;
        text.remove_prefix(1U);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

bool require_arity(
    ScriptResult& result,
    const ScriptSource& source,
    const std::vector<Word>& words,
    std::size_t expected,
    std::size_t line
) {
    if (words.size() == expected) {
        return true;
    }
    diagnose(
        result,
        source,
        "L1001",
        "wrong number of operands for " + words.front().text,
        line,
        words.front().column
    );
    return false;
}

}  // namespace

ScriptResult parse_script(const ScriptSource& source) {
    ScriptResult result;
    if (source.logical_path.empty()) {
        diagnose(result, source, "L1002", "script logical path must not be empty", 1, 1);
        return result;
    }

    LinkScript script;
    std::vector<RegionRecord> regions;
    std::vector<PlacementRecord> placements;
    std::unordered_map<std::string, std::size_t> region_names;
    std::unordered_map<std::string, std::size_t> placement_names;
    const auto lines = split_lines(source.text);
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const auto line_number = line_index + 1U;
        const auto words = tokenize_line(lines[line_index]);
        if (words.empty()) {
            continue;
        }
        const auto& command = words.front();
        if (command.text == "target") {
            if (!require_arity(result, source, words, 2U, line_number)) {
                continue;
            }
            if (!script.target_profile.empty()) {
                diagnose(
                    result,
                    source,
                    "L1003",
                    "target may be declared only once",
                    line_number,
                    command.column
                );
            } else if (!is_profile_identifier(words[1].text)) {
                diagnose(
                    result,
                    source,
                    "L1004",
                    "target must be a lower-case profile identifier",
                    line_number,
                    words[1].column
                );
            } else {
                script.target_profile = words[1].text;
            }
            continue;
        }
        if (command.text == "entry") {
            if (!require_arity(result, source, words, 2U, line_number)) {
                continue;
            }
            if (!script.entry_symbol.empty()) {
                diagnose(
                    result,
                    source,
                    "L1005",
                    "entry may be declared only once",
                    line_number,
                    command.column
                );
            } else if (!is_ascii_identifier(words[1].text, true)) {
                diagnose(
                    result,
                    source,
                    "L1006",
                    "entry must be a symbol identifier",
                    line_number,
                    words[1].column
                );
            } else {
                script.entry_symbol = words[1].text;
            }
            continue;
        }
        if (command.text == "region") {
            if (!require_arity(result, source, words, 4U, line_number)) {
                continue;
            }
            if (!is_ascii_identifier(words[1].text, false)) {
                diagnose(
                    result,
                    source,
                    "L1007",
                    "region name must be an identifier",
                    line_number,
                    words[1].column
                );
                continue;
            }
            if (region_names.contains(words[1].text)) {
                diagnose(
                    result,
                    source,
                    "L1008",
                    "duplicate region: " + words[1].text,
                    line_number,
                    words[1].column
                );
                continue;
            }
            const auto origin = parse_number(words[2].text);
            const auto length = parse_number(words[3].text);
            if (!origin.has_value() || *origin > 0xFFFFU) {
                diagnose(
                    result,
                    source,
                    "L1009",
                    "region origin must fit 16 bits",
                    line_number,
                    words[2].column
                );
                continue;
            }
            if (!length.has_value() || *length == 0U || *length > 65'536U
                || static_cast<std::uint64_t>(*origin) + *length > 65'536U) {
                diagnose(
                    result,
                    source,
                    "L1010",
                    "region length is empty or exceeds the address space",
                    line_number,
                    words[3].column
                );
                continue;
            }
            region_names.emplace(words[1].text, regions.size());
            regions.push_back(RegionRecord{
                MemoryRegion{
                    words[1].text,
                    static_cast<std::uint16_t>(*origin),
                    *length,
                },
                line_number,
                command.column,
            });
            continue;
        }
        if (command.text == "place") {
            if (!require_arity(result, source, words, 3U, line_number)) {
                continue;
            }
            if (!is_ascii_identifier(words[1].text, true)
                || !is_ascii_identifier(words[2].text, false)) {
                diagnose(
                    result,
                    source,
                    "L1011",
                    "place requires section and region identifiers",
                    line_number,
                    words[1].column
                );
                continue;
            }
            if (placement_names.contains(words[1].text)) {
                diagnose(
                    result,
                    source,
                    "L1012",
                    "duplicate section placement: " + words[1].text,
                    line_number,
                    words[1].column
                );
                continue;
            }
            placement_names.emplace(words[1].text, placements.size());
            placements.push_back(PlacementRecord{
                SectionPlacement{words[1].text, words[2].text},
                line_number,
                command.column,
            });
            continue;
        }
        diagnose(
            result,
            source,
            "L1013",
            "unknown link script command: " + command.text,
            line_number,
            command.column
        );
    }

    if (script.target_profile.empty()) {
        diagnose(result, source, "L1014", "link script requires target", 1, 1);
    }
    if (script.entry_symbol.empty()) {
        diagnose(result, source, "L1015", "link script requires entry", 1, 1);
    }
    if (regions.empty()) {
        diagnose(result, source, "L1016", "link script requires a memory region", 1, 1);
    }
    if (placements.empty()) {
        diagnose(result, source, "L1017", "link script requires a section placement", 1, 1);
    }

    for (std::size_t left_index = 0; left_index < regions.size(); ++left_index) {
        const auto left_begin = static_cast<std::uint32_t>(regions[left_index].region.origin);
        const auto left_end = left_begin + regions[left_index].region.length;
        for (std::size_t right_index = left_index + 1U; right_index < regions.size(); ++right_index) {
            const auto right_begin = static_cast<std::uint32_t>(regions[right_index].region.origin);
            const auto right_end = right_begin + regions[right_index].region.length;
            if (left_begin < right_end && right_begin < left_end) {
                diagnose(
                    result,
                    source,
                    "L1018",
                    "memory regions overlap: " + regions[left_index].region.name + " and "
                        + regions[right_index].region.name,
                    regions[right_index].line,
                    regions[right_index].column
                );
            }
        }
    }
    for (const auto& placement : placements) {
        if (!region_names.contains(placement.placement.region_name)) {
            diagnose(
                result,
                source,
                "L1019",
                "placement references unknown region: " + placement.placement.region_name,
                placement.line,
                placement.column
            );
        }
    }

    if (!result.diagnostics.empty()) {
        return result;
    }
    for (const auto& region : regions) {
        script.regions.push_back(region.region);
    }
    for (const auto& placement : placements) {
        script.placements.push_back(placement.placement);
    }
    result.script = std::move(script);
    return result;
}

}  // namespace jr800::linker
