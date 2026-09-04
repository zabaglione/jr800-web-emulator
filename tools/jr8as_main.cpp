// SPDX-License-Identifier: MIT

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "distinct_path_set.hpp"
#include "jr800/assembler/assembler.hpp"
#include "jr800/formats/jro.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

struct CliOptions {
    std::string target;
    std::filesystem::path output;
    std::optional<std::filesystem::path> listing;
    std::filesystem::path input;
};

void print_usage(std::ostream& stream) {
    stream << "Usage: jr8as --target <profile> -o <output.jro> "
              "[--listing <output.lst>] <input.s>\n";
}

std::optional<CliOptions> parse_options(int argc, char* argv[]) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&](std::string_view option) -> std::optional<std::string> {
            if (index + 1 >= argc) {
                std::cerr << "jr8as: missing value for " << option << '\n';
                return std::nullopt;
            }
            ++index;
            return std::string{argv[index]};
        };

        if (argument == "--target") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.target = *value;
        } else if (argument == "-o") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.output = *value;
        } else if (argument == "--listing") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.listing = *value;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8as: unknown option: " << argument << '\n';
            return std::nullopt;
        } else if (!options.input.empty()) {
            std::cerr << "jr8as: exactly one input file is required\n";
            return std::nullopt;
        } else {
            options.input = argument;
        }
    }

    if (options.target.empty() || options.output.empty() || options.input.empty()) {
        std::cerr << "jr8as: --target, -o, and one input file are required\n";
        return std::nullopt;
    }

    jr800::tools::DistinctPathSet paths;
    const auto insert_path = [&](const std::filesystem::path& path) {
        const auto result = paths.insert(path);
        if (result == jr800::tools::PathInsertResult::inspection_error) {
            std::cerr << "jr8as: cannot resolve path identity: " << path.string() << '\n';
        }
        return result == jr800::tools::PathInsertResult::inserted;
    };
    if (!insert_path(options.input) || !insert_path(options.output)
        || (options.listing.has_value() && !insert_path(*options.listing))) {
        std::cerr << "jr8as: input, object, and listing paths must be distinct\n";
        return std::nullopt;
    }
    return options;
}

std::optional<std::string> read_source(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8as: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::string{
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
        std::cerr << "jr8as: cannot open output: " << path.string() << '\n';
        return false;
    }
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!output) {
        std::cerr << "jr8as: failed to write output: " << path.string() << '\n';
        return false;
    }
    return true;
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "jr8as: cannot open listing: " << path.string() << '\n';
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        std::cerr << "jr8as: failed to write listing: " << path.string() << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "jr8as " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }

    const auto cli_options = parse_options(argc, argv);
    if (!cli_options.has_value()) {
        print_usage(std::cerr);
        return 2;
    }
    const auto source_text = read_source(cli_options->input);
    if (!source_text.has_value()) {
        return 2;
    }

    const auto result = jr800::assembler::assemble(
        jr800::assembler::Source{cli_options->input.generic_string(), *source_text},
        jr800::assembler::Options{cli_options->target, JR800_PROJECT_VERSION}
    );
    if (!result.succeeded()) {
        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.path << ':' << diagnostic.line << ':'
                      << diagnostic.column << ": error[" << diagnostic.code
                      << "]: " << diagnostic.message << '\n';
        }
        return 1;
    }

    try {
        const auto bytes = jr800::formats::jro::write(result.output->object);
        if (!write_binary(cli_options->output, bytes)) {
            return 2;
        }
        if (cli_options->listing.has_value()
            && !write_text(*cli_options->listing, result.output->listing)) {
            return 2;
        }
    } catch (const jr800::formats::jro::Error& error) {
        std::cerr << "jr8as: failed to serialize JRO: " << error.what() << '\n';
        return 2;
    }
    return 0;
}
