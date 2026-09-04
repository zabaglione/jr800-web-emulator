// SPDX-License-Identifier: MIT

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "distinct_path_set.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jro.hpp"
#include "jr800/formats/linked_error.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

struct CliOptions {
    std::optional<std::uint32_t> segment_index;
    std::optional<std::string> section_name;
    std::optional<std::uint32_t> image_address;
    std::optional<std::uint32_t> image_size;
    std::filesystem::path input;
    std::filesystem::path output;
};

void print_usage(std::ostream& stream) {
    stream << "Usage: jr8objcopy --segment <index> -o <output.bin> "
              "<input.j8a>\n"
           << "       jr8objcopy --section <name> -o <output.bin> "
              "<input.jro>\n"
           << "       jr8objcopy --image <address> --size <bytes> "
              "-o <output.bin> <input.j8a>\n";
}

std::optional<std::uint32_t> parse_unsigned(
    std::string_view text,
    bool allow_hexadecimal
) {
    auto digits = text;
    auto base = 10;
    if (
        allow_hexadecimal
        && digits.size() > 2U
        && digits[0] == '0'
        && (digits[1] == 'x' || digits[1] == 'X')
    ) {
        digits.remove_prefix(2U);
        base = 16;
    }
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(
        digits.data(),
        digits.data() + digits.size(),
        value,
        base
    );
    if (
        digits.empty()
        || error != std::errc{}
        || end != digits.data() + digits.size()
    ) {
        return std::nullopt;
    }
    return value;
}

std::optional<CliOptions> parse_options(int argc, char* argv[]) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&](std::string_view option)
            -> std::optional<std::string_view> {
            if (
                index + 1 >= argc
                || std::string_view{argv[index + 1]}.starts_with('-')
            ) {
                std::cerr << "jr8objcopy: missing value for " << option << '\n';
                return std::nullopt;
            }
            ++index;
            return std::string_view{argv[index]};
        };

        if (argument == "--segment") {
            if (options.segment_index.has_value()) {
                std::cerr << "jr8objcopy: --segment may be specified only once\n";
                return std::nullopt;
            }
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.segment_index = parse_unsigned(*value, false);
            if (!options.segment_index.has_value()) {
                std::cerr << "jr8objcopy: invalid segment index: " << *value << '\n';
                return std::nullopt;
            }
        } else if (argument == "--section") {
            if (options.section_name.has_value()) {
                std::cerr << "jr8objcopy: --section may be specified only once\n";
                return std::nullopt;
            }
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.section_name = *value;
        } else if (argument == "--image") {
            if (options.image_address.has_value()) {
                std::cerr << "jr8objcopy: --image may be specified only once\n";
                return std::nullopt;
            }
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.image_address = parse_unsigned(*value, true);
            if (
                !options.image_address.has_value()
                || *options.image_address > 0xFFFFU
            ) {
                std::cerr << "jr8objcopy: invalid image address: " << *value
                          << '\n';
                return std::nullopt;
            }
        } else if (argument == "--size") {
            if (options.image_size.has_value()) {
                std::cerr << "jr8objcopy: --size may be specified only once\n";
                return std::nullopt;
            }
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.image_size = parse_unsigned(*value, true);
            if (
                !options.image_size.has_value()
                || *options.image_size == 0U
                || *options.image_size > 65'536U
            ) {
                std::cerr << "jr8objcopy: invalid image size: " << *value << '\n';
                return std::nullopt;
            }
        } else if (argument == "-o") {
            if (!options.output.empty()) {
                std::cerr << "jr8objcopy: -o may be specified only once\n";
                return std::nullopt;
            }
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.output = *value;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8objcopy: unknown option: " << argument << '\n';
            return std::nullopt;
        } else if (!options.input.empty()) {
            std::cerr << "jr8objcopy: exactly one input is required\n";
            return std::nullopt;
        } else {
            options.input = argument;
        }
    }

    if (options.image_address.has_value() != options.image_size.has_value()) {
        std::cerr << "jr8objcopy: --image and --size must be specified together\n";
        return std::nullopt;
    }
    if (
        options.image_address.has_value()
        && *options.image_address + *options.image_size > 65'536U
    ) {
        std::cerr << "jr8objcopy: image range exceeds the 16-bit address space\n";
        return std::nullopt;
    }
    const auto selector_count = static_cast<unsigned int>(
        options.segment_index.has_value()
    ) + static_cast<unsigned int>(options.section_name.has_value())
        + static_cast<unsigned int>(options.image_address.has_value());
    if (
        selector_count != 1U
        || options.output.empty()
        || options.input.empty()
    ) {
        std::cerr << "jr8objcopy: exactly one of --segment, --section, or "
                     "--image with --size, plus -o and one input, is required\n";
        return std::nullopt;
    }

    jr800::tools::DistinctPathSet paths;
    const auto input_result = paths.insert(options.input);
    const auto output_result = paths.insert(options.output);
    if (
        input_result == jr800::tools::PathInsertResult::inspection_error
        || output_result == jr800::tools::PathInsertResult::inspection_error
    ) {
        std::cerr << "jr8objcopy: cannot resolve input or output path identity\n";
        return std::nullopt;
    }
    if (
        input_result != jr800::tools::PathInsertResult::inserted
        || output_result != jr800::tools::PathInsertResult::inserted
    ) {
        std::cerr << "jr8objcopy: input and output paths must be distinct\n";
        return std::nullopt;
    }
    return options;
}

std::optional<std::vector<std::uint8_t>> read_binary(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8objcopy: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

bool write_binary(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "jr8objcopy: cannot open output: " << path.string() << '\n';
        return false;
    }
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!output) {
        std::cerr << "jr8objcopy: failed to write output: " << path.string() << '\n';
        return false;
    }
    return true;
}

int extract_application_segment(
    const CliOptions& options,
    const std::vector<std::uint8_t>& bytes
) {
    try {
        const auto application = jr800::formats::jr8app::read(bytes);
        const auto segment_index = *options.segment_index;
        if (segment_index >= application.segments.size()) {
            std::cerr << "jr8objcopy: segment index is out of range: "
                      << segment_index << '\n';
            return 1;
        }
        const auto& segment = application.segments[segment_index];
        if (segment.kind != jr800::formats::jr8app::SegmentKind::data) {
            std::cerr << "jr8objcopy: segment " << segment_index
                      << " is ZERO_FILL and has no stored bytes\n";
            return 1;
        }
        return write_binary(options.output, segment.data) ? 0 : 2;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8objcopy: invalid JR8APP: " << error.what();
        if (error.byte_offset().has_value()) {
            std::cerr << " at byte " << *error.byte_offset();
        }
        std::cerr << '\n';
        return 1;
    }
}

int extract_object_section(
    const CliOptions& options,
    const std::vector<std::uint8_t>& bytes
) {
    try {
        const auto object = jr800::formats::jro::read(bytes);
        const auto found = std::find_if(
            object.sections.begin(),
            object.sections.end(),
            [&](const auto& section) {
                return section.name == *options.section_name;
            }
        );
        if (found == object.sections.end()) {
            std::cerr << "jr8objcopy: section was not found\n";
            return 1;
        }
        if (found->type != jr800::formats::jro::SectionType::program_bits) {
            std::cerr << "jr8objcopy: selected section is NO_BITS and has no "
                         "stored bytes\n";
            return 1;
        }
        const auto section_index = static_cast<std::uint32_t>(
            std::distance(object.sections.begin(), found)
        );
        const auto has_relocation = std::any_of(
            object.relocations.begin(),
            object.relocations.end(),
            [section_index](const auto& relocation) {
                return relocation.section_index == section_index;
            }
        );
        if (has_relocation) {
            std::cerr << "jr8objcopy: selected section has unresolved relocations\n";
            return 1;
        }
        return write_binary(options.output, found->data) ? 0 : 2;
    } catch (const jr800::formats::jro::Error& error) {
        std::cerr << "jr8objcopy: invalid JRO: " << error.what();
        if (error.byte_offset().has_value()) {
            std::cerr << " at byte " << *error.byte_offset();
        }
        std::cerr << '\n';
        return 1;
    }
}

int extract_application_image(
    const CliOptions& options,
    const std::vector<std::uint8_t>& bytes
) {
    try {
        const auto application = jr800::formats::jr8app::read(bytes);
        const auto image_begin = *options.image_address;
        const auto image_size = *options.image_size;
        const auto image_end = image_begin + image_size;
        std::vector<std::uint8_t> image(image_size, 0U);
        std::vector<bool> loaded(image_size, false);

        for (const auto& segment : application.segments) {
            const auto segment_begin = static_cast<std::uint32_t>(
                segment.address
            );
            const auto segment_end = segment_begin + segment.logical_size;
            const auto overlap_begin = std::max(image_begin, segment_begin);
            const auto overlap_end = std::min(image_end, segment_end);
            if (overlap_begin >= overlap_end) {
                continue;
            }

            const auto image_offset = overlap_begin - image_begin;
            const auto segment_offset = overlap_begin - segment_begin;
            const auto count = overlap_end - overlap_begin;
            if (segment.kind == jr800::formats::jr8app::SegmentKind::data) {
                std::copy_n(
                    segment.data.begin() + segment_offset,
                    count,
                    image.begin() + image_offset
                );
            }
            std::fill_n(loaded.begin() + image_offset, count, true);
        }

        if (std::find(loaded.begin(), loaded.end(), false) != loaded.end()) {
            std::cerr << "jr8objcopy: image range includes unloaded addresses\n";
            return 1;
        }
        return write_binary(options.output, image) ? 0 : 2;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8objcopy: invalid JR8APP: " << error.what();
        if (error.byte_offset().has_value()) {
            std::cerr << " at byte " << *error.byte_offset();
        }
        std::cerr << '\n';
        return 1;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "jr8objcopy " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }

    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        print_usage(std::cerr);
        return 2;
    }
    const auto bytes = read_binary(options->input);
    if (!bytes.has_value()) {
        return 2;
    }

    if (options->segment_index.has_value()) {
        return extract_application_segment(*options, *bytes);
    }
    if (options->section_name.has_value()) {
        return extract_object_section(*options, *bytes);
    }
    return extract_application_image(*options, *bytes);
}
