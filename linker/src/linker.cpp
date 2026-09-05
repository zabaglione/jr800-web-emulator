// SPDX-License-Identifier: MIT

#include "jr800/linker/linker.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "jr800/formats/linked_error.hpp"

namespace jr800::linker {
namespace {

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

struct RegionState {
    std::size_t script_index{};
    std::uint32_t cursor{};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> intervals;
};

struct PlacedSection {
    std::size_t object_index{};
    std::size_t section_index{};
    std::size_t region_index{};
    std::uint32_t address{};
    std::vector<std::uint8_t> data;
};

struct ResolvedSymbol {
    std::uint32_t value{};
    bool address{};
    std::size_t object_index{};
    std::size_t symbol_index{};
};

std::uint64_t region_end(const MemoryRegion& region) {
    return static_cast<std::uint64_t>(region.origin) + region.length;
}

std::uint64_t align_up(std::uint32_t value, std::uint32_t alignment) {
    return (static_cast<std::uint64_t>(value) + alignment - 1U)
        & ~(static_cast<std::uint64_t>(alignment) - 1U);
}

bool ranges_overlap(
    std::uint32_t left_begin,
    std::uint32_t left_end,
    std::uint32_t right_begin,
    std::uint32_t right_end
) {
    return left_begin < right_end && right_begin < left_end;
}

std::string hex_address(std::uint32_t value) {
    std::ostringstream stream;
    stream << '$' << std::uppercase << std::hex << std::setw(4)
           << std::setfill('0') << value;
    return stream.str();
}

bool debug_symbol_less(
    const formats::jr8dbg::Symbol& left,
    const formats::jr8dbg::Symbol& right
) {
    return std::tie(
               left.value,
               left.kind,
               left.name,
               left.binding,
               left.source_file_index,
               left.size
           )
        < std::tie(
               right.value,
               right.kind,
               right.name,
               right.binding,
               right.source_file_index,
               right.size
           );
}

bool debug_line_less(
    const formats::jr8dbg::LineMapping& left,
    const formats::jr8dbg::LineMapping& right
) {
    return std::tie(
               left.address,
               left.length,
               left.source_file_index,
               left.line,
               left.column
           )
        < std::tie(
               right.address,
               right.length,
               right.source_file_index,
               right.line,
               right.column
           );
}

class LinkEngine {
public:
    LinkEngine(
        const std::vector<InputObject>& inputs,
        const LinkScript& script,
        const Options& options,
        std::vector<Diagnostic>& diagnostics
    )
        : inputs_(inputs),
          script_(script),
          options_(options),
          diagnostics_(diagnostics) {}

    std::optional<Output> run() {
        validate_model();
        validate_inputs();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        place_sections();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        resolve_defined_symbols();
        resolve_entry();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        apply_relocations();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        auto output = build_output();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        return output;
    }

private:
    void diagnose(
        std::string path,
        std::string code,
        std::string message,
        std::size_t line = 0,
        std::size_t column = 0
    ) {
        diagnostics_.push_back(Diagnostic{
            std::move(code),
            std::move(message),
            std::move(path),
            line,
            column,
        });
    }

    void validate_model() {
        if (script_.target_profile.empty() || script_.entry_symbol.empty()
            || script_.regions.empty() || script_.placements.empty()) {
            diagnose("<link>", "L2001", "link script model is incomplete");
            return;
        }
        if (options_.producer_version.empty()) {
            diagnose("<link>", "L2002", "producer version must not be empty");
        }

        std::unordered_set<std::string> region_names;
        for (std::size_t index = 0; index < script_.regions.size(); ++index) {
            const auto& region = script_.regions[index];
            if (region.name.empty() || region.length == 0U || region_end(region) > 65'536U
                || !region_names.insert(region.name).second) {
                diagnose("<link>", "L2003", "invalid or duplicate memory region");
            }
            for (std::size_t other = 0; other < index; ++other) {
                const auto& previous = script_.regions[other];
                if (ranges_overlap(
                        region.origin,
                        static_cast<std::uint32_t>(region_end(region)),
                        previous.origin,
                        static_cast<std::uint32_t>(region_end(previous)))) {
                    diagnose("<link>", "L2004", "memory regions overlap");
                }
            }
        }

        std::unordered_set<std::string> section_names;
        for (const auto& placement : script_.placements) {
            if (placement.section_name.empty() || placement.region_name.empty()
                || !section_names.insert(placement.section_name).second
                || !region_names.contains(placement.region_name)) {
                diagnose("<link>", "L2005", "invalid section placement");
            }
        }
    }

    void validate_inputs() {
        if (inputs_.empty()) {
            diagnose("<link>", "L2006", "at least one input object is required");
            return;
        }
        std::unordered_set<std::string> input_paths;
        for (const auto& input : inputs_) {
            if (input.logical_path.empty() || !input_paths.insert(input.logical_path).second) {
                diagnose(
                    input.logical_path.empty() ? "<link>" : input.logical_path,
                    "L2007",
                    "input object paths must be nonempty and unique"
                );
            }
            try {
                static_cast<void>(formats::jro::write(input.object));
            } catch (const formats::jro::Error& error) {
                diagnose(
                    input.logical_path,
                    "L2008",
                    std::string("invalid JRO input: ") + error.what()
                );
            }
            if (input.object.target_profile != script_.target_profile) {
                diagnose(
                    input.logical_path,
                    "L2009",
                    "JRO target profile does not match the link script"
                );
            }
        }
    }

    void place_sections() {
        std::unordered_map<std::string, std::size_t> region_indexes;
        region_states_.reserve(script_.regions.size());
        for (std::size_t index = 0; index < script_.regions.size(); ++index) {
            region_indexes.emplace(script_.regions[index].name, index);
            region_states_.push_back(RegionState{
                index,
                script_.regions[index].origin,
                {},
            });
        }
        std::unordered_map<std::string, std::size_t> placement_regions;
        for (const auto& placement : script_.placements) {
            placement_regions.emplace(
                placement.section_name,
                region_indexes.at(placement.region_name)
            );
        }

        placed_lookup_.resize(inputs_.size());
        for (std::size_t object_index = 0; object_index < inputs_.size(); ++object_index) {
            const auto& input = inputs_[object_index];
            placed_lookup_[object_index].assign(input.object.sections.size(), kNoIndex);
            for (std::size_t section_index = 0;
                 section_index < input.object.sections.size();
                 ++section_index) {
                const auto& section = input.object.sections[section_index];
                const auto placement = placement_regions.find(section.name);
                if (placement == placement_regions.end()) {
                    diagnose(
                        input.logical_path,
                        "L2101",
                        "no placement for section: " + section.name
                    );
                    continue;
                }
                auto& state = region_states_[placement->second];
                const auto& region = script_.regions[state.script_index];
                std::uint64_t address = 0U;
                if (section.fixed_address.has_value()) {
                    address = *section.fixed_address;
                    if (address % section.alignment != 0U) {
                        diagnose(
                            input.logical_path,
                            "L2102",
                            "fixed section address violates alignment: " + section.name
                        );
                        continue;
                    }
                } else {
                    address = align_up(state.cursor, section.alignment);
                }
                const auto end = address + section.logical_size;
                if (address < region.origin || end > region_end(region)) {
                    diagnose(
                        input.logical_path,
                        "L2103",
                        "section does not fit region " + region.name + ": " + section.name
                    );
                    continue;
                }
                bool overlaps = false;
                if (section.logical_size != 0U) {
                    for (const auto& interval : state.intervals) {
                        if (ranges_overlap(
                                static_cast<std::uint32_t>(address),
                                static_cast<std::uint32_t>(end),
                                interval.first,
                                interval.second)) {
                            overlaps = true;
                            break;
                        }
                    }
                }
                if (overlaps) {
                    diagnose(
                        input.logical_path,
                        "L2104",
                        "section overlaps another section: " + section.name
                    );
                    continue;
                }
                if (section.logical_size != 0U) {
                    state.intervals.emplace_back(
                        static_cast<std::uint32_t>(address),
                        static_cast<std::uint32_t>(end)
                    );
                }
                state.cursor = std::max(state.cursor, static_cast<std::uint32_t>(end));
                placed_lookup_[object_index][section_index] = placed_sections_.size();
                placed_sections_.push_back(PlacedSection{
                    object_index,
                    section_index,
                    placement->second,
                    static_cast<std::uint32_t>(address),
                    section.data,
                });
            }
        }
    }

    void resolve_defined_symbols() {
        resolved_symbols_.resize(inputs_.size());
        for (std::size_t object_index = 0; object_index < inputs_.size(); ++object_index) {
            const auto& input = inputs_[object_index];
            auto& resolved = resolved_symbols_[object_index];
            resolved.resize(input.object.symbols.size());
            for (std::size_t symbol_index = 0;
                 symbol_index < input.object.symbols.size();
                 ++symbol_index) {
                const auto& symbol = input.object.symbols[symbol_index];
                if (symbol.definition == formats::jro::SymbolDefinition::undefined) {
                    continue;
                }
                ResolvedSymbol value;
                value.object_index = object_index;
                value.symbol_index = symbol_index;
                if (symbol.definition == formats::jro::SymbolDefinition::absolute) {
                    value.value = symbol.value;
                    value.address = false;
                } else {
                    const auto placed_index = placed_lookup_[object_index][*symbol.section_index];
                    if (placed_index == kNoIndex) {
                        continue;
                    }
                    const auto address = static_cast<std::uint64_t>(
                        placed_sections_[placed_index].address
                    ) + symbol.value;
                    if (address > 0xFFFFU) {
                        diagnose(
                            input.logical_path,
                            "L2201",
                            "symbol address exceeds 16 bits: " + symbol.name
                        );
                        continue;
                    }
                    value.value = static_cast<std::uint32_t>(address);
                    value.address = true;
                }
                resolved[symbol_index] = value;
                if (symbol.binding == formats::jro::SymbolBinding::global) {
                    const auto [existing, inserted] = global_symbols_.emplace(symbol.name, value);
                    if (!inserted) {
                        diagnose(
                            input.logical_path,
                            "L2202",
                            "duplicate global symbol: " + symbol.name + " (already defined in "
                                + inputs_[existing->second.object_index].logical_path + ')'
                        );
                    }
                }
            }
        }
    }

    void resolve_entry() {
        const auto entry = global_symbols_.find(script_.entry_symbol);
        if (entry == global_symbols_.end()) {
            diagnose(
                "<link>",
                "L2203",
                "entry symbol is undefined or not global: " + script_.entry_symbol
            );
            return;
        }
        if (!entry->second.address) {
            diagnose("<link>", "L2204", "entry symbol must be address-bearing");
            return;
        }
        entry_point_ = static_cast<std::uint16_t>(entry->second.value);
    }

    std::optional<ResolvedSymbol> resolve_relocation_symbol(
        std::size_t object_index,
        std::uint32_t symbol_index
    ) {
        const auto& symbol = inputs_[object_index].object.symbols[symbol_index];
        if (symbol.definition != formats::jro::SymbolDefinition::undefined) {
            return resolved_symbols_[object_index][symbol_index];
        }
        const auto global = global_symbols_.find(symbol.name);
        if (global == global_symbols_.end()) {
            diagnose(
                inputs_[object_index].logical_path,
                "L2301",
                "undefined symbol: " + symbol.name
            );
            return std::nullopt;
        }
        return global->second;
    }

    void relocation_overflow(
        const InputObject& input,
        const formats::jro::Symbol& symbol,
        std::string_view kind,
        std::int64_t value
    ) {
        diagnose(
            input.logical_path,
            "L2302",
            std::string(kind) + " relocation overflow for " + symbol.name + ": "
                + std::to_string(value)
        );
    }

    void apply_relocations() {
        using formats::jro::RelocationType;

        for (std::size_t object_index = 0; object_index < inputs_.size(); ++object_index) {
            const auto& input = inputs_[object_index];
            for (const auto& relocation : input.object.relocations) {
                const auto placed_index = placed_lookup_[object_index][relocation.section_index];
                if (placed_index == kNoIndex) {
                    continue;
                }
                auto& section = placed_sections_[placed_index];
                const auto resolved = resolve_relocation_symbol(
                    object_index,
                    relocation.symbol_index
                );
                if (!resolved.has_value()) {
                    continue;
                }
                const auto& symbol = input.object.symbols[relocation.symbol_index];
                const auto target = static_cast<std::int64_t>(resolved->value)
                    + relocation.addend;
                const auto field_address = static_cast<std::int64_t>(section.address)
                    + relocation.offset;
                switch (relocation.type) {
                case RelocationType::abs8:
                    if (target < 0 || target > 0xFF) {
                        relocation_overflow(input, symbol, "ABS8", target);
                        break;
                    }
                    section.data[relocation.offset] = static_cast<std::uint8_t>(target);
                    break;
                case RelocationType::abs16_be:
                    if (target < 0 || target > 0xFFFF) {
                        relocation_overflow(input, symbol, "ABS16_BE", target);
                        break;
                    }
                    section.data[relocation.offset] = static_cast<std::uint8_t>(
                        (target >> 8U) & 0xFF
                    );
                    section.data[relocation.offset + 1U] = static_cast<std::uint8_t>(
                        target & 0xFF
                    );
                    break;
                case RelocationType::rel8: {
                    const auto relative = target - (field_address + 1);
                    if (relative < -128 || relative > 127) {
                        relocation_overflow(input, symbol, "REL8", relative);
                        break;
                    }
                    section.data[relocation.offset] = static_cast<std::uint8_t>(
                        relative & 0xFF
                    );
                    break;
                }
                case RelocationType::direct8:
                    if (target < 0 || target > 0xFF) {
                        relocation_overflow(input, symbol, "DIRECT8", target);
                        break;
                    }
                    section.data[relocation.offset] = static_cast<std::uint8_t>(target);
                    break;
                }
            }
        }
    }

    std::optional<std::uint32_t> add_source_file(
        formats::jr8dbg::DebugInfo& debug_info,
        const formats::jro::SourceFile& source,
        const std::string& input_path
    ) {
        const auto found = source_indexes_.find(source.path);
        if (found != source_indexes_.end()) {
            if (debug_info.source_files[found->second].content_sha256 != source.content_sha256) {
                diagnose(
                    input_path,
                    "L2401",
                    "source path has conflicting content digests: " + source.path
                );
                return std::nullopt;
            }
            return found->second;
        }
        const auto index = static_cast<std::uint32_t>(debug_info.source_files.size());
        source_indexes_.emplace(source.path, index);
        debug_info.source_files.push_back(formats::jr8dbg::SourceFile{
            source.path,
            source.content_sha256,
        });
        return index;
    }

    void add_debug_sources(formats::jr8dbg::DebugInfo& debug_info) {
        source_remap_.resize(inputs_.size());
        for (std::size_t object_index = 0; object_index < inputs_.size(); ++object_index) {
            const auto& input = inputs_[object_index];
            auto& remap = source_remap_[object_index];
            remap.resize(input.object.source_files.size());
            for (std::size_t source_index = 0;
                 source_index < input.object.source_files.size();
                 ++source_index) {
                remap[source_index] = add_source_file(
                    debug_info,
                    input.object.source_files[source_index],
                    input.logical_path
                );
            }
        }
    }

    void add_debug_symbols(formats::jr8dbg::DebugInfo& debug_info) {
        for (std::size_t object_index = 0; object_index < inputs_.size(); ++object_index) {
            const auto& input = inputs_[object_index];
            for (std::size_t symbol_index = 0;
                 symbol_index < input.object.symbols.size();
                 ++symbol_index) {
                const auto& symbol = input.object.symbols[symbol_index];
                const auto& resolved = resolved_symbols_[object_index][symbol_index];
                if (!resolved.has_value()) {
                    continue;
                }
                std::optional<std::uint32_t> source_index;
                if (!source_remap_[object_index].empty()
                    && source_remap_[object_index].front().has_value()) {
                    source_index = *source_remap_[object_index].front();
                }
                debug_info.symbols.push_back(formats::jr8dbg::Symbol{
                    symbol.name,
                    symbol.binding == formats::jro::SymbolBinding::global
                        ? formats::jr8dbg::SymbolBinding::global
                        : formats::jr8dbg::SymbolBinding::local,
                    resolved->address ? formats::jr8dbg::SymbolKind::address
                                      : formats::jr8dbg::SymbolKind::absolute,
                    static_cast<std::uint16_t>(resolved->value),
                    symbol.size,
                    source_index,
                });
            }
        }
        std::sort(debug_info.symbols.begin(), debug_info.symbols.end(), debug_symbol_less);
    }

    void add_debug_lines(formats::jr8dbg::DebugInfo& debug_info) {
        for (std::size_t object_index = 0; object_index < inputs_.size(); ++object_index) {
            const auto& input = inputs_[object_index];
            for (const auto& mapping : input.object.source_lines) {
                const auto placed_index = placed_lookup_[object_index][mapping.section_index];
                if (placed_index == kNoIndex
                    || !source_remap_[object_index][mapping.source_file_index].has_value()) {
                    continue;
                }
                const auto address = static_cast<std::uint64_t>(
                    placed_sections_[placed_index].address
                ) + mapping.offset;
                if (address > 0xFFFFU) {
                    diagnose(input.logical_path, "L2402", "source mapping address exceeds 16 bits");
                    continue;
                }
                debug_info.line_mappings.push_back(formats::jr8dbg::LineMapping{
                    static_cast<std::uint16_t>(address),
                    mapping.length,
                    *source_remap_[object_index][mapping.source_file_index],
                    mapping.line,
                    mapping.column,
                });
            }
        }
        std::sort(
            debug_info.line_mappings.begin(),
            debug_info.line_mappings.end(),
            debug_line_less
        );
    }

    std::string make_link_map() const {
        std::ostringstream map;
        map << "JR8LD MAP\n"
            << "Producer: jr8ld " << options_.producer_version << '\n'
            << "Target: " << script_.target_profile << '\n'
            << "Entry: " << script_.entry_symbol << " = " << hex_address(entry_point_)
            << "\n\nRegions:\n";
        for (const auto& region : script_.regions) {
            map << "  " << region.name << ' ' << hex_address(region.origin) << '-'
                << hex_address(
                       static_cast<std::uint32_t>(region_end(region) - 1U)
                   )
                << " (" << std::dec << region.length << " bytes)\n";
        }

        auto sections = placed_sections_;
        std::sort(sections.begin(), sections.end(), [&](const auto& left, const auto& right) {
            return std::tie(left.address, left.object_index, left.section_index)
                < std::tie(right.address, right.object_index, right.section_index);
        });
        map << "\nSections:\n";
        for (const auto& placed : sections) {
            const auto& input = inputs_[placed.object_index];
            const auto& section = input.object.sections[placed.section_index];
            map << "  " << hex_address(placed.address);
            if (section.logical_size == 0U) {
                map << " EMPTY";
            } else {
                map << '-' << hex_address(placed.address + section.logical_size - 1U);
            }
            map << "  " << script_.regions[placed.region_index].name << "  "
                << input.logical_path << ':' << section.name << '\n';
        }
        return map.str();
    }

    static std::string make_symbol_output(const formats::jr8dbg::DebugInfo& debug_info) {
        std::ostringstream symbols;
        symbols << "JR8LD SYMBOLS\n";
        for (const auto& symbol : debug_info.symbols) {
            symbols << (symbol.kind == formats::jr8dbg::SymbolKind::absolute ? '=' : ' ')
                    << hex_address(symbol.value) << ' '
                    << (symbol.binding == formats::jr8dbg::SymbolBinding::global ? 'G' : 'L')
                    << ' ' << symbol.name << " size=" << std::dec << symbol.size << " source=";
            if (symbol.source_file_index.has_value()) {
                symbols << debug_info.source_files[*symbol.source_file_index].path;
            } else {
                symbols << '-';
            }
            symbols << '\n';
        }
        return symbols.str();
    }

    std::optional<Output> build_output() {
        formats::jr8app::Application application;
        application.target_profile = script_.target_profile;
        application.entry_point = entry_point_;
        for (const auto& placed : placed_sections_) {
            const auto& section = inputs_[placed.object_index].object.sections[
                placed.section_index
            ];
            if (section.logical_size == 0U) {
                continue;
            }
            application.segments.push_back(formats::jr8app::Segment{
                section.type == formats::jro::SectionType::program_bits
                    ? formats::jr8app::SegmentKind::data
                    : formats::jr8app::SegmentKind::zero_fill,
                static_cast<std::uint16_t>(placed.address),
                section.logical_size,
                placed.data,
            });
        }
        std::sort(
            application.segments.begin(),
            application.segments.end(),
            [](const auto& left, const auto& right) {
                return std::tie(left.address, left.kind, left.logical_size, left.data)
                    < std::tie(right.address, right.kind, right.logical_size, right.data);
            }
        );

        formats::jr8dbg::DebugInfo debug_info;
        debug_info.target_profile = script_.target_profile;
        add_debug_sources(debug_info);
        add_debug_symbols(debug_info);
        add_debug_lines(debug_info);
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }

        try {
            if (!formats::jr8app::entry_point_is_loaded(application)) {
                diagnose("<link>", "L2999", "entry point is outside loaded segments");
                return std::nullopt;
            }
            application.integrity_sha256 = formats::jr8app::compute_integrity(application);
            debug_info.application_integrity_sha256 = application.integrity_sha256;
            static_cast<void>(formats::jr8app::write(application));
            static_cast<void>(formats::jr8dbg::write(debug_info));
        } catch (const formats::linked::Error& error) {
            diagnose(
                "<link>",
                "L2999",
                std::string("invalid linked output: ") + error.what()
            );
            return std::nullopt;
        }

        return Output{
            application,
            debug_info,
            make_link_map(),
            make_symbol_output(debug_info),
        };
    }

    const std::vector<InputObject>& inputs_;
    const LinkScript& script_;
    const Options& options_;
    std::vector<Diagnostic>& diagnostics_;
    std::vector<RegionState> region_states_;
    std::vector<PlacedSection> placed_sections_;
    std::vector<std::vector<std::size_t>> placed_lookup_;
    std::vector<std::vector<std::optional<ResolvedSymbol>>> resolved_symbols_;
    std::unordered_map<std::string, ResolvedSymbol> global_symbols_;
    std::uint16_t entry_point_{};
    std::unordered_map<std::string, std::uint32_t> source_indexes_;
    std::vector<std::vector<std::optional<std::uint32_t>>> source_remap_;
};

}  // namespace

Result link_objects(
    const std::vector<InputObject>& inputs,
    const LinkScript& script,
    const Options& options
) {
    Result result;
    LinkEngine engine{inputs, script, options, result.diagnostics};
    result.output = engine.run();
    return result;
}

}  // namespace jr800::linker
