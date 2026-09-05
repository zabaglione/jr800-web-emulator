// SPDX-License-Identifier: MIT

#include "jr800/core/rp5c01_register_file.hpp"

#include <chrono>

#include <array>
#include <cstddef>

namespace jr800::core {
namespace {

constexpr std::array<std::uint8_t, 0x0DU> time_register_masks{
    0x0FU,
    0x07U,
    0x0FU,
    0x07U,
    0x0FU,
    0x03U,
    0x07U,
    0x0FU,
    0x03U,
    0x0FU,
    0x01U,
    0x0FU,
    0x0FU,
};

constexpr std::array<std::uint8_t, 0x0DU> time_register_maximums{
    0x09U,
    0x05U,
    0x09U,
    0x05U,
    0x09U,
    0x03U,
    0x06U,
    0x09U,
    0x03U,
    0x09U,
    0x01U,
    0x09U,
    0x09U,
};

constexpr std::array<std::uint8_t, 0x0DU> alarm_register_masks{
    0x00U,
    0x00U,
    0x0FU,
    0x07U,
    0x0FU,
    0x03U,
    0x07U,
    0x0FU,
    0x03U,
    0x00U,
    0x01U,
    0x03U,
    0x00U,
};

constexpr std::array<std::uint8_t, 0x0DU> alarm_register_maximums{
    0x00U,
    0x00U,
    0x09U,
    0x05U,
    0x09U,
    0x03U,
    0x06U,
    0x09U,
    0x03U,
    0x00U,
    0x01U,
    0x03U,
    0x00U,
};

constexpr std::array<std::uint8_t, 7U> alarm_comparison_groups{
    0U, 0U,
    1U, 1U,
    2U,
    3U, 3U,
};

struct DecimalCounter {
    Rp5c01RegisterStatus status{Rp5c01RegisterStatus::unknown_state};
    std::uint8_t value{};
};

DecimalCounter decode_decimal_counter(
    const std::optional<std::uint8_t>& units,
    const std::optional<std::uint8_t>& tens,
    std::uint8_t minimum,
    std::uint8_t maximum
) noexcept {
    if (!units.has_value() || !tens.has_value()) {
        return {Rp5c01RegisterStatus::unknown_state, 0U};
    }
    const auto value = static_cast<std::uint8_t>(*tens * 10U + *units);
    if (*units > 9U || *tens > 9U || value < minimum || value > maximum) {
        return {Rp5c01RegisterStatus::unsupported_operation, 0U};
    }
    return {Rp5c01RegisterStatus::ok, value};
}

template<typename Bank>
void store_decimal_counter(
    Bank& bank,
    std::size_t units_address,
    std::size_t tens_address,
    std::uint8_t value
) noexcept {
    bank[units_address] = static_cast<std::uint8_t>(value % 10U);
    bank[tens_address] = static_cast<std::uint8_t>(value / 10U);
}

std::uint8_t days_in_month(
    std::uint8_t month,
    bool leap_year
) noexcept {
    constexpr std::array<std::uint8_t, 12U> days{
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U,
    };
    if (month == 2U && leap_year) {
        return 29U;
    }
    return days[static_cast<std::size_t>(month - 1U)];
}

std::optional<bool> three_state_and(
    std::optional<bool> left,
    std::optional<bool> right
) noexcept {
    if (left == false || right == false) {
        return false;
    }
    if (!left.has_value() || !right.has_value()) {
        return std::nullopt;
    }
    return true;
}

std::optional<bool> three_state_or(
    std::optional<bool> left,
    std::optional<bool> right
) noexcept {
    if (left == true || right == true) {
        return true;
    }
    if (left == false && right == false) {
        return false;
    }
    return std::nullopt;
}

}  // namespace

bool CalendarDateTime::valid() const noexcept {
    return year >= 2000U && year <= 2099U
        && hour < 24U && minute < 60U && second < 60U
        && std::chrono::year_month_day{
            std::chrono::year{year}, std::chrono::month{month},
            std::chrono::day{day},
        }.ok();
}

bool Rp5c01RegisterFile::set_datetime(CalendarDateTime value) noexcept {
    if (!value.valid()) return false;
    store_decimal_counter(time_registers_, 0U, 1U, value.second);
    store_decimal_counter(time_registers_, 2U, 3U, value.minute);
    store_decimal_counter(time_registers_, 4U, 5U, value.hour);
    store_decimal_counter(time_registers_, 7U, 8U, value.day);
    store_decimal_counter(time_registers_, 9U, 10U, value.month);
    store_decimal_counter(time_registers_, 11U, 12U,
        static_cast<std::uint8_t>(value.year - 2000U));
    const auto date = std::chrono::year{value.year}
        / std::chrono::month{value.month} / std::chrono::day{value.day};
    time_registers_[6U] = static_cast<std::uint8_t>(
        std::chrono::weekday{std::chrono::sys_days{date}}.c_encoding());
    alarm_registers_[10U] = 1U;  // 24-hour clock.
    alarm_registers_[11U] = static_cast<std::uint8_t>(value.year % 4U);
    mode_register_ = 8U;  // Time bank, timer enabled, alarm disabled.
    oscillator_divider_ticks_ = 0U;
    clock_hold_pending_ = false;
    clock_hold_read_guard_ticks_ = 0U;
    return true;
}

void Rp5c01RegisterFile::initialize_zero() noexcept {
    time_registers_.fill(0U);
    alarm_registers_.fill(0U);
    ram_block_10_.fill(0U);
    ram_block_11_.fill(0U);
    mode_register_ = 0U;
    clock_16hz_output_enabled_ = true;
    clock_1hz_output_enabled_ = true;
    clock_hold_pending_ = false;
    clock_hold_read_guard_ticks_ = 0U;
    oscillator_divider_ticks_ = 0U;
    alarm_comparison_enabled_.fill(false);
}

Rp5c01RegisterReadResult Rp5c01RegisterFile::read(
    std::uint8_t address
) const noexcept {
    if (address >= register_count) {
        return {Rp5c01RegisterStatus::invalid_address, std::nullopt};
    }
    if (address == test_register_address || address == reset_control_address) {
        return {Rp5c01RegisterStatus::ok, 0U};
    }
    if (address == mode_register_address) {
        if (!mode_register_.has_value()) {
            return {Rp5c01RegisterStatus::unknown_state, std::nullopt};
        }
        return {Rp5c01RegisterStatus::ok, *mode_register_};
    }
    if (!mode_register_.has_value()) {
        return {Rp5c01RegisterStatus::unknown_state, std::nullopt};
    }

    const auto mode = static_cast<std::uint8_t>(
        *mode_register_ & mode_mask
    );
    if (mode == 0U && address < bank_register_count
        && clock_hold_read_guard_ticks_ != 0U) {
        return {
            Rp5c01RegisterStatus::unsupported_operation,
            std::nullopt,
        };
    }
    const auto mask = writable_mask(mode, address);
    if (mask == 0U) {
        return {Rp5c01RegisterStatus::ok, 0U};
    }

    const auto& stored = bank(mode)[static_cast<std::size_t>(address)];
    if (!stored.has_value()) {
        return {Rp5c01RegisterStatus::unknown_state, std::nullopt};
    }
    return {
        Rp5c01RegisterStatus::ok,
        static_cast<std::uint8_t>(*stored & mask),
    };
}

Rp5c01RegisterWriteResult Rp5c01RegisterFile::write(
    std::uint8_t address,
    std::uint8_t value
) noexcept {
    if (address >= register_count) {
        return {Rp5c01RegisterStatus::invalid_address, 0U, false};
    }

    const auto nibble = static_cast<std::uint8_t>(value & data_mask);
    if (address == mode_register_address) {
        const auto previous_value_known = mode_register_.has_value();
        const auto previous_value = mode_register_.value_or(0U);
        constexpr std::uint8_t timer_enable_mask = 0x08U;
        const auto resumes_clock = previous_value_known
            && (previous_value & timer_enable_mask) == 0U
            && (nibble & timer_enable_mask) != 0U;
        if (resumes_clock && clock_hold_pending_ == true) {
            auto candidate = *this;
            candidate.mode_register_ = nibble;
            candidate.clock_hold_pending_ = false;
            const auto status = candidate.advance_one_second();
            if (status != Rp5c01RegisterStatus::ok) {
                return {status, previous_value, true};
            }
            candidate.clock_hold_read_guard_ticks_ =
                clock_hold_safe_read_ticks;
            *this = candidate;
            return {
                Rp5c01RegisterStatus::ok,
                previous_value,
                true,
            };
        }
        mode_register_ = nibble;
        return {
            Rp5c01RegisterStatus::ok,
            previous_value,
            previous_value_known,
        };
    }
    if (address == test_register_address) {
        if (nibble != 0U) {
            return {
                Rp5c01RegisterStatus::unsupported_operation,
                0U,
                true,
            };
        }
        return {Rp5c01RegisterStatus::ok, 0U, true};
    }
    if (address == reset_control_address) {
        if ((nibble & 0x01U) != 0U) {
            for (
                auto alarm_address = first_alarm_register_address;
                alarm_address <= last_alarm_register_address;
                ++alarm_address
            ) {
                alarm_registers_[
                    static_cast<std::size_t>(alarm_address)
                ] = 0U;
            }
            alarm_comparison_enabled_.fill(false);
        }

        if ((nibble & 0x02U) != 0U) {
            oscillator_divider_ticks_ = 0U;
        }
        clock_16hz_output_enabled_ = (nibble & 0x04U) == 0U;
        clock_1hz_output_enabled_ = (nibble & 0x08U) == 0U;
        return {Rp5c01RegisterStatus::ok, 0U, true};
    }
    if (!mode_register_.has_value()) {
        return {Rp5c01RegisterStatus::unknown_state, 0U, false};
    }

    const auto mode = static_cast<std::uint8_t>(
        *mode_register_ & mode_mask
    );
    const auto mask = writable_mask(mode, address);
    if (mask == 0U) {
        return {Rp5c01RegisterStatus::ok, 0U, true};
    }

    const auto stored_value = static_cast<std::uint8_t>(nibble & mask);
    if (stored_value > maximum_value(mode, address)) {
        return {
            Rp5c01RegisterStatus::unsupported_operation,
            0U,
            false,
        };
    }

    auto& stored = bank(mode)[static_cast<std::size_t>(address)];
    const auto previous_value_known = stored.has_value();
    const auto previous_value = stored.value_or(0U);
    stored = stored_value;
    if (mode == 1U && address >= first_alarm_register_address
        && address <= last_alarm_register_address) {
        const auto group = alarm_comparison_groups[
            static_cast<std::size_t>(address - first_alarm_register_address)
        ];
        alarm_comparison_enabled_[group] = true;
    }
    return {
        Rp5c01RegisterStatus::ok,
        static_cast<std::uint8_t>(previous_value & mask),
        previous_value_known,
    };
}

Rp5c01RegisterStatus Rp5c01RegisterFile::advance_one_second() noexcept {
    if (!mode_register_.has_value()) {
        return Rp5c01RegisterStatus::unknown_state;
    }
    constexpr std::uint8_t timer_enable_mask = 0x08U;
    if ((*mode_register_ & timer_enable_mask) == 0U) {
        clock_hold_pending_ = true;
        return Rp5c01RegisterStatus::ok;
    }

    return advance_running_one_second();
}

Rp5c01RegisterStatus
Rp5c01RegisterFile::advance_running_one_second() noexcept {
    auto next_time = time_registers_;
    auto next_alarm = alarm_registers_;

    const auto seconds = decode_decimal_counter(
        next_time[0U],
        next_time[1U],
        0U,
        59U
    );
    if (seconds.status != Rp5c01RegisterStatus::ok) {
        return seconds.status;
    }
    if (seconds.value < 59U) {
        store_decimal_counter(
            next_time,
            0U,
            1U,
            static_cast<std::uint8_t>(seconds.value + 1U)
        );
        time_registers_ = next_time;
        return Rp5c01RegisterStatus::ok;
    }

    const auto minutes = decode_decimal_counter(
        next_time[2U],
        next_time[3U],
        0U,
        59U
    );
    if (minutes.status != Rp5c01RegisterStatus::ok) {
        return minutes.status;
    }
    store_decimal_counter(next_time, 0U, 1U, 0U);
    if (minutes.value < 59U) {
        store_decimal_counter(
            next_time,
            2U,
            3U,
            static_cast<std::uint8_t>(minutes.value + 1U)
        );
        time_registers_ = next_time;
        return Rp5c01RegisterStatus::ok;
    }

    const auto hour_system = next_alarm[0x0AU];
    if (!hour_system.has_value()) {
        return Rp5c01RegisterStatus::unknown_state;
    }
    store_decimal_counter(next_time, 2U, 3U, 0U);

    bool advance_date = false;
    if ((*hour_system & 0x01U) != 0U) {
        const auto hours = decode_decimal_counter(
            next_time[4U],
            next_time[5U],
            0U,
            23U
        );
        if (hours.status != Rp5c01RegisterStatus::ok) {
            return hours.status;
        }
        if (hours.value < 23U) {
            store_decimal_counter(
                next_time,
                4U,
                5U,
                static_cast<std::uint8_t>(hours.value + 1U)
            );
        } else {
            store_decimal_counter(next_time, 4U, 5U, 0U);
            advance_date = true;
        }
    } else {
        if (!next_time[4U].has_value() || !next_time[5U].has_value()) {
            return Rp5c01RegisterStatus::unknown_state;
        }
        const auto units = *next_time[4U];
        const auto tens_and_period = *next_time[5U];
        const auto hours = static_cast<std::uint8_t>(
            (tens_and_period & 0x01U) * 10U + units
        );
        if (units > 9U || hours < 1U || hours > 12U) {
            return Rp5c01RegisterStatus::unsupported_operation;
        }

        auto afternoon = (tens_and_period & 0x02U) != 0U;
        std::uint8_t next_hours{};
        if (hours == 11U) {
            next_hours = 12U;
            advance_date = afternoon;
            afternoon = !afternoon;
        } else if (hours == 12U) {
            next_hours = 1U;
        } else {
            next_hours = static_cast<std::uint8_t>(hours + 1U);
        }
        next_time[4U] = static_cast<std::uint8_t>(next_hours % 10U);
        next_time[5U] = static_cast<std::uint8_t>(
            (afternoon ? 0x02U : 0U)
            | (next_hours >= 10U ? 0x01U : 0U)
        );
    }

    if (!advance_date) {
        time_registers_ = next_time;
        return Rp5c01RegisterStatus::ok;
    }

    if (next_time[6U].has_value()) {
        if (*next_time[6U] > 6U) {
            return Rp5c01RegisterStatus::unsupported_operation;
        }
        next_time[6U] = static_cast<std::uint8_t>(
            (*next_time[6U] + 1U) % 7U
        );
    }

    const auto date = decode_decimal_counter(
        next_time[7U],
        next_time[8U],
        1U,
        31U
    );
    if (date.status != Rp5c01RegisterStatus::ok) {
        return date.status;
    }
    if (date.value < 28U) {
        store_decimal_counter(
            next_time,
            7U,
            8U,
            static_cast<std::uint8_t>(date.value + 1U)
        );
        time_registers_ = next_time;
        return Rp5c01RegisterStatus::ok;
    }

    const auto month = decode_decimal_counter(
        next_time[9U],
        next_time[0x0AU],
        1U,
        12U
    );
    if (month.status != Rp5c01RegisterStatus::ok) {
        return month.status;
    }

    bool leap_year = false;
    if (month.value == 2U) {
        if (!next_alarm[0x0BU].has_value()) {
            return Rp5c01RegisterStatus::unknown_state;
        }
        if (*next_alarm[0x0BU] > 3U) {
            return Rp5c01RegisterStatus::unsupported_operation;
        }
        leap_year = *next_alarm[0x0BU] == 0U;
    }

    const auto month_days = days_in_month(month.value, leap_year);
    if (date.value > month_days) {
        return Rp5c01RegisterStatus::unsupported_operation;
    }

    if (date.value < month_days) {
        store_decimal_counter(
            next_time,
            7U,
            8U,
            static_cast<std::uint8_t>(date.value + 1U)
        );
    } else {
        store_decimal_counter(next_time, 7U, 8U, 1U);
        if (month.value < 12U) {
            store_decimal_counter(
                next_time,
                9U,
                0x0AU,
                static_cast<std::uint8_t>(month.value + 1U)
            );
        } else {
            store_decimal_counter(next_time, 9U, 0x0AU, 1U);
            if (next_time[0x0BU].has_value()
                && next_time[0x0CU].has_value()) {
                const auto year = decode_decimal_counter(
                    next_time[0x0BU],
                    next_time[0x0CU],
                    0U,
                    99U
                );
                if (year.status != Rp5c01RegisterStatus::ok) {
                    return year.status;
                }
                store_decimal_counter(
                    next_time,
                    0x0BU,
                    0x0CU,
                    static_cast<std::uint8_t>((year.value + 1U) % 100U)
                );
            } else {
                next_time[0x0BU] = std::nullopt;
                next_time[0x0CU] = std::nullopt;
            }
            if (next_alarm[0x0BU].has_value()) {
                const auto leap_year_counter = *next_alarm[0x0BU];
                if (leap_year_counter > 3U) {
                    return Rp5c01RegisterStatus::unsupported_operation;
                }
                next_alarm[0x0BU] = static_cast<std::uint8_t>(
                    (leap_year_counter + 1U) % 4U
                );
            }
        }
    }

    time_registers_ = next_time;
    alarm_registers_ = next_alarm;
    return Rp5c01RegisterStatus::ok;
}

Rp5c01RegisterStatus Rp5c01RegisterFile::adjust_seconds() noexcept {
    if (!clock_hold_pending_.has_value()) {
        return Rp5c01RegisterStatus::unknown_state;
    }
    if (*clock_hold_pending_ || clock_hold_read_guard_ticks_ != 0U) {
        return Rp5c01RegisterStatus::unsupported_operation;
    }

    const auto seconds = decode_decimal_counter(
        time_registers_[0U],
        time_registers_[1U],
        0U,
        59U
    );
    if (seconds.status != Rp5c01RegisterStatus::ok) {
        return seconds.status;
    }

    if (seconds.value < 30U) {
        auto next_time = time_registers_;
        store_decimal_counter(next_time, 0U, 1U, 0U);
        time_registers_ = next_time;
        return Rp5c01RegisterStatus::ok;
    }

    auto candidate = *this;
    store_decimal_counter(candidate.time_registers_, 0U, 1U, 59U);
    const auto status = candidate.advance_running_one_second();
    if (status != Rp5c01RegisterStatus::ok) {
        return status;
    }
    *this = candidate;
    return Rp5c01RegisterStatus::ok;
}

Rp5c01RegisterStatus Rp5c01RegisterFile::advance_oscillator_ticks(
    std::uint32_t ticks
) noexcept {
    if (ticks == 0U) {
        return Rp5c01RegisterStatus::ok;
    }
    if (!oscillator_divider_ticks_.has_value()) {
        return Rp5c01RegisterStatus::unknown_state;
    }

    constexpr std::uint32_t ticks_per_second = 32'768U;
    const auto total_ticks = static_cast<std::uint64_t>(
        *oscillator_divider_ticks_
    ) + ticks;
    const auto elapsed_seconds = total_ticks / ticks_per_second;

    auto candidate = *this;
    candidate.clock_hold_read_guard_ticks_ = static_cast<std::uint8_t>(
        ticks >= candidate.clock_hold_read_guard_ticks_
            ? 0U
            : candidate.clock_hold_read_guard_ticks_ - ticks
    );
    candidate.oscillator_divider_ticks_ = static_cast<std::uint32_t>(
        total_ticks % ticks_per_second
    );
    for (std::uint64_t second = 0U; second < elapsed_seconds; ++second) {
        const auto status = candidate.advance_one_second();
        if (status != Rp5c01RegisterStatus::ok) {
            return status;
        }
    }
    *this = candidate;
    return Rp5c01RegisterStatus::ok;
}

std::optional<std::uint8_t> Rp5c01RegisterFile::mode_register() const noexcept {
    return mode_register_;
}

std::optional<bool>
Rp5c01RegisterFile::clock_16hz_output_enabled() const noexcept {
    return clock_16hz_output_enabled_;
}

std::optional<bool>
Rp5c01RegisterFile::clock_1hz_output_enabled() const noexcept {
    return clock_1hz_output_enabled_;
}

std::optional<bool>
Rp5c01RegisterFile::divider_16hz_signal() const noexcept {
    if (!oscillator_divider_ticks_.has_value()) {
        return std::nullopt;
    }
    constexpr std::uint32_t divider_bit_mask = 0x0400U;
    return (*oscillator_divider_ticks_ & divider_bit_mask) != 0U;
}

std::optional<bool>
Rp5c01RegisterFile::divider_1hz_signal() const noexcept {
    if (!oscillator_divider_ticks_.has_value()) {
        return std::nullopt;
    }
    constexpr std::uint32_t divider_bit_mask = 0x4000U;
    return (*oscillator_divider_ticks_ & divider_bit_mask) != 0U;
}

std::optional<bool>
Rp5c01RegisterFile::clock_16hz_gate_output() const noexcept {
    return three_state_and(
        divider_16hz_signal(),
        clock_16hz_output_enabled_
    );
}

std::optional<bool>
Rp5c01RegisterFile::clock_1hz_gate_output() const noexcept {
    return three_state_and(
        divider_1hz_signal(),
        clock_1hz_output_enabled_
    );
}

std::optional<bool> Rp5c01RegisterFile::clock_hold_pending() const noexcept {
    return clock_hold_pending_;
}

std::optional<bool>
Rp5c01RegisterFile::alarm_comparator_output() const noexcept {
    bool has_unknown_input = false;
    for (std::size_t group = 0U;
         group < alarm_comparison_enabled_.size();
         ++group) {
        const auto enabled = alarm_comparison_enabled_[group];
        if (!enabled.has_value()) {
            has_unknown_input = true;
            continue;
        }
        if (!*enabled) {
            continue;
        }

        for (std::size_t offset = 0U;
             offset < alarm_comparison_groups.size();
             ++offset) {
            if (alarm_comparison_groups[offset] != group) {
                continue;
            }
            const auto address = offset + first_alarm_register_address;
            const auto time = time_registers_[address];
            const auto alarm = alarm_registers_[address];
            if (!time.has_value() || !alarm.has_value()) {
                has_unknown_input = true;
                continue;
            }
            if (*time != *alarm) {
                return false;
            }
        }
    }
    if (has_unknown_input) {
        return std::nullopt;
    }
    return true;
}

std::optional<bool>
Rp5c01RegisterFile::alarm_comparator_gate_output() const noexcept {
    constexpr std::uint8_t alarm_enable_mask = 0x04U;
    std::optional<bool> alarm_enabled;
    if (mode_register_.has_value()) {
        alarm_enabled = (*mode_register_ & alarm_enable_mask) != 0U;
    }
    return three_state_and(alarm_comparator_output(), alarm_enabled);
}

std::optional<bool>
Rp5c01RegisterFile::alarm_terminal_pull_low() const noexcept {
    return three_state_or(
        alarm_comparator_gate_output(),
        three_state_or(
            clock_16hz_gate_output(),
            clock_1hz_gate_output()
        )
    );
}

std::uint8_t Rp5c01RegisterFile::writable_mask(
    std::uint8_t mode,
    std::uint8_t address
) noexcept {
    const auto index = static_cast<std::size_t>(address);
    if (mode == 0U) {
        return time_register_masks[index];
    }
    if (mode == 1U) {
        return alarm_register_masks[index];
    }
    return data_mask;
}

std::uint8_t Rp5c01RegisterFile::maximum_value(
    std::uint8_t mode,
    std::uint8_t address
) noexcept {
    const auto index = static_cast<std::size_t>(address);
    if (mode == 0U) {
        return time_register_maximums[index];
    }
    if (mode == 1U) {
        return alarm_register_maximums[index];
    }
    return data_mask;
}

const Rp5c01RegisterFile::RegisterBank& Rp5c01RegisterFile::bank(
    std::uint8_t mode
) const noexcept {
    if (mode == 0U) {
        return time_registers_;
    }
    if (mode == 1U) {
        return alarm_registers_;
    }
    if (mode == 2U) {
        return ram_block_10_;
    }
    return ram_block_11_;
}

Rp5c01RegisterFile::RegisterBank& Rp5c01RegisterFile::bank(
    std::uint8_t mode
) noexcept {
    if (mode == 0U) {
        return time_registers_;
    }
    if (mode == 1U) {
        return alarm_registers_;
    }
    if (mode == 2U) {
        return ram_block_10_;
    }
    return ram_block_11_;
}

}  // namespace jr800::core
