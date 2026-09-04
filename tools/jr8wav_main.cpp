// SPDX-License-Identifier: MIT

#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "distinct_path_set.hpp"
#include "jr800/formats/fsk_wav.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/native_msave.hpp"
#include "jr800/formats/jr8rom.hpp"
#include "jr800/formats/rom_dump.hpp"
#include "jr800/formats/sha256.hpp"

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

namespace {

struct EncodeOptions {
    std::uint16_t address{};
    std::uint16_t block_size{jr800::formats::default_rom_dump_block_size};
    std::filesystem::path input;
    std::filesystem::path output;
};

bool paths_are_distinct(
    const std::filesystem::path& input,
    const std::filesystem::path& output
) {
    jr800::tools::DistinctPathSet paths;
    const auto input_result = paths.insert(input);
    const auto output_result = paths.insert(output);
    if (input_result == jr800::tools::PathInsertResult::inspection_error
        || output_result == jr800::tools::PathInsertResult::inspection_error) {
        std::cerr << "jr8wav: cannot resolve input/output path identity\n";
        return false;
    }
    if (input_result != jr800::tools::PathInsertResult::inserted
        || output_result != jr800::tools::PathInsertResult::inserted) {
        std::cerr << "jr8wav: input and output paths must be distinct\n";
        return false;
    }
    return true;
}

bool paths_are_distinct(
    const std::filesystem::path& first_input,
    const std::filesystem::path& second_input,
    const std::filesystem::path& output
) {
    jr800::tools::DistinctPathSet paths;
    const std::array candidates{first_input, second_input, output};
    for (const auto& candidate : candidates) {
        const auto result = paths.insert(candidate);
        if (result == jr800::tools::PathInsertResult::inspection_error) {
            std::cerr << "jr8wav: cannot resolve path identity\n";
            return false;
        }
        if (result != jr800::tools::PathInsertResult::inserted) {
            std::cerr << "jr8wav: input and output paths must all be distinct\n";
            return false;
        }
    }
    return true;
}

void print_usage(std::ostream& stream) {
    stream << "Usage:\n"
              "  jr8wav encode --address <address> [--block-size <bytes>] "
              "<input.bin> <output.wav>\n"
              "  jr8wav decode <input.wav> <output.j8r>\n"
              "  jr8wav decode-native-program <input.wav> <output.j8a>\n"
              "  jr8wav decode-native-msave <input.wav> <output.j8r>\n"
              "  jr8wav verify-native-msave <first.wav> <second.wav> "
              "<output.j8r>\n";
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
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value,
        base
    );
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<EncodeOptions> parse_encode_options(int argc, char* argv[]) {
    EncodeOptions options;
    bool address_seen = false;
    std::vector<std::filesystem::path> paths;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&](std::string_view option) -> std::optional<std::string> {
            if (index + 1 >= argc) {
                std::cerr << "jr8wav: missing value for " << option << '\n';
                return std::nullopt;
            }
            ++index;
            return std::string{argv[index]};
        };
        if (argument == "--address" || argument == "--block-size") {
            const auto text = require_value(argument);
            if (!text.has_value()) {
                return std::nullopt;
            }
            const auto value = parse_number(*text);
            if (!value.has_value()) {
                std::cerr << "jr8wav: invalid number for " << argument << '\n';
                return std::nullopt;
            }
            if (argument == "--address") {
                if (*value > 0xFFFFU) {
                    std::cerr << "jr8wav: address is outside the 16-bit range\n";
                    return std::nullopt;
                }
                options.address = static_cast<std::uint16_t>(*value);
                address_seen = true;
            } else {
                if (*value == 0U || *value > 4'096U) {
                    std::cerr << "jr8wav: block size must be between 1 and 4096\n";
                    return std::nullopt;
                }
                options.block_size = static_cast<std::uint16_t>(*value);
            }
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "jr8wav: unknown option: " << argument << '\n';
            return std::nullopt;
        } else {
            paths.emplace_back(argument);
        }
    }
    if (!address_seen || paths.size() != 2U) {
        std::cerr << "jr8wav: encode requires --address, one input, and one output\n";
        return std::nullopt;
    }
    options.input = std::move(paths[0]);
    options.output = std::move(paths[1]);
    if (!paths_are_distinct(options.input, options.output)) {
        return std::nullopt;
    }
    return options;
}

std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "jr8wav: cannot open input: " << path.string() << '\n';
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

bool write_file(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "jr8wav: cannot open output: " << path.string() << '\n';
        return false;
    }
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!output) {
        std::cerr << "jr8wav: failed writing output: " << path.string() << '\n';
        return false;
    }
    return true;
}

bool write_jr8rom(
    const std::filesystem::path& path,
    std::uint16_t address,
    std::span<const std::uint8_t> payload
) {
    try {
        jr800::formats::jr8rom::Image image;
        image.segments = {{
            address,
            std::vector<std::uint8_t>{payload.begin(), payload.end()},
        }};
        image.integrity_sha256 = jr800::formats::jr8rom::compute_integrity(image);
        const auto encoded = jr800::formats::jr8rom::write(image);
        return write_file(path, encoded);
    } catch (const std::exception& error) {
        std::cerr << "jr8wav: cannot encode JR8ROM: " << error.what() << '\n';
        return false;
    }
}

bool write_jr8app(
    const std::filesystem::path& path,
    std::uint16_t address,
    std::uint16_t entry_point,
    std::span<const std::uint8_t> payload
) {
    try {
        jr800::formats::jr8app::Application application;
        application.target_profile = "hd6301v1";
        application.entry_point = entry_point;
        application.segments = {{
            jr800::formats::jr8app::SegmentKind::data,
            address,
            static_cast<std::uint32_t>(payload.size()),
            std::vector<std::uint8_t>{payload.begin(), payload.end()},
        }};
        application.integrity_sha256 =
            jr800::formats::jr8app::compute_integrity(application);
        const auto encoded = jr800::formats::jr8app::write(application);
        return write_file(path, encoded);
    } catch (const std::exception& error) {
        std::cerr << "jr8wav: cannot encode JR8APP: " << error.what() << '\n';
        return false;
    }
}

std::string digest_text(std::span<const std::uint8_t> bytes) {
    const auto digest = jr800::formats::sha256(bytes);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

int encode(const EncodeOptions& options) {
    const auto payload = read_file(options.input);
    if (!payload.has_value()) {
        return 1;
    }
    try {
        const auto frames = jr800::formats::make_rom_dump_frames(
            options.address,
            *payload,
            options.block_size
        );
        const auto wav = jr800::formats::encode_fsk_wav(frames);
        if (!write_file(options.output, wav)) {
            return 1;
        }
        std::cout << "encoded address=$" << std::uppercase << std::hex
                  << std::setw(4) << std::setfill('0') << options.address
                  << std::dec << " length=" << payload->size()
                  << " blocks=" << frames.size() - 1U
                  << " sha256=" << digest_text(*payload) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "jr8wav: " << error.what() << '\n';
        return 1;
    }
}

int decode(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path
) {
    const auto wav = read_file(input_path);
    if (!wav.has_value()) {
        return 1;
    }
    const auto decoded = jr800::formats::decode_fsk_wav(*wav);
    for (const auto& issue : decoded.issues) {
        std::cerr << "jr8wav: " << jr800::formats::fsk_wav_issue_name(issue.code)
                  << " in burst " << issue.burst_index << '\n';
    }
    const auto recovery = jr800::formats::recover_rom_dump(decoded.frames);
    for (const auto& issue : recovery.issues) {
        std::cerr << "jr8wav: " << jr800::formats::rom_dump_issue_name(issue.code);
        if (issue.block_number.has_value()) {
            std::cerr << " for block " << *issue.block_number;
        }
        std::cerr << '\n';
    }
    if (!decoded.issues.empty() || !recovery.complete) {
        std::cerr << "jr8wav: refusing incomplete or unverified output\n";
        return 2;
    }
    if (!write_jr8rom(
            output_path,
            recovery.segment_address,
            recovery.payload
        )) {
        return 1;
    }
    std::cout << "decoded address=$" << std::uppercase << std::hex
              << std::setw(4) << std::setfill('0') << recovery.segment_address
              << std::dec << " length=" << recovery.payload.size()
              << " sha256=" << digest_text(recovery.payload) << '\n';
    return 0;
}

void print_native_issues(
    const jr800::formats::NativeMsaveDecodeResult& decoded,
    std::string_view label
) {
    for (const auto& issue : decoded.issues) {
        std::cerr << "jr8wav: " << label << ": native-msave-"
                  << jr800::formats::native_msave_issue_name(issue.code);
        if (issue.code
            == jr800::formats::NativeMsaveIssueCode::unexpected_burst_count) {
            std::cerr << " observed=" << issue.burst_index;
        } else {
            std::cerr << " in burst " << issue.burst_index;
        }
        std::cerr << '\n';
    }
}

void print_native_summary(
    std::string_view action,
    const jr800::formats::NativeMsaveFile& file
) {
    std::cout << action << " name=\"" << file.filename << "\" address=$"
              << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
              << file.start_address << " execution=$" << std::setw(4)
              << file.execution_address << std::dec
              << " length=" << file.payload.size()
              << " channel-index=" << file.source_channel
              << " header-byte-order="
              << (file.header_byte_order
                          == jr800::formats::NativeMsaveByteOrder::little_endian
                      ? "little-endian"
                      : "big-endian")
              << " header-layout="
              << (file.header_layout
                          == jr800::formats::NativeMsaveHeaderLayout::compact_fields
                      ? "compact-fields"
                      : "reserved-byte-before-fields")
              << " sha256=" << digest_text(file.payload) << '\n';
}

int decode_native_program(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path
) {
    const auto wav = read_file(input_path);
    if (!wav.has_value()) {
        return 1;
    }
    const auto decoded = jr800::formats::decode_native_program_wav(*wav);
    print_native_issues(decoded, "input");
    if (!decoded.issues.empty() || !decoded.file.has_value()) {
        std::cerr << "jr8wav: refusing incomplete or unverified native program output\n";
        return 2;
    }
    if (!write_jr8app(
            output_path,
            decoded.file->start_address,
            decoded.file->execution_address,
            decoded.file->payload
        )) {
        return 1;
    }
    print_native_summary("decoded-native-program", *decoded.file);
    return 0;
}

int decode_native_msave(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path
) {
    const auto wav = read_file(input_path);
    if (!wav.has_value()) {
        return 1;
    }
    const auto decoded = jr800::formats::decode_native_msave_wav(*wav);
    print_native_issues(decoded, "input");
    if (!decoded.issues.empty() || !decoded.file.has_value()) {
        std::cerr << "jr8wav: refusing incomplete or unverified native MSAVE output\n";
        return 2;
    }
    if (!write_jr8rom(
            output_path,
            decoded.file->start_address,
            decoded.file->payload
        )) {
        return 1;
    }
    print_native_summary("decoded-native-msave", *decoded.file);
    return 0;
}

bool native_files_match(
    const jr800::formats::NativeMsaveFile& first,
    const jr800::formats::NativeMsaveFile& second
) {
    return first.filename == second.filename
        && first.start_address == second.start_address
        && first.execution_address == second.execution_address
        && first.payload == second.payload;
}

int verify_native_msave(
    const std::filesystem::path& first_path,
    const std::filesystem::path& second_path,
    const std::filesystem::path& output_path
) {
    const auto first_wav = read_file(first_path);
    const auto second_wav = read_file(second_path);
    if (!first_wav.has_value() || !second_wav.has_value()) {
        return 1;
    }
    if (*first_wav == *second_wav) {
        std::cerr << "jr8wav: native MSAVE recording files are identical; "
                     "refusing independence claim\n";
        return 2;
    }
    const auto first = jr800::formats::decode_native_msave_wav(*first_wav);
    const auto second = jr800::formats::decode_native_msave_wav(*second_wav);
    print_native_issues(first, "first input");
    print_native_issues(second, "second input");
    if (!first.issues.empty() || !second.issues.empty()
        || !first.file.has_value() || !second.file.has_value()) {
        std::cerr << "jr8wav: refusing incomplete or unverified native MSAVE output\n";
        return 2;
    }
    if (!native_files_match(*first.file, *second.file)) {
        std::cerr << "jr8wav: independent native MSAVE recordings differ; "
                     "refusing output\n";
        return 2;
    }
    if (!write_jr8rom(
            output_path,
            first.file->start_address,
            first.file->payload
        )) {
        return 1;
    }
    print_native_summary("verified-native-msave recordings=2", *first.file);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "jr8wav " << JR800_PROJECT_VERSION << '\n';
        return 0;
    }
    if (argc >= 2 && std::string_view{argv[1]} == "encode") {
        const auto options = parse_encode_options(argc, argv);
        if (!options.has_value()) {
            print_usage(std::cerr);
            return 1;
        }
        return encode(*options);
    }
    if (argc == 4 && std::string_view{argv[1]} == "decode") {
        if (!paths_are_distinct(argv[2], argv[3])) {
            return 1;
        }
        return decode(argv[2], argv[3]);
    }
    if (argc == 4 && std::string_view{argv[1]} == "decode-native-msave") {
        if (!paths_are_distinct(argv[2], argv[3])) {
            return 1;
        }
        return decode_native_msave(argv[2], argv[3]);
    }
    if (argc == 4 && std::string_view{argv[1]} == "decode-native-program") {
        if (!paths_are_distinct(argv[2], argv[3])) {
            return 1;
        }
        return decode_native_program(argv[2], argv[3]);
    }
    if (argc == 5 && std::string_view{argv[1]} == "verify-native-msave") {
        if (!paths_are_distinct(argv[2], argv[3], argv[4])) {
            return 1;
        }
        return verify_native_msave(argv[2], argv[3], argv[4]);
    }
    print_usage(std::cerr);
    return 1;
}
