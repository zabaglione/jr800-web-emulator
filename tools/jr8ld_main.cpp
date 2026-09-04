// SPDX-License-Identifier: MIT

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "distinct_path_set.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/jro.hpp"
#include "jr800/formats/linked_error.hpp"
#include "jr800/linker/linker.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

struct CliOptions {
    std::filesystem::path script;
    std::filesystem::path application;
    std::filesystem::path debug;
    std::filesystem::path map;
    std::filesystem::path symbols;
    std::vector<std::filesystem::path> inputs;
};

void print_usage(std::ostream& stream) {
    stream << "Usage: jr8ld --script <memory.j8l> -o <output.j8a> "
              "--debug <output.j8d> --map <output.map> --symbols <output.sym> "
              "<input.jro> [input.jro ...]\n";
}

std::optional<CliOptions> parse_options(int argc, char* argv[]) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&](std::string_view option) -> std::optional<std::string> {
            if (index + 1 >= argc) {
                std::cerr << "jr8ld: missing value for " << option << '\n';
                return std::nullopt;
            }
            ++index;
            return std::string{argv[index]};
        };
        if (argument == "--script") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.script = *value;
        } else if (argument == "-o") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.application = *value;
        } else if (argument == "--debug") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.debug = *value;
        } else if (argument == "--map") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.map = *value;
        } else if (argument == "--symbols") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.symbols = *value;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8ld: unknown option: " << argument << '\n';
            return std::nullopt;
        } else {
            options.inputs.emplace_back(argument);
        }
    }
    if (options.script.empty() || options.application.empty() || options.debug.empty()
        || options.map.empty() || options.symbols.empty() || options.inputs.empty()) {
        std::cerr << "jr8ld: all output options, --script, and input objects are required\n";
        return std::nullopt;
    }

    jr800::tools::DistinctPathSet paths;
    const auto insert_path = [&](const std::filesystem::path& path) {
        const auto result = paths.insert(path);
        if (result == jr800::tools::PathInsertResult::inspection_error) {
            std::cerr << "jr8ld: cannot resolve path identity: " << path.string() << '\n';
        }
        return result == jr800::tools::PathInsertResult::inserted;
    };
    if (!insert_path(options.script) || !insert_path(options.application)
        || !insert_path(options.debug) || !insert_path(options.map)
        || !insert_path(options.symbols)) {
        std::cerr << "jr8ld: script and output paths must be distinct\n";
        return std::nullopt;
    }
    for (const auto& input : options.inputs) {
        if (!insert_path(input)) {
            std::cerr << "jr8ld: input, script, and output paths must be distinct\n";
            return std::nullopt;
        }
    }
    return options;
}

std::optional<std::vector<std::uint8_t>> read_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8ld: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

std::optional<std::string> read_text(const std::filesystem::path& path) {
    const auto bytes = read_binary(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return std::string{bytes->begin(), bytes->end()};
}

bool write_binary(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "jr8ld: cannot open output: " << path.string() << '\n';
        return false;
    }
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!output) {
        std::cerr << "jr8ld: failed to write output: " << path.string() << '\n';
        return false;
    }
    return true;
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
    return write_binary(
        path,
        std::vector<std::uint8_t>{text.begin(), text.end()}
    );
}

void print_diagnostics(const std::vector<jr800::linker::Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.path;
        if (diagnostic.line != 0U) {
            std::cerr << ':' << diagnostic.line << ':' << diagnostic.column;
        }
        std::cerr << ": error[" << diagnostic.code << "]: "
                  << diagnostic.message << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "jr8ld " << JR800_PROJECT_VERSION << '\n';
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

    const auto script_text = read_text(options->script);
    if (!script_text.has_value()) {
        return 2;
    }
    const auto script_result = jr800::linker::parse_script(jr800::linker::ScriptSource{
        options->script.generic_string(),
        *script_text,
    });
    if (!script_result.succeeded()) {
        print_diagnostics(script_result.diagnostics);
        return 1;
    }

    std::vector<jr800::linker::InputObject> inputs;
    inputs.reserve(options->inputs.size());
    for (const auto& path : options->inputs) {
        const auto bytes = read_binary(path);
        if (!bytes.has_value()) {
            return 2;
        }
        try {
            inputs.push_back(jr800::linker::InputObject{
                path.generic_string(),
                jr800::formats::jro::read(*bytes),
            });
        } catch (const jr800::formats::jro::Error& error) {
            std::cerr << path.string() << ": error: invalid JRO: " << error.what();
            if (error.byte_offset().has_value()) {
                std::cerr << " at byte " << *error.byte_offset();
            }
            std::cerr << '\n';
            return 1;
        }
    }

    const auto link_result = jr800::linker::link_objects(
        inputs,
        *script_result.script,
        jr800::linker::Options{JR800_PROJECT_VERSION}
    );
    if (!link_result.succeeded()) {
        print_diagnostics(link_result.diagnostics);
        return 1;
    }

    try {
        const auto application_bytes = jr800::formats::jr8app::write(
            link_result.output->application
        );
        const auto debug_bytes = jr800::formats::jr8dbg::write(
            link_result.output->debug_info
        );
        if (!write_binary(options->application, application_bytes)
            || !write_binary(options->debug, debug_bytes)
            || !write_text(options->map, link_result.output->link_map)
            || !write_text(options->symbols, link_result.output->symbol_output)) {
            return 2;
        }
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8ld: failed to serialize output: " << error.what() << '\n';
        return 2;
    }
    return 0;
}
