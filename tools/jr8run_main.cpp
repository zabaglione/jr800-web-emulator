// SPDX-License-Identifier: MIT

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/linked_error.hpp"
#include "jr800/runtime/application_loader.hpp"
#include "jr8run_jr800.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

struct MemoryDump {
    std::uint16_t address{};
    std::uint32_t length{};
};

struct MemoryWatchpoint {
    std::uint16_t address{};
    jr800::debugger::MemoryWatchpointMode mode{
        jr800::debugger::MemoryWatchpointMode::access
    };
};

struct SourceLocation {
    std::string path;
    std::uint32_t line{};
};

struct ExecutionBreakpoint {
    std::uint16_t address{};
    std::string condition;
};

struct CliOptions {
    std::filesystem::path application;
    std::optional<std::filesystem::path> debug;
    std::vector<ExecutionBreakpoint> execution_breakpoints;
    std::vector<MemoryWatchpoint> memory_watchpoints;
    std::optional<std::uint16_t> run_to_address;
    std::optional<SourceLocation> run_to_source;
    std::optional<std::string> run_to_symbol;
    std::optional<jr800::debugger::AccessTraceFilter> trace_filter;
    bool step_over{};
    bool step_out{};
    std::vector<MemoryDump> dumps;
    std::vector<std::string> expected_expressions;
    std::optional<std::string> expected_stop;
    std::uint16_t stack_pointer{0x01FF};
    std::uint64_t instruction_limit{100'000};
};

void print_usage(std::ostream& stream) {
    stream << "Usage: jr8run [--debug <app.j8d>] [--break <address>] "
              "[--break-if <address>:<expression>] "
              "[--watch <read|write|access>:<address>] "
              "[--step-over | --step-out | --run-to <address> | "
              "--run-to-source <path:line> | --run-to-symbol <name>] "
              "[--max-instructions <count>] "
              "[--trace <fetch|read|write|data|all>:<first>:<last>] "
              "[--stack <address>] [--dump <address:length>] "
              "[--expect <expression>] [--expect-stop <reason>] "
              "<app.j8a>\n"
              "       jr8run jr800 [options] <rom.j8r>\n";
}

std::optional<std::uint64_t> parse_number(std::string_view text) {
    int base = 10;
    if (!text.empty() && text.front() == '$') {
        base = 16;
        text.remove_prefix(1U);
    } else if (text.size() > 2U && text[0] == '0'
               && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2U);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint16_t> parse_address(std::string_view text) {
    const auto value = parse_number(text);
    if (!value.has_value() || *value > 0xFFFFU) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*value);
}

std::optional<SourceLocation> parse_source_location(std::string_view text) {
    const auto separator = text.rfind(':');
    if (separator == std::string_view::npos || separator == 0U
        || separator + 1U == text.size()) {
        return std::nullopt;
    }
    const auto line = parse_number(text.substr(separator + 1U));
    if (!line.has_value() || *line == 0U || *line > 0xFFFF'FFFFULL) {
        return std::nullopt;
    }
    return SourceLocation{
        std::string{text.substr(0U, separator)},
        static_cast<std::uint32_t>(*line),
    };
}

std::optional<jr800::debugger::AccessTraceFilter> parse_trace_filter(
    std::string_view text
) {
    const auto first_separator = text.find(':');
    const auto second_separator = first_separator == std::string_view::npos
        ? std::string_view::npos
        : text.find(':', first_separator + 1U);
    if (first_separator == std::string_view::npos
        || second_separator == std::string_view::npos
        || text.find(':', second_separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }
    const auto first_address = parse_address(
        text.substr(first_separator + 1U, second_separator - first_separator - 1U)
    );
    const auto last_address = parse_address(text.substr(second_separator + 1U));
    if (!first_address.has_value() || !last_address.has_value()) {
        return std::nullopt;
    }
    using jr800::debugger::AccessTraceMask;
    const auto mode = text.substr(0U, first_separator);
    std::optional<AccessTraceMask> mask;
    if (mode == "fetch") {
        mask = AccessTraceMask::instruction_fetch;
    } else if (mode == "read") {
        mask = AccessTraceMask::data_read;
    } else if (mode == "write") {
        mask = AccessTraceMask::data_write;
    } else if (mode == "data") {
        mask = AccessTraceMask::data;
    } else if (mode == "all") {
        mask = AccessTraceMask::all;
    }
    if (!mask.has_value()) {
        return std::nullopt;
    }
    const jr800::debugger::AccessTraceFilter filter{
        *first_address,
        *last_address,
        *mask,
    };
    return jr800::debugger::is_valid_access_trace_filter(filter)
        ? std::optional{filter}
        : std::nullopt;
}

std::optional<MemoryDump> parse_dump(std::string_view text) {
    const auto separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto address = parse_address(text.substr(0, separator));
    const auto length = parse_number(text.substr(separator + 1U));
    if (!address.has_value() || !length.has_value() || *length == 0U
        || static_cast<std::uint64_t>(*address) + *length > 65'536U) {
        return std::nullopt;
    }
    return MemoryDump{*address, static_cast<std::uint32_t>(*length)};
}

std::optional<MemoryWatchpoint> parse_watchpoint(std::string_view text) {
    const auto separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto address = parse_address(text.substr(separator + 1U));
    if (!address.has_value()) {
        return std::nullopt;
    }
    using jr800::debugger::MemoryWatchpointMode;
    const auto mode = text.substr(0U, separator);
    if (mode == "read") {
        return MemoryWatchpoint{*address, MemoryWatchpointMode::read};
    }
    if (mode == "write") {
        return MemoryWatchpoint{*address, MemoryWatchpointMode::write};
    }
    if (mode == "access") {
        return MemoryWatchpoint{*address, MemoryWatchpointMode::access};
    }
    return std::nullopt;
}

std::optional<ExecutionBreakpoint> parse_conditional_breakpoint(
    std::string_view text
) {
    const auto separator = text.find(':');
    if (separator == std::string_view::npos || separator == 0U
        || separator + 1U == text.size()) {
        return std::nullopt;
    }
    const auto address = parse_address(text.substr(0U, separator));
    if (!address.has_value()) {
        return std::nullopt;
    }
    return ExecutionBreakpoint{
        *address,
        std::string{text.substr(separator + 1U)},
    };
}

std::optional<CliOptions> parse_options(int argc, char* argv[]) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&](std::string_view option) -> std::optional<std::string> {
            if (index + 1 >= argc) {
                std::cerr << "jr8run: missing value for " << option << '\n';
                return std::nullopt;
            }
            ++index;
            return std::string{argv[index]};
        };
        if (argument == "--debug") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.debug = *value;
        } else if (argument == "--break") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto address = parse_address(*value);
            if (!address.has_value()) {
                std::cerr << "jr8run: invalid address for " << argument << '\n';
                return std::nullopt;
            }
            options.execution_breakpoints.push_back(
                ExecutionBreakpoint{*address, {}}
            );
        } else if (argument == "--break-if") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto breakpoint = parse_conditional_breakpoint(*value);
            if (!breakpoint.has_value()) {
                std::cerr << "jr8run: invalid conditional breakpoint\n";
                return std::nullopt;
            }
            options.execution_breakpoints.push_back(*breakpoint);
        } else if (argument == "--watch") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto watchpoint = parse_watchpoint(*value);
            if (!watchpoint.has_value()) {
                std::cerr << "jr8run: invalid memory watchpoint\n";
                return std::nullopt;
            }
            options.memory_watchpoints.push_back(*watchpoint);
        } else if (argument == "--run-to") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto address = parse_address(*value);
            if (!address.has_value()) {
                std::cerr << "jr8run: invalid run-to address\n";
                return std::nullopt;
            }
            options.run_to_address = *address;
        } else if (argument == "--run-to-source") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto location = parse_source_location(*value);
            if (!location.has_value()) {
                std::cerr << "jr8run: invalid run-to-source location\n";
                return std::nullopt;
            }
            options.run_to_source = *location;
        } else if (argument == "--run-to-symbol") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            if (value->empty()) {
                std::cerr << "jr8run: symbol name must be nonempty\n";
                return std::nullopt;
            }
            options.run_to_symbol = std::string{*value};
        } else if (argument == "--trace") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto filter = parse_trace_filter(*value);
            if (!filter.has_value()) {
                std::cerr << "jr8run: invalid access trace filter\n";
                return std::nullopt;
            }
            options.trace_filter = *filter;
        } else if (argument == "--step-over") {
            options.step_over = true;
        } else if (argument == "--step-out") {
            options.step_out = true;
        } else if (argument == "--max-instructions") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto count = parse_number(*value);
            if (!count.has_value() || *count == 0U) {
                std::cerr << "jr8run: instruction limit must be nonzero\n";
                return std::nullopt;
            }
            options.instruction_limit = *count;
        } else if (argument == "--stack") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto address = parse_address(*value);
            if (!address.has_value()) {
                std::cerr << "jr8run: invalid stack address\n";
                return std::nullopt;
            }
            options.stack_pointer = *address;
        } else if (argument == "--dump") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto dump = parse_dump(*value);
            if (!dump.has_value()) {
                std::cerr << "jr8run: invalid memory dump range\n";
                return std::nullopt;
            }
            options.dumps.push_back(*dump);
        } else if (argument == "--expect") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.expected_expressions.push_back(*value);
        } else if (argument == "--expect-stop") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.expected_stop = *value;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8run: unknown option: " << argument << '\n';
            return std::nullopt;
        } else if (!options.application.empty()) {
            std::cerr << "jr8run: exactly one JR8APP input is required\n";
            return std::nullopt;
        } else {
            options.application = argument;
        }
    }
    if (options.application.empty()) {
        std::cerr << "jr8run: one JR8APP input is required\n";
        return std::nullopt;
    }
    const auto control_mode_count = static_cast<unsigned int>(
        options.step_over
    ) + static_cast<unsigned int>(options.step_out)
        + static_cast<unsigned int>(options.run_to_address.has_value())
        + static_cast<unsigned int>(options.run_to_source.has_value())
        + static_cast<unsigned int>(options.run_to_symbol.has_value());
    if (control_mode_count > 1U) {
        std::cerr << "jr8run: step-over, step-out, run-to, run-to-source, and "
                     "run-to-symbol are mutually exclusive\n";
        return std::nullopt;
    }
    if ((options.run_to_source.has_value() || options.run_to_symbol.has_value())
        && !options.debug.has_value()) {
        std::cerr << "jr8run: source and symbol targets require --debug\n";
        return std::nullopt;
    }
    return options;
}

std::optional<std::vector<std::uint8_t>> read_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8run: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

std::string hex_byte(std::uint8_t value) {
    std::ostringstream stream;
    stream << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<unsigned int>(value);
    return stream.str();
}

std::string hex_address(std::uint16_t value) {
    std::ostringstream stream;
    stream << '$' << std::uppercase << std::hex << std::setw(4)
           << std::setfill('0') << value;
    return stream.str();
}

std::string_view stop_name(jr800::debugger::StopReason reason) {
    using jr800::debugger::StopReason;
    switch (reason) {
    case StopReason::step_complete:
        return "step-complete";
    case StopReason::instruction_limit:
        return "instruction-limit";
    case StopReason::execution_breakpoint:
        return "execution-breakpoint";
    case StopReason::memory_watchpoint:
        return "memory-watchpoint";
    case StopReason::cpu_fault:
        return "cpu-fault";
    case StopReason::detached:
        return "detached";
    case StopReason::sleeping:
        return "sleeping";
    case StopReason::address_reached:
        return "address-reached";
    case StopReason::step_out_complete:
        return "step-out-complete";
    case StopReason::breakpoint_condition_error:
        return "breakpoint-condition-error";
    }
    return "unknown";
}

std::string_view expression_compile_error_name(
    jr800::debugger::ExpressionCompileError error
) noexcept {
    using Error = jr800::debugger::ExpressionCompileError;
    switch (error) {
    case Error::none:
        return "none";
    case Error::empty:
        return "empty";
    case Error::too_long:
        return "too-long";
    case Error::invalid_token:
        return "invalid-token";
    case Error::invalid_syntax:
        return "invalid-syntax";
    case Error::unknown_identifier:
        return "unknown-identifier";
    case Error::too_complex:
        return "too-complex";
    }
    return "unknown";
}

std::string_view expression_evaluation_error_name(
    jr800::debugger::ExpressionEvaluationError error
) noexcept {
    using Error = jr800::debugger::ExpressionEvaluationError;
    switch (error) {
    case Error::none:
        return "none";
    case Error::unknown_state:
        return "unknown-state";
    case Error::memory_access:
        return "memory-access";
    case Error::division_by_zero:
        return "division-by-zero";
    case Error::invalid_shift:
        return "invalid-shift";
    case Error::address_out_of_range:
        return "address-out-of-range";
    case Error::symbol_not_found:
        return "symbol-not-found";
    case Error::ambiguous_symbol:
        return "ambiguous-symbol";
    }
    return "unknown";
}

std::string_view load_error_name(jr800::runtime::LoadApplicationResult result) {
    using jr800::runtime::LoadApplicationResult;
    switch (result) {
    case LoadApplicationResult::loaded:
        return "loaded";
    case LoadApplicationResult::invalid_format:
        return "invalid-format";
    case LoadApplicationResult::unknown_profile:
        return "unknown-profile";
    case LoadApplicationResult::unreviewed_profile:
        return "unreviewed-profile";
    case LoadApplicationResult::segment_out_of_range:
        return "segment-out-of-range";
    case LoadApplicationResult::target_mismatch:
        return "target-mismatch";
    }
    return "unknown";
}

std::uint16_t stop_pc(const jr800::debugger::StopInfo& stop) {
    using jr800::debugger::StopReason;
    if (stop.reason == StopReason::memory_watchpoint
        || stop.reason == StopReason::cpu_fault) {
        return stop.step.pc_before;
    }
    return stop.trigger_address;
}

std::string_view access_name(jr800::core::AccessKind access) noexcept {
    using jr800::core::AccessKind;
    switch (access) {
    case AccessKind::instruction_fetch:
        return "instruction-fetch";
    case AccessKind::data_read:
        return "data-read";
    case AccessKind::data_write:
        return "data-write";
    }
    return "unknown";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "jr8run " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }
    if (argc >= 2 && std::string_view{argv[1]} == "jr800") {
        return jr800::tools::run_jr800_command(argc - 1, argv + 1);
    }
    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        print_usage(std::cerr);
        return 2;
    }

    std::vector<std::unique_ptr<jr800::debugger::CompiledExpression>>
        compiled_expectations;
    compiled_expectations.reserve(options->expected_expressions.size());
    for (const auto& expectation : options->expected_expressions) {
        jr800::debugger::ExpressionCompileDiagnostic diagnostic;
        auto compiled = jr800::debugger::compile_expression(
            expectation,
            diagnostic
        );
        if (!compiled) {
            std::cerr << "jr8run: invalid expectation at byte "
                      << diagnostic.offset << ": "
                      << expression_compile_error_name(diagnostic.error) << '\n';
            return 2;
        }
        compiled_expectations.push_back(std::move(compiled));
    }

    const auto application_bytes = read_binary(options->application);
    if (!application_bytes.has_value()) {
        return 2;
    }
    jr800::formats::jr8app::Application application;
    try {
        application = jr800::formats::jr8app::read(*application_bytes);
    } catch (const jr800::formats::linked::Error& error) {
        std::cerr << "jr8run: invalid JR8APP: " << error.what() << '\n';
        return 1;
    }

    jr800::core::SyntheticMachine machine;
    const auto load_result = jr800::runtime::load_application(
        machine,
        application,
        options->stack_pointer
    );
    if (load_result != jr800::runtime::LoadApplicationResult::loaded) {
        std::cerr << "jr8run: application load failed: "
                  << load_error_name(load_result) << '\n';
        return 1;
    }

    jr800::debugger::Debugger debugger;
    if (!debugger.attach(machine.execution())) {
        std::cerr << "jr8run: debugger attach failed\n";
        return 1;
    }
    std::optional<jr800::formats::jr8dbg::DebugInfo> debug_info;
    if (options->debug.has_value()) {
        const auto debug_bytes = read_binary(*options->debug);
        if (!debug_bytes.has_value()) {
            return 2;
        }
        try {
            debug_info = jr800::formats::jr8dbg::read(*debug_bytes);
        } catch (const jr800::formats::linked::Error& error) {
            std::cerr << "jr8run: invalid JR8DBG: " << error.what() << '\n';
            return 1;
        }
        const auto debug_result = debugger.load_debug_info(
            *debug_info,
            application.integrity_sha256
        );
        if (debug_result != jr800::debugger::DebugInfoLoadResult::loaded) {
            std::cerr << "jr8run: JR8DBG does not match the loaded application\n";
            return 1;
        }
    }
    for (const auto& breakpoint : options->execution_breakpoints) {
        if (breakpoint.condition.empty()) {
            debugger.set_execution_breakpoint(breakpoint.address, true);
            continue;
        }
        const auto result = debugger.set_conditional_execution_breakpoint(
            breakpoint.address,
            breakpoint.condition
        );
        if (!result.succeeded()) {
            std::cerr << "jr8run: invalid breakpoint condition at byte "
                      << result.offset << ": "
                      << expression_compile_error_name(result.error) << '\n';
            return 2;
        }
    }
    for (const auto& watchpoint : options->memory_watchpoints) {
        debugger.set_memory_watchpoint(
            watchpoint.address,
            watchpoint.mode,
            true
        );
    }

    jr800::debugger::StopInfo stop;
    std::optional<jr800::debugger::StepOutState> step_out_state;
    std::optional<std::uint16_t> source_target_address;
    std::optional<std::uint16_t> symbol_target_address;
    if (options->step_over) {
        stop = debugger.step_over(options->instruction_limit);
    } else if (options->step_out) {
        const auto result = debugger.step_out(options->instruction_limit);
        stop = result.stop;
        if (stop.reason == jr800::debugger::StopReason::instruction_limit
            || stop.reason == jr800::debugger::StopReason::sleeping) {
            step_out_state = result.state;
        }
    } else if (options->run_to_address.has_value()) {
        stop = debugger.run_to(
            *options->run_to_address,
            options->instruction_limit
        );
    } else if (options->run_to_source.has_value()) {
        source_target_address = debugger.source_address(
            options->run_to_source->path,
            options->run_to_source->line
        );
        if (!source_target_address.has_value()) {
            std::cerr << "jr8run: source location not found: "
                      << options->run_to_source->path << ':'
                      << options->run_to_source->line << '\n';
            return 1;
        }
        stop = debugger.run_to(
            *source_target_address,
            options->instruction_limit
        );
    } else if (options->run_to_symbol.has_value()) {
        const auto resolved = debugger.symbol_address(*options->run_to_symbol);
        if (!resolved.succeeded()) {
            const auto message = resolved.status
                    == jr800::debugger::SymbolAddressStatus::ambiguous
                ? "symbol is ambiguous"
                : resolved.status
                        == jr800::debugger::SymbolAddressStatus::not_address
                    ? "symbol is not an address"
                    : "symbol not found";
            std::cerr << "jr8run: " << message << ": "
                      << *options->run_to_symbol << '\n';
            return 1;
        }
        symbol_target_address = resolved.address;
        stop = debugger.run_to(
            *symbol_target_address,
            options->instruction_limit
        );
    } else {
        stop = debugger.run(options->instruction_limit);
    }
    const auto& state = machine.execution().cpu().state();
    std::cout << "Stop: " << stop_name(stop.reason) << '\n'
              << "Instructions: " << stop.instructions_executed << '\n'
              << "PC: " << hex_address(state.pc) << '\n'
              << "Cycles: " << state.cycle_count << '\n'
              << "A: $" << hex_byte(state.a) << '\n';
    if (stop.reason == jr800::debugger::StopReason::memory_watchpoint
        && stop.trigger_access.has_value()) {
        std::cout << "Memory watchpoint: " << hex_address(stop.trigger_address)
                  << " (" << access_name(*stop.trigger_access) << ")\n";
    }
    if (stop.reason == jr800::debugger::StopReason::address_reached) {
        std::cout << "Address reached: " << hex_address(stop.trigger_address)
                  << '\n';
    }
    if (stop.reason
        == jr800::debugger::StopReason::breakpoint_condition_error) {
        std::cout << "Breakpoint condition: "
                  << expression_evaluation_error_name(stop.condition_error)
                  << '\n';
    }
    if (source_target_address.has_value()) {
        std::cout << "Source target: " << options->run_to_source->path << ':'
                  << options->run_to_source->line << " -> "
                  << hex_address(*source_target_address) << '\n';
    }
    if (symbol_target_address.has_value()) {
        std::cout << "Symbol target: " << *options->run_to_symbol << " -> "
                  << hex_address(*symbol_target_address) << '\n';
    }
    if (stop.continuation_address.has_value()) {
        std::cout << "Step-over continuation: "
                  << hex_address(*stop.continuation_address) << '\n';
    }
    if (step_out_state.has_value()) {
        std::cout << "Step-out continuation: continued="
                  << (step_out_state->continued ? "true" : "false")
                  << " depth=" << step_out_state->nesting_depth << '\n';
    }
    if (debug_info.has_value()) {
        const auto* mapping = debugger.source_at(stop_pc(stop));
        if (mapping != nullptr) {
            const auto& source = debug_info->source_files[mapping->source_file_index];
            std::cout << "Source: " << source.path << ':' << mapping->line << ':'
                      << mapping->column << '\n';
        }
    }

    std::cout << "History:\n";
    for (const auto& entry : debugger.history()) {
        const auto disassembly = debugger.disassemble(entry.pc_before);
        std::cout << "  #" << entry.sequence << ' ' << hex_address(entry.pc_before)
                  << ' ' << (disassembly.has_value() ? disassembly->text : "<detached>")
                  << " -> " << hex_address(entry.pc_after)
                  << " cycles=" << static_cast<unsigned int>(entry.cycles)
                  << " accesses=" << entry.access_count << '\n';
    }
    if (options->trace_filter.has_value()) {
        std::cout << "Trace:\n";
        for (const auto& access : debugger.memory_accesses(
                 *options->trace_filter
             )) {
            std::cout << "  #" << access.sequence
                      << " pc=" << hex_address(access.instruction_pc)
                      << ' ' << access_name(access.kind)
                      << " address=" << hex_address(access.address)
                      << " value="
                      << (access.value_known
                              ? '$' + hex_byte(access.value)
                              : "??")
                      << " previous="
                      << (access.previous_value_known
                              ? '$' + hex_byte(access.previous_value)
                              : "??")
                      << '\n';
        }
    }
    for (const auto& dump : options->dumps) {
        std::cout << "Memory " << hex_address(dump.address) << ':';
        for (std::uint32_t offset = 0; offset < dump.length; ++offset) {
            std::cout << ' ' << hex_byte(machine.bus().peek8(
                static_cast<std::uint16_t>(dump.address + offset)
            ));
        }
        std::cout << '\n';
    }

    bool expectations_passed = true;
    if (options->expected_stop.has_value()
        && *options->expected_stop != stop_name(stop.reason)) {
        std::cerr << "jr8run: expected stop " << *options->expected_stop
                  << " but got " << stop_name(stop.reason) << '\n';
        expectations_passed = false;
    }
    for (std::size_t index = 0U; index < compiled_expectations.size(); ++index) {
        const auto result = debugger.evaluate_expression(
            *compiled_expectations[index]
        );
        if (!result.has_value()) {
            std::cerr << "jr8run: expectation evaluation failed: "
                      << options->expected_expressions[index]
                      << ": debugger-detached\n";
            expectations_passed = false;
        } else if (!result->succeeded()) {
            std::cerr << "jr8run: expectation evaluation failed: "
                      << options->expected_expressions[index] << ": "
                      << expression_evaluation_error_name(result->error) << '\n';
            expectations_passed = false;
        } else if (result->value == 0U) {
            std::cerr << "jr8run: expectation failed: "
                      << options->expected_expressions[index] << '\n';
            expectations_passed = false;
        }
    }
    if (!expectations_passed || stop.reason == jr800::debugger::StopReason::cpu_fault
        || stop.reason == jr800::debugger::StopReason::detached
        || stop.reason
            == jr800::debugger::StopReason::breakpoint_condition_error) {
        return 1;
    }
    return 0;
}
