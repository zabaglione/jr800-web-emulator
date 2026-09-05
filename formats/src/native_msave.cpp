// SPDX-License-Identifier: MIT

#include "jr800/formats/native_msave.hpp"

#include "wav_pcm.hpp"
#include "jr800/formats/basic_program.hpp"
#include "jr800/formats/linked_error.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace jr800::formats {
namespace {

constexpr std::size_t kHeaderBodySize = 32U;
constexpr std::size_t kChecksumSize = 2U;
constexpr std::size_t kHeaderBlockSize = kHeaderBodySize + kChecksumSize;
constexpr std::uint8_t kMsaveHeaderType = 0x01U;
constexpr std::size_t kMaximumObservedPayloadSize = 32'768U;

struct SampleRange {
    std::size_t begin{};
    std::size_t end{};
};

struct CycleRun {
    bool long_period{};
    std::size_t count{};
};

struct ZeroCrossing {
    double position{};
    bool rising{};
};

struct HeaderFields {
    NativeMsaveByteOrder byte_order{NativeMsaveByteOrder::little_endian};
    NativeMsaveHeaderLayout layout{NativeMsaveHeaderLayout::compact_fields};
    std::size_t payload_length{};
    std::uint16_t start_address{};
    std::uint16_t execution_address{};
};

std::uint16_t read_be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | static_cast<std::uint16_t>(bytes[offset + 1U])
    );
}

std::uint16_t read_le16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U
        );
}

HeaderFields read_header_fields(
    std::span<const std::uint8_t> header,
    NativeMsaveByteOrder byte_order,
    NativeMsaveHeaderLayout layout
) {
    const auto read = byte_order == NativeMsaveByteOrder::little_endian
        ? read_le16
        : read_be16;
    const std::size_t offset = layout
            == NativeMsaveHeaderLayout::compact_fields
        ? 17U
        : 18U;
    return {
        byte_order,
        layout,
        static_cast<std::size_t>(read(header, offset)),
        read(header, offset + 2U),
        read(header, offset + 4U),
    };
}

bool valid_header_layout(
    std::span<const std::uint8_t> header,
    NativeMsaveHeaderLayout layout
) {
    const auto reserved_begin = layout
            == NativeMsaveHeaderLayout::compact_fields
        ? 23U
        : 24U;
    if (layout == NativeMsaveHeaderLayout::reserved_byte_before_fields
        && header[17U] != 0U) {
        return false;
    }
    return std::all_of(
        header.begin() + static_cast<std::ptrdiff_t>(reserved_begin),
        header.end(),
        [](std::uint8_t byte) { return byte == 0U; }
    );
}

bool valid_header_fields(const HeaderFields& fields) noexcept {
    return fields.payload_length != 0U
        && fields.payload_length <= kMaximumObservedPayloadSize
        && static_cast<std::uint32_t>(fields.start_address)
                + fields.payload_length
            <= 0x1'0000U;
}

bool valid_program_range(const HeaderFields& fields) noexcept {
    const auto begin = static_cast<std::uint32_t>(fields.start_address);
    const auto end = begin + fields.payload_length;
    return begin >= 0x2000U && end <= 0x8000U;
}

std::uint16_t additive_sum(std::span<const std::uint8_t> bytes) {
    std::uint32_t sum = 0U;
    for (const auto byte : bytes) {
        sum += byte;
    }
    return static_cast<std::uint16_t>(sum & 0xFFFFU);
}

std::size_t strongest_channel(
    const detail::WavPcm16& wav,
    std::vector<double>& selected_samples
) {
    std::size_t selected = 0U;
    double selected_energy = -1.0;
    for (std::size_t channel = 0U; channel < wav.channels; ++channel) {
        auto samples = detail::centered_channel(wav, channel);
        double energy = 0.0;
        for (const auto sample : samples) {
            energy += sample * sample;
        }
        if (energy > selected_energy) {
            selected = channel;
            selected_energy = energy;
            selected_samples = std::move(samples);
        }
    }
    return selected;
}

std::vector<SampleRange> find_bursts(
    std::span<const double> samples,
    std::uint32_t sample_rate
) {
    const auto peak_iterator = std::max_element(
        samples.begin(),
        samples.end(),
        [](double left, double right) { return std::abs(left) < std::abs(right); }
    );
    if (peak_iterator == samples.end() || std::abs(*peak_iterator) < 64.0) {
        return {};
    }
    const double threshold = std::max(32.0, std::abs(*peak_iterator) * 0.06);
    const auto gap_limit = std::max<std::size_t>(
        8U,
        static_cast<std::size_t>(std::lround(
            static_cast<double>(sample_rate) * 0.001
        ))
    );
    const auto minimum_length = std::max<std::size_t>(
        32U,
        static_cast<std::size_t>(sample_rate / 20U)
    );

    std::vector<SampleRange> ranges;
    std::optional<std::size_t> begin;
    std::size_t last_active = 0U;
    for (std::size_t index = 0U; index < samples.size(); ++index) {
        if (std::abs(samples[index]) >= threshold) {
            if (!begin.has_value()) {
                begin = index;
            }
            last_active = index;
        } else if (begin.has_value() && index - last_active > gap_limit) {
            if (last_active + 1U - *begin >= minimum_length) {
                ranges.push_back({*begin, last_active + 1U});
            }
            begin.reset();
        }
    }
    if (begin.has_value() && last_active + 1U - *begin >= minimum_length) {
        ranges.push_back({*begin, last_active + 1U});
    }
    return ranges;
}

std::vector<CycleRun> measure_cycle_runs(
    std::span<const double> samples,
    SampleRange range,
    std::uint32_t sample_rate
) {
    const auto padding = std::max<std::size_t>(
        8U,
        static_cast<std::size_t>(std::lround(
            static_cast<double>(sample_rate) * 0.004
        ))
    );
    range.begin = range.begin > padding ? range.begin - padding : 0U;
    range.end = std::min(samples.size(), range.end + padding);

    std::vector<ZeroCrossing> crossings;
    crossings.reserve((range.end - range.begin) / 8U);
    double previous = samples[range.begin];
    for (std::size_t index = range.begin + 1U; index < range.end; ++index) {
        const double current = samples[index];
        if ((previous < 0.0 && current >= 0.0)
            || (previous > 0.0 && current <= 0.0)) {
            const double denominator = std::abs(previous) + std::abs(current);
            const double fraction = denominator == 0.0
                ? 0.0
                : std::abs(previous) / denominator;
            crossings.push_back({
                static_cast<double>(index - 1U) + fraction,
                previous < 0.0 && current >= 0.0,
            });
        }
        previous = current;
    }

    const double scale = static_cast<double>(sample_rate) / 48'000.0;
    const double minimum = 7.0 * scale;
    const double boundary = 17.0 * scale;
    const double maximum = 32.0 * scale;
    std::vector<CycleRun> runs;
    bool previous_was_valid = false;
    for (std::size_t index = 0U; index + 1U < crossings.size(); ++index) {
        if (!crossings[index].rising || crossings[index + 1U].rising) {
            continue;
        }
        const double duration =
            crossings[index + 1U].position - crossings[index].position;
        if (duration <= minimum || duration >= maximum) {
            previous_was_valid = false;
            continue;
        }
        const bool long_period = duration >= boundary;
        if (!previous_was_valid || runs.back().long_period != long_period) {
            runs.push_back({long_period, 1U});
        } else {
            ++runs.back().count;
        }
        previous_was_valid = true;
    }
    return runs;
}

bool count_near(std::size_t value, std::size_t expected) {
    const auto tolerance = std::max<std::size_t>(2U, expected / 10U);
    const auto lower = expected > tolerance ? expected - tolerance : 0U;
    return value >= lower && value <= expected + tolerance;
}

std::optional<std::size_t> find_sync_end(
    std::span<const CycleRun> runs,
    std::size_t sync_cycles
) {
    for (std::size_t index = 1U; index + 2U < runs.size(); ++index) {
        if (!runs[index - 1U].long_period
            && runs[index - 1U].count >= 3'000U
            && runs[index].long_period
            && count_near(runs[index].count, sync_cycles)
            && !runs[index + 1U].long_period
            && count_near(runs[index + 1U].count, sync_cycles)
            && runs[index + 2U].long_period
            && runs[index + 2U].count >= 2U) {
            return index + 2U;
        }
    }
    return std::nullopt;
}

std::vector<bool> cycle_bits(
    std::span<const CycleRun> runs,
    std::size_t first_run,
    std::size_t maximum_bits
) {
    std::vector<bool> bits;
    bits.reserve(maximum_bits);
    for (std::size_t index = first_run; index < runs.size(); ++index) {
        const auto remaining = maximum_bits - bits.size();
        const auto cycles = std::min(runs[index].count, remaining);
        bits.insert(bits.end(), cycles, runs[index].long_period);
        if (bits.size() == maximum_bits) {
            break;
        }
    }
    return bits;
}

std::optional<std::vector<std::uint8_t>> decode_block(
    std::span<const double> samples,
    SampleRange range,
    std::uint32_t sample_rate,
    std::size_t sync_cycles,
    std::size_t expected_bytes,
    std::size_t burst_index,
    NativeMsaveDecodeResult& result
) {
    const auto runs = measure_cycle_runs(samples, range, sample_rate);
    const auto first_run = find_sync_end(runs, sync_cycles);
    if (!first_run.has_value()) {
        result.issues.push_back({
            NativeMsaveIssueCode::synchronization_failed,
            burst_index,
        });
        return std::nullopt;
    }
    const auto required_bits = expected_bytes * 9U + 1U;
    const auto bits = cycle_bits(runs, *first_run, required_bits);
    if (bits.size() < 2U || !bits[0] || !bits[1]) {
        result.issues.push_back({NativeMsaveIssueCode::framing_error, burst_index});
        return std::nullopt;
    }

    std::size_t position = 2U;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(expected_bytes);
    for (std::size_t byte_index = 0U; byte_index < expected_bytes; ++byte_index) {
        if (position > bits.size() || bits.size() - position < 8U) {
            result.issues.push_back({
                NativeMsaveIssueCode::truncated_block,
                burst_index,
            });
            return std::nullopt;
        }
        std::uint8_t value = 0U;
        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            value = static_cast<std::uint8_t>(value << 1U);
            if (bits[position++]) {
                value = static_cast<std::uint8_t>(value | 1U);
            }
        }
        bytes.push_back(value);
        if (byte_index + 1U < expected_bytes) {
            if (position >= bits.size() || !bits[position]) {
                result.issues.push_back({
                    NativeMsaveIssueCode::framing_error,
                    burst_index,
                });
                return std::nullopt;
            }
            ++position;
        }
    }
    return bytes;
}

bool verify_block_sum(
    std::span<const std::uint8_t> block,
    std::size_t burst_index,
    NativeMsaveDecodeResult& result
) {
    if (block.size() < kChecksumSize) {
        result.issues.push_back({
            NativeMsaveIssueCode::truncated_block,
            burst_index,
        });
        return false;
    }
    const auto body = block.first(block.size() - kChecksumSize);
    const auto stored = read_be16(block, block.size() - kChecksumSize);
    if (stored != additive_sum(body)) {
        result.issues.push_back({
            NativeMsaveIssueCode::checksum_mismatch,
            burst_index,
        });
        return false;
    }
    return true;
}

std::optional<std::string> parse_filename(std::span<const std::uint8_t> field, bool basic = false) {
    std::string filename;
    bool padding_started = false;
    for (const auto byte : field) {
        if (byte == 0U) {
            padding_started = true;
            continue;
        }
        if (padding_started || byte < 0x20U || (!basic && byte > 0x7EU)) {
            return std::nullopt;
        }
        filename.push_back(static_cast<char>(byte));
    }
    if (filename.empty() && !basic) {
        return std::nullopt;
    }
    return filename;
}

}  // namespace

namespace {
template<class Reader>
NativeMsaveDecodeResult decode_program_blocks(std::size_t block_count,
    bool program_input, std::size_t source_channel, Reader read_block) {
    NativeMsaveDecodeResult result;
    if (block_count == 0U || block_count > 130U) {
        result.issues.push_back({NativeMsaveIssueCode::unexpected_burst_count, block_count});
        return result;
    }
    const auto header = read_block(0U, kHeaderBlockSize, result);
    if (!header.has_value() || !verify_block_sum(*header, 0U, result)) {
        return result;
    }
    const auto header_body = std::span<const std::uint8_t>{*header}.first(
        kHeaderBodySize
    );
    if (program_input && (header_body[0] == 2U || header_body[0] == 3U)) {
        const bool text = header_body[0] == 3U;
        const auto name = parse_filename(header_body.subspan(1U, 16U), true);
        const auto reserved = text ? 17U : 24U;
        if (!name.has_value() || header_body[17] != 0U
            || !std::all_of(header_body.begin() + reserved, header_body.end(), [](auto value) { return value == 0U; })
            || (!text && (header_body[22] != 0U || header_body[23] != 0U))) {
            result.issues.push_back({NativeMsaveIssueCode::unsupported_header, 0U});
            return result;
        }
        NativeMsaveFile file;
        file.kind = text ? jr8app::ProgramKind::basic_text : jr8app::ProgramKind::basic_binary;
        file.filename = *name;
        file.source_channel = source_channel;
        file.header_byte_order = NativeMsaveByteOrder::big_endian;
        file.header_layout = NativeMsaveHeaderLayout::reserved_byte_before_fields;
        if (text) {
            bool final = false;
            for (std::size_t index = 1; index < block_count; ++index) {
                if (final) {
                    result.issues.push_back({NativeMsaveIssueCode::unexpected_trailing_blocks, index});
                    return result;
                }
                const auto data = read_block(index, 259U, result);
                if (!data.has_value() || !verify_block_sum(*data, index, result)) return result;
                if ((*data)[0] > 1U) {
                    result.issues.push_back({NativeMsaveIssueCode::invalid_basic_program, index});
                    return result;
                }
                final = (*data)[0] == 1U;
                file.payload.insert(file.payload.end(), data->begin() + 1, data->begin() + 257);
            }
            const auto terminal = std::find(file.payload.begin(), file.payload.end(), 0x1AU);
            const auto size = static_cast<std::size_t>(terminal - file.payload.begin());
            if (!final || terminal == file.payload.end() || size > 32768U
                || (size + 1U) / 256U + 1U != block_count - 1U
                || !std::all_of(terminal + 1, file.payload.end(), [](auto value) { return value == 0U; })) {
                result.issues.push_back({NativeMsaveIssueCode::invalid_basic_program, block_count - 1U});
                return result;
            }
            file.payload.resize(size);
        } else {
            const auto length = read_be16(header_body, 18U);
            const auto start = read_be16(header_body, 20U);
            file.start_address = start;
            if (length < 2U || start < 0x2000U || static_cast<std::uint32_t>(start) + length > 0x8000U) {
                result.issues.push_back({NativeMsaveIssueCode::invalid_basic_program, 0U});
                return result;
            }
            if (block_count > 2U) {
                result.issues.push_back({NativeMsaveIssueCode::unexpected_trailing_blocks, 2U});
                return result;
            }
            const auto data = read_block(1U, static_cast<std::size_t>(length) + 2U, result);
            if (!data.has_value() || !verify_block_sum(*data, 1U, result)) return result;
            file.payload.assign(data->begin(), data->end() - 2);
        }
        try {
            static_cast<void>(native_program_application(file));
        } catch (const linked::Error&) {
            result.issues.push_back({NativeMsaveIssueCode::invalid_basic_program, 1U});
            return result;
        }
        result.file = std::move(file);
        return result;
    }
    if (block_count > 2U) {
        result.issues.push_back({NativeMsaveIssueCode::unexpected_burst_count, block_count});
        return result;
    }
    const auto filename = parse_filename(header_body.subspan(1U, 16U), program_input);
    if (header_body[0] != kMsaveHeaderType || !filename.has_value()) {
        result.issues.push_back({NativeMsaveIssueCode::unsupported_header, 0U});
        return result;
    }

    const auto decode_candidate = [&](
        const HeaderFields& fields,
        NativeMsaveDecodeResult& candidate_result
    ) -> std::optional<NativeMsaveFile> {
        const auto data = read_block(1U, fields.payload_length + kChecksumSize, candidate_result);
        if (!data.has_value()
            || !verify_block_sum(*data, 1U, candidate_result)) {
            return std::nullopt;
        }
        NativeMsaveFile file;
        file.filename = *filename;
        file.start_address = fields.start_address;
        file.execution_address = fields.execution_address;
        file.source_channel = source_channel;
        file.header_byte_order = fields.byte_order;
        file.header_layout = fields.layout;
        const auto payload = std::span<const std::uint8_t>{*data}.first(
            fields.payload_length
        );
        file.payload.assign(payload.begin(), payload.end());
        return file;
    };

    if (!program_input) {
        constexpr auto layout = NativeMsaveHeaderLayout::compact_fields;
        if (!valid_header_layout(header_body, layout)) {
            result.issues.push_back({
                NativeMsaveIssueCode::unsupported_header,
                0U,
            });
            return result;
        }
        const auto fields = read_header_fields(
            header_body,
            NativeMsaveByteOrder::little_endian,
            layout
        );
        if (!valid_header_fields(fields)) {
            result.issues.push_back({NativeMsaveIssueCode::invalid_length, 0U});
            return result;
        }
        result.file = decode_candidate(fields, result);
        return result;
    }

    const std::array candidates{
        read_header_fields(
            header_body,
            NativeMsaveByteOrder::little_endian,
            NativeMsaveHeaderLayout::compact_fields
        ),
        read_header_fields(
            header_body,
            NativeMsaveByteOrder::big_endian,
            NativeMsaveHeaderLayout::compact_fields
        ),
        read_header_fields(
            header_body,
            NativeMsaveByteOrder::little_endian,
            NativeMsaveHeaderLayout::reserved_byte_before_fields
        ),
        read_header_fields(
            header_body,
            NativeMsaveByteOrder::big_endian,
            NativeMsaveHeaderLayout::reserved_byte_before_fields
        ),
    };
    std::vector<NativeMsaveFile> decoded_files;
    NativeMsaveDecodeResult first_failure;
    bool saw_candidate_range = false;
    for (const auto& fields : candidates) {
        if (!valid_header_layout(header_body, fields.layout)
            || !valid_header_fields(fields)
            || !valid_program_range(fields)) {
            continue;
        }
        saw_candidate_range = true;
        NativeMsaveDecodeResult candidate_result;
        const auto file = decode_candidate(fields, candidate_result);
        if (file.has_value()) {
            decoded_files.push_back(*file);
        } else if (first_failure.issues.empty()) {
            first_failure = std::move(candidate_result);
        }
    }
    if (decoded_files.empty()) {
        if (saw_candidate_range) {
            result.issues = std::move(first_failure.issues);
        } else {
            result.issues.push_back({
                NativeMsaveIssueCode::invalid_program_range,
                0U,
            });
        }
        return result;
    }
    const bool interpretations_differ = std::any_of(
        decoded_files.begin() + 1,
        decoded_files.end(),
        [&](const auto& file) {
            return file.start_address != decoded_files.front().start_address
                || file.execution_address
                    != decoded_files.front().execution_address
                || file.payload != decoded_files.front().payload;
        }
    );
    if (interpretations_differ) {
        result.issues.push_back({
            NativeMsaveIssueCode::ambiguous_header_byte_order,
            0U,
        });
        return result;
    }
    result.file = std::move(decoded_files.front());
    return result;
}

} // namespace

NativeMsaveDecodeResult decode_msave_wav(
    std::span<const std::uint8_t> wav_bytes,
    bool program_input
) {
    NativeMsaveDecodeResult result;
    const auto parsed = detail::parse_pcm16_wav(wav_bytes);
    if (!parsed.wav.has_value()) {
        result.issues.push_back({
            parsed.error == detail::WavPcmError::unsupported_wav
                ? NativeMsaveIssueCode::unsupported_wav
                : NativeMsaveIssueCode::invalid_wav,
            0U,
        });
        return result;
    }

    std::vector<double> samples;
    const auto source_channel = strongest_channel(*parsed.wav, samples);
    const auto bursts = find_bursts(samples, parsed.wav->sample_rate);
    if (bursts.empty()) {
        result.issues.push_back({NativeMsaveIssueCode::no_signal, 0U});
        return result;
    }
    if (bursts.size() > 130U) {
        result.issues.push_back({
            NativeMsaveIssueCode::unexpected_burst_count,
            bursts.size(),
        });
        return result;
    }

    return decode_program_blocks(bursts.size(), program_input, source_channel,
        [&](std::size_t index, std::size_t length, NativeMsaveDecodeResult& result) {
            return decode_block(samples, bursts[std::min(index, bursts.size() - 1U)], parsed.wav->sample_rate,
                index == 0U ? 40U : 20U, length, index, result);
        });
}

NativeMsaveDecodeResult decode_native_program_blocks(
    std::span<const std::vector<std::uint8_t>> blocks) {
    if (blocks.size() < 2U) {
        return {std::nullopt, {{NativeMsaveIssueCode::unexpected_burst_count, blocks.size()}}};
    }
    return decode_program_blocks(blocks.size(), true, 0U,
        [&](std::size_t index, std::size_t length, NativeMsaveDecodeResult& result)
            -> std::optional<std::vector<std::uint8_t>> {
            if (blocks[index].size() + 2U != length) {
                result.issues.push_back({NativeMsaveIssueCode::truncated_block, index});
                return std::nullopt;
            }
            auto bytes = blocks[index];
            std::uint16_t sum = 0U;
            for (auto value : bytes) sum = static_cast<std::uint16_t>(sum + value);
            bytes.push_back(static_cast<std::uint8_t>(sum >> 8U));
            bytes.push_back(static_cast<std::uint8_t>(sum));
            return bytes;
        });
}

NativeMsaveDecodeResult decode_native_msave_wav(
    std::span<const std::uint8_t> wav_bytes
) {
    return decode_msave_wav(wav_bytes, false);
}

NativeMsaveDecodeResult decode_native_program_wav(
    std::span<const std::uint8_t> wav_bytes
) {
    return decode_msave_wav(wav_bytes, true);
}

jr8app::Application native_program_application(const NativeMsaveFile& file) {
    jr8app::Application application;
    application.target_profile = "hd6301v1";
    application.kind = file.kind;
    application.name.assign(file.filename.begin(), file.filename.end());
    if (file.kind == jr8app::ProgramKind::machine_code) {
        application.entry_point = file.execution_address;
        application.segments = {{jr8app::SegmentKind::data, file.start_address,
            static_cast<std::uint32_t>(file.payload.size()), file.payload}};
    } else {
        application.basic_data = file.payload;
    }
    application.integrity_sha256 = jr8app::compute_integrity(application);
    return application;
}

std::string_view native_msave_issue_name(NativeMsaveIssueCode code) noexcept {
    switch (code) {
    case NativeMsaveIssueCode::invalid_basic_program:
        return "invalid-basic-program";
    case NativeMsaveIssueCode::unexpected_trailing_blocks:
        return "unexpected-trailing-blocks";
    case NativeMsaveIssueCode::invalid_wav:
        return "invalid-wav";
    case NativeMsaveIssueCode::unsupported_wav:
        return "unsupported-wav";
    case NativeMsaveIssueCode::no_signal:
        return "no-signal";
    case NativeMsaveIssueCode::unexpected_burst_count:
        return "unexpected-burst-count";
    case NativeMsaveIssueCode::synchronization_failed:
        return "synchronization-failed";
    case NativeMsaveIssueCode::truncated_block:
        return "truncated-block";
    case NativeMsaveIssueCode::framing_error:
        return "framing-error";
    case NativeMsaveIssueCode::checksum_mismatch:
        return "checksum-mismatch";
    case NativeMsaveIssueCode::unsupported_header:
        return "unsupported-header";
    case NativeMsaveIssueCode::invalid_length:
        return "invalid-length";
    case NativeMsaveIssueCode::invalid_program_range:
        return "invalid-program-range";
    case NativeMsaveIssueCode::ambiguous_header_byte_order:
        return "ambiguous-header-byte-order";
    }
    return "unknown";
}

}  // namespace jr800::formats
