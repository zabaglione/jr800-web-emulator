// SPDX-License-Identifier: MIT

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "distinct_path_set.hpp"
#include "jr800/formats/jr8rom.hpp"
#include "jr800/formats/linked_error.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

struct SegmentInput {
    std::uint16_t address{};
    std::filesystem::path path;
};

struct CreateOptions {
    std::vector<SegmentInput> segments;
    std::filesystem::path output;
};

struct CombineOptions {
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output;
};

struct ExtractOptions {
    std::optional<std::uint16_t> address;
    std::optional<std::size_t> size;
    std::filesystem::path input;
    std::filesystem::path output;
};

void print_usage(std::ostream& stream) {
    stream << "Usage:\n"
              "  jr8rom create -o <output.j8r> "
              "--segment <address> <input.bin> [--segment ...]\n"
              "  jr8rom combine -o <output.j8r> "
              "<input.j8r> <input.j8r> [input...]\n"
              "  jr8rom extract --address <address> --size <bytes> "
              "-o <output.bin> <input.j8r>\n"
              "  jr8rom inspect <input.j8r>\n"
              "  jr8rom verify <input.j8r>\n";
}

std::optional<std::uint32_t> parse_unsigned(std::string_view text) {
    auto digits = text;
    auto base = 10;
    if (!digits.empty() && digits.front() == '$') {
        digits.remove_prefix(1U);
        base = 16;
    } else if (
        digits.size() > 2U
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
    if (digits.empty() || error != std::errc{}
        || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint16_t> parse_address(std::string_view text) {
    const auto value = parse_unsigned(text);
    if (!value.has_value() || *value > 0xFFFFU) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*value);
}

std::optional<std::size_t> parse_size(std::string_view text) {
    const auto value = parse_unsigned(text);
    if (!value.has_value() || *value == 0U
        || *value > jr800::formats::jr8rom::maximum_segment_size) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(*value);
}

bool output_is_distinct(const CreateOptions& options) {
    for (const auto& segment : options.segments) {
        jr800::tools::DistinctPathSet paths;
        const auto input_result = paths.insert(segment.path);
        const auto output_result = paths.insert(options.output);
        if (
            input_result == jr800::tools::PathInsertResult::inspection_error
            || output_result == jr800::tools::PathInsertResult::inspection_error
        ) {
            std::cerr << "jr8rom: cannot resolve input or output path identity\n";
            return false;
        }
        if (
            input_result != jr800::tools::PathInsertResult::inserted
            || output_result != jr800::tools::PathInsertResult::inserted
        ) {
            std::cerr << "jr8rom: segment inputs and output must be distinct\n";
            return false;
        }
    }
    return true;
}

bool input_and_output_are_distinct(
    const std::filesystem::path& input,
    const std::filesystem::path& output
) {
    jr800::tools::DistinctPathSet paths;
    const auto input_result = paths.insert(input);
    const auto output_result = paths.insert(output);
    if (
        input_result == jr800::tools::PathInsertResult::inspection_error
        || output_result == jr800::tools::PathInsertResult::inspection_error
    ) {
        std::cerr << "jr8rom: cannot resolve input or output path identity\n";
        return false;
    }
    if (
        input_result != jr800::tools::PathInsertResult::inserted
        || output_result != jr800::tools::PathInsertResult::inserted
    ) {
        std::cerr << "jr8rom: input and output must be distinct\n";
        return false;
    }
    return true;
}

std::optional<CreateOptions> parse_create_options(int argc, char* argv[]) {
    CreateOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "-o") {
            if (!options.output.empty()) {
                std::cerr << "jr8rom: -o may be specified only once\n";
                return std::nullopt;
            }
            if (index + 1 >= argc) {
                std::cerr << "jr8rom: missing value for -o\n";
                return std::nullopt;
            }
            options.output = argv[++index];
            continue;
        }
        if (argument == "--segment") {
            if (index + 2 >= argc) {
                std::cerr << "jr8rom: --segment requires an address and input\n";
                return std::nullopt;
            }
            const std::string_view address_text{argv[++index]};
            const auto address = parse_address(address_text);
            if (!address.has_value()) {
                std::cerr << "jr8rom: invalid segment address: "
                          << address_text << '\n';
                return std::nullopt;
            }
            options.segments.push_back(SegmentInput{*address, argv[++index]});
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8rom: unknown option: " << argument << '\n';
        } else {
            std::cerr << "jr8rom: unexpected argument: " << argument << '\n';
        }
        return std::nullopt;
    }

    if (options.output.empty() || options.segments.empty()) {
        std::cerr << "jr8rom: create requires -o and at least one --segment\n";
        return std::nullopt;
    }
    if (!output_is_distinct(options)) {
        return std::nullopt;
    }
    return options;
}

std::optional<CombineOptions> parse_combine_options(int argc, char* argv[]) {
    CombineOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "-o") {
            if (!options.output.empty()) {
                std::cerr << "jr8rom: -o may be specified only once\n";
                return std::nullopt;
            }
            if (index + 1 >= argc) {
                std::cerr << "jr8rom: missing value for -o\n";
                return std::nullopt;
            }
            options.output = argv[++index];
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8rom: unknown option: " << argument << '\n';
            return std::nullopt;
        }
        options.inputs.emplace_back(argument);
    }

    if (options.output.empty() || options.inputs.size() < 2U) {
        std::cerr << "jr8rom: combine requires -o and at least two inputs\n";
        return std::nullopt;
    }
    for (const auto& input : options.inputs) {
        if (!input_and_output_are_distinct(input, options.output)) {
            return std::nullopt;
        }
    }
    return options;
}

std::optional<ExtractOptions> parse_extract_options(int argc, char* argv[]) {
    ExtractOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--address") {
            if (options.address.has_value()) {
                std::cerr << "jr8rom: --address may be specified only once\n";
                return std::nullopt;
            }
            if (index + 1 >= argc) {
                std::cerr << "jr8rom: missing value for --address\n";
                return std::nullopt;
            }
            const std::string_view value{argv[++index]};
            options.address = parse_address(value);
            if (!options.address.has_value()) {
                std::cerr << "jr8rom: invalid extraction address: " << value
                          << '\n';
                return std::nullopt;
            }
            continue;
        }
        if (argument == "--size") {
            if (options.size.has_value()) {
                std::cerr << "jr8rom: --size may be specified only once\n";
                return std::nullopt;
            }
            if (index + 1 >= argc) {
                std::cerr << "jr8rom: missing value for --size\n";
                return std::nullopt;
            }
            const std::string_view value{argv[++index]};
            options.size = parse_size(value);
            if (!options.size.has_value()) {
                std::cerr << "jr8rom: invalid extraction size: " << value << '\n';
                return std::nullopt;
            }
            continue;
        }
        if (argument == "-o") {
            if (!options.output.empty()) {
                std::cerr << "jr8rom: -o may be specified only once\n";
                return std::nullopt;
            }
            if (index + 1 >= argc) {
                std::cerr << "jr8rom: missing value for -o\n";
                return std::nullopt;
            }
            options.output = argv[++index];
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8rom: unknown option: " << argument << '\n';
            return std::nullopt;
        }
        if (!options.input.empty()) {
            std::cerr << "jr8rom: extract requires exactly one input\n";
            return std::nullopt;
        }
        options.input = argument;
    }

    if (
        !options.address.has_value()
        || !options.size.has_value()
        || options.input.empty()
        || options.output.empty()
    ) {
        std::cerr << "jr8rom: extract requires --address, --size, -o, and one "
                     "input\n";
        return std::nullopt;
    }
    if (
        static_cast<std::size_t>(*options.address) + *options.size
        > jr800::formats::jr8rom::maximum_segment_size
    ) {
        std::cerr << "jr8rom: extraction range exceeds the 16-bit address "
                     "space\n";
        return std::nullopt;
    }
    if (!input_and_output_are_distinct(options.input, options.output)) {
        return std::nullopt;
    }
    return options;
}

std::optional<std::vector<std::uint8_t>> read_binary(
    const std::filesystem::path& path,
    std::uintmax_t maximum_size,
    bool allow_empty
) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        std::cerr << "jr8rom: cannot inspect input: " << path.string() << '\n';
        return std::nullopt;
    }
    if (size > maximum_size) {
        std::cerr << "jr8rom: input exceeds the size limit: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    if (!allow_empty && size == 0U) {
        std::cerr << "jr8rom: segment input must not be empty: "
                  << path.string() << '\n';
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8rom: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    if (!input) {
        std::cerr << "jr8rom: failed to read input: " << path.string() << '\n';
        return std::nullopt;
    }
    return bytes;
}

bool write_binary(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "jr8rom: cannot open output: " << path.string() << '\n';
        return false;
    }
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!output) {
        std::cerr << "jr8rom: failed to write output: " << path.string() << '\n';
        return false;
    }
    return true;
}

int create_container(const CreateOptions& options) {
    jr800::formats::jr8rom::Image image;
    image.segments.reserve(options.segments.size());
    for (const auto& input : options.segments) {
        auto data = read_binary(
            input.path,
            jr800::formats::jr8rom::maximum_segment_size,
            false
        );
        if (!data.has_value()) {
            return 2;
        }
        image.segments.push_back({input.address, std::move(*data)});
    }

    try {
        image.integrity_sha256 = jr800::formats::jr8rom::compute_integrity(image);
        const auto bytes = jr800::formats::jr8rom::write(image);
        return write_binary(options.output, bytes) ? 0 : 2;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8rom: invalid segment set: " << error.what() << '\n';
        return 1;
    }
}

int combine_containers(const CombineOptions& options) {
    jr800::formats::jr8rom::Image combined;
    for (const auto& input : options.inputs) {
        const auto bytes = read_binary(
            input,
            jr800::formats::jr8rom::maximum_encoded_size,
            true
        );
        if (!bytes.has_value()) {
            return 2;
        }
        try {
            auto image = jr800::formats::jr8rom::read(*bytes);
            combined.segments.insert(
                combined.segments.end(),
                std::make_move_iterator(image.segments.begin()),
                std::make_move_iterator(image.segments.end())
            );
        } catch (const jr800::formats::linked::Error& error) {
            std::cerr << "jr8rom: invalid JR8ROM input " << input.string()
                      << ": " << error.what();
            if (error.byte_offset().has_value()) {
                std::cerr << " at byte " << *error.byte_offset();
            }
            std::cerr << '\n';
            return 1;
        }
    }

    try {
        combined.integrity_sha256 =
            jr800::formats::jr8rom::compute_integrity(combined);
        const auto bytes = jr800::formats::jr8rom::write(combined);
        return write_binary(options.output, bytes) ? 0 : 2;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8rom: cannot combine inputs: " << error.what() << '\n';
        return 1;
    }
}

int verify_container(const std::filesystem::path& path) {
    const auto bytes = read_binary(
        path,
        jr800::formats::jr8rom::maximum_encoded_size,
        true
    );
    if (!bytes.has_value()) {
        return 2;
    }
    try {
        static_cast<void>(jr800::formats::jr8rom::read(*bytes));
        return 0;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8rom: invalid JR8ROM: " << error.what();
        if (error.byte_offset().has_value()) {
            std::cerr << " at byte " << *error.byte_offset();
        }
        std::cerr << '\n';
        return 1;
    }
}

int inspect_container(const std::filesystem::path& path) {
    const auto bytes = read_binary(
        path,
        jr800::formats::jr8rom::maximum_encoded_size,
        true
    );
    if (!bytes.has_value()) {
        return 2;
    }
    try {
        const auto image = jr800::formats::jr8rom::read(*bytes);
        std::cout << "JR8ROM "
                  << jr800::formats::jr8rom::format_major_version << '.'
                  << jr800::formats::jr8rom::format_minor_version
                  << " segments=" << image.segments.size() << '\n';
        for (std::size_t index = 0; index < image.segments.size(); ++index) {
            const auto& segment = image.segments[index];
            const auto end = static_cast<std::size_t>(segment.address)
                + segment.data.size() - 1U;
            std::cout << index << "\taddress=$" << std::uppercase << std::hex
                      << std::setw(4) << std::setfill('0') << segment.address
                      << std::dec << "\tlength=" << segment.data.size()
                      << "\tend=$" << std::uppercase << std::hex
                      << std::setw(4) << std::setfill('0') << end
                      << std::dec << '\n';
        }
        return std::cout ? 0 : 2;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8rom: invalid JR8ROM: " << error.what();
        if (error.byte_offset().has_value()) {
            std::cerr << " at byte " << *error.byte_offset();
        }
        std::cerr << '\n';
        return 1;
    }
}

int extract_container(const ExtractOptions& options) {
    const auto bytes = read_binary(
        options.input,
        jr800::formats::jr8rom::maximum_encoded_size,
        true
    );
    if (!bytes.has_value()) {
        return 2;
    }
    try {
        const auto image = jr800::formats::jr8rom::read(*bytes);
        const auto extracted = jr800::formats::jr8rom::extract_range(
            image,
            *options.address,
            *options.size
        );
        if (!extracted.has_value()) {
            std::cerr << "jr8rom: extraction range includes unstored addresses\n";
            return 1;
        }
        return write_binary(options.output, *extracted) ? 0 : 2;
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8rom: invalid JR8ROM: " << error.what();
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
        std::cout << "jr8rom " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }
    if (argc < 2) {
        std::cerr << "jr8rom: a command is required\n";
        print_usage(std::cerr);
        return 2;
    }

    const std::string_view command{argv[1]};
    if (command == "create") {
        const auto options = parse_create_options(argc, argv);
        if (!options.has_value()) {
            print_usage(std::cerr);
            return 2;
        }
        return create_container(*options);
    }
    if (command == "combine") {
        const auto options = parse_combine_options(argc, argv);
        if (!options.has_value()) {
            print_usage(std::cerr);
            return 2;
        }
        return combine_containers(*options);
    }
    if (command == "extract") {
        const auto options = parse_extract_options(argc, argv);
        if (!options.has_value()) {
            print_usage(std::cerr);
            return 2;
        }
        return extract_container(*options);
    }
    if (command == "verify") {
        if (argc != 3) {
            std::cerr << "jr8rom: verify requires exactly one input\n";
            print_usage(std::cerr);
            return 2;
        }
        return verify_container(argv[2]);
    }
    if (command == "inspect") {
        if (argc != 3) {
            std::cerr << "jr8rom: inspect requires exactly one input\n";
            print_usage(std::cerr);
            return 2;
        }
        return inspect_container(argv[2]);
    }

    std::cerr << "jr8rom: unknown command: " << command << '\n';
    print_usage(std::cerr);
    return 2;
}
