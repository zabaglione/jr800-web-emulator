// SPDX-License-Identifier: MIT

#include "jr800/core/jr800_bus.hpp"

#include <cstdint>

namespace jr800::core {
Jr800Bus::Jr800Bus(const Jr800Bus& source) noexcept : Bus() {
    copy_state_from(source);
}

void Jr800Bus::copy_state_from(const Jr800Bus& source) noexcept {
    ports_ = source.ports_;
    ram_control_ = source.ram_control_;
    sci_ = source.sci_;
    timer_ = source.timer_;
    keyboard_ = source.keyboard_;
    calendar_ = source.calendar_;
    lcd_ = source.lcd_;
    memory_ = source.memory_;
    experimental_lcd_configuration_ = source.experimental_lcd_configuration_;
    experimental_calendar_configuration_ = source.experimental_calendar_configuration_;
    ignore_unsupported_io_ = source.ignore_unsupported_io_;
    calendar_cpu_cycle_remainder_ = source.calendar_cpu_cycle_remainder_;
    lcd_substituted_data_read_count_ = source.lcd_substituted_data_read_count_;
    ignored_io_access_count_ = source.ignored_io_access_count_;
    copy_execution_context(source);
}

namespace {

BusFault bus_fault(Jr800MemoryStatus status) noexcept {
    switch (status) {
    case Jr800MemoryStatus::ok:
        return BusFault::none;
    case Jr800MemoryStatus::invalid_rom_size:
    case Jr800MemoryStatus::rom_not_loaded:
        return BusFault::backing_store_unavailable;
    case Jr800MemoryStatus::uninitialized_ram:
        return BusFault::uninitialized_read;
    case Jr800MemoryStatus::unsupported_region:
        return BusFault::unsupported_access;
    case Jr800MemoryStatus::read_only:
        return BusFault::read_only_write;
    }
    return BusFault::unsupported_access;
}

BusFault lcd_fault(Jr800LcdAccessStatus status) noexcept {
    switch (status) {
    case Jr800LcdAccessStatus::ok:
        return BusFault::none;
    case Jr800LcdAccessStatus::unknown_state:
        return BusFault::uninitialized_read;
    case Jr800LcdAccessStatus::not_handled:
    case Jr800LcdAccessStatus::unsupported_select:
    case Jr800LcdAccessStatus::busy:
    case Jr800LcdAccessStatus::reset_asserted:
    case Jr800LcdAccessStatus::unsupported_instruction:
    case Jr800LcdAccessStatus::no_pending_instruction:
        return BusFault::unsupported_access;
    }
    return BusFault::unsupported_access;
}

BusFault calendar_fault(Rp5c01RegisterStatus status) noexcept {
    switch (status) {
    case Rp5c01RegisterStatus::ok:
        return BusFault::none;
    case Rp5c01RegisterStatus::unknown_state:
        return BusFault::uninitialized_read;
    case Rp5c01RegisterStatus::unsupported_operation:
    case Rp5c01RegisterStatus::invalid_address:
        return BusFault::unsupported_access;
    }
    return BusFault::unsupported_access;
}

BusFault calendar_cycle_fault(Rp5c01RegisterStatus status) noexcept {
    switch (status) {
    case Rp5c01RegisterStatus::ok:
        return BusFault::none;
    case Rp5c01RegisterStatus::unknown_state:
        return BusFault::device_state_unknown;
    case Rp5c01RegisterStatus::unsupported_operation:
    case Rp5c01RegisterStatus::invalid_address:
        return BusFault::device_state_unsupported;
    }
    return BusFault::device_state_unsupported;
}

Jr800CalendarOperationStatus calendar_operation_status(
    Rp5c01RegisterStatus status
) noexcept {
    switch (status) {
    case Rp5c01RegisterStatus::ok:
        return Jr800CalendarOperationStatus::ok;
    case Rp5c01RegisterStatus::unknown_state:
        return Jr800CalendarOperationStatus::unknown_state;
    case Rp5c01RegisterStatus::unsupported_operation:
    case Rp5c01RegisterStatus::invalid_address:
        return Jr800CalendarOperationStatus::unsupported_state;
    }
    return Jr800CalendarOperationStatus::unsupported_state;
}

}  // namespace

Jr800Bus::Jr800Bus(
    Jr800ExperimentalMachineConfiguration configuration
) noexcept
    : experimental_lcd_configuration_(configuration.lcd),
      experimental_calendar_configuration_(configuration.calendar),
      ignore_unsupported_io_(configuration.ignore_unsupported_io) {
    if (configuration.internal_ram.has_value()) {
        static_cast<void>(memory_.initialize_ram(
            Jr800MemoryRegion::cpu_internal_ram,
            configuration.internal_ram->initial_value
        ));
    }
    if (configuration.memory.has_value()) {
        static_cast<void>(memory_.initialize_ram(
            Jr800MemoryRegion::standard_ram,
            configuration.memory->standard_ram_initial_value
        ));
        if (configuration.memory->expansion_ram_initial_value.has_value()) {
            static_cast<void>(memory_.initialize_ram(
                Jr800MemoryRegion::expansion_ram,
                *configuration.memory->expansion_ram_initial_value
            ));
        }
    }
    if (experimental_calendar_configuration_.has_value()) {
        calendar_.initialize_zero();
    }
}

Jr800CalendarOperationStatus Jr800Bus::set_calendar_datetime(
    CalendarDateTime value
) noexcept {
    if (!experimental_calendar_configuration_.has_value()) {
        return Jr800CalendarOperationStatus::calendar_disconnected;
    }
    if (!calendar_.set_datetime(value)) {
        return Jr800CalendarOperationStatus::unsupported_state;
    }
    calendar_cpu_cycle_remainder_ = 0U;
    return Jr800CalendarOperationStatus::ok;
}

Jr800MemoryStatus Jr800Bus::load_logical_rom(
    std::span<const std::uint8_t> bytes
) noexcept {
    return memory_.load_logical_rom(bytes);
}

bool Jr800Bus::can_host_load_ram(
    std::uint16_t address,
    std::size_t size
) const noexcept {
    return memory_.can_host_load_ram(address, size);
}

Jr800MemoryStatus Jr800Bus::host_load_ram(
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
) noexcept {
    return memory_.host_load_ram(address, bytes);
}

Jr800MemoryStatus Jr800Bus::host_fill_ram(
    std::uint16_t address,
    std::size_t size,
    std::uint8_t value
) noexcept {
    return memory_.host_fill_ram(address, size, value);
}

void Jr800Bus::reset_cpu_devices() noexcept {
    ignored_io_access_count_ = 0U;
    ports_.reset();
    ram_control_.reset();
    sci_.reset();
    timer_.reset();
    keyboard_.clear_activity();
    if (experimental_lcd_configuration_.has_value()) {
        lcd_substituted_data_read_count_ = 0U;
        for (std::uint8_t controller = 0U;
             controller < Jr800Lcd::controller_count;
             ++controller) {
            static_cast<void>(
                lcd_.set_controller_reset_line(controller, true)
            );
            static_cast<void>(
                lcd_.set_controller_reset_line(controller, false)
            );
        }
    }
}

void Jr800Bus::set_port1_pin_state(
    std::uint8_t value,
    std::uint8_t known_mask
) noexcept {
    ports_.set_port1_pin_state(value, known_mask);
}

void Jr800Bus::set_port2_pin_state(
    std::uint8_t value,
    std::uint8_t known_mask
) noexcept {
    ports_.set_port2_pin_state(value, known_mask);
    sci_.set_receive_pin_state(
        (value & 0x08U) != 0U,
        (known_mask & 0x08U) != 0U
    );
    timer_.set_input_capture_pin_state(
        (value & 0x01U) != 0U,
        (known_mask & 0x01U) != 0U
    );
}

void Jr800Bus::set_ram_standby_power_valid(
    bool value,
    bool known
) noexcept {
    ram_control_.set_standby_power_valid(value, known);
}

bool Jr800Bus::set_keyboard_bus_response(
    std::uint16_t address,
    std::uint8_t value,
    bool known
) noexcept {
    return keyboard_.set_bus_response(address, value, known);
}

bool Jr800Bus::set_keyboard_key_state(
    Jr800Key key,
    bool pressed
) noexcept {
    return keyboard_.set_key_state(key, pressed);
}

void Jr800Bus::clear_keyboard_activity() noexcept {
    keyboard_.clear_activity();
}

Jr800KeyboardActivity Jr800Bus::keyboard_activity() const noexcept {
    return keyboard_.activity();
}

Jr800CalendarOperationStatus Jr800Bus::advance_calendar_oscillator_ticks(
    std::uint32_t ticks
) noexcept {
    if (!experimental_calendar_configuration_.has_value()) {
        return Jr800CalendarOperationStatus::calendar_disconnected;
    }
    return calendar_operation_status(
        calendar_.advance_oscillator_ticks(ticks)
    );
}

Jr800CalendarOperationStatus Jr800Bus::adjust_calendar_seconds() noexcept {
    if (!experimental_calendar_configuration_.has_value()) {
        return Jr800CalendarOperationStatus::calendar_disconnected;
    }
    return calendar_operation_status(calendar_.adjust_seconds());
}

Jr800CalendarAlarmTerminalState
Jr800Bus::calendar_alarm_terminal_state() const noexcept {
    if (!experimental_calendar_configuration_.has_value()) {
        return {};
    }
    return {true, calendar_.alarm_terminal_pull_low()};
}

Hd6301v1Port2TimerOutputState
Jr800Bus::port2_timer_output_state() const noexcept {
    return ports_.port2_timer_output_state(
        timer_.output_compare_level()
    );
}

BusFault Jr800Bus::advance_cycles(std::uint32_t cycles) noexcept {
    if (experimental_calendar_configuration_.has_value()
        && experimental_calendar_configuration_->cpu_cycle_ratio.has_value()) {
        using Ratio = Jr800ExperimentalCalendarCpuCycleRatio;
        if (*experimental_calendar_configuration_->cpu_cycle_ratio
            != Ratio::e030_nominal_1_2288_mhz) {
            return BusFault::device_state_unsupported;
        }

        // E-030 establishes a nominal 1.2288 MHz CPU E clock. The RP5C01
        // oscillator is 32.768 kHz, so the exact nominal ratio is 2 / 75.
        constexpr std::uint64_t oscillator_tick_numerator = 2U;
        constexpr std::uint64_t cpu_cycle_denominator = 75U;
        const auto scaled_cycles = static_cast<std::uint64_t>(
            calendar_cpu_cycle_remainder_
        ) + static_cast<std::uint64_t>(cycles) * oscillator_tick_numerator;
        const auto oscillator_ticks = static_cast<std::uint32_t>(
            scaled_cycles / cpu_cycle_denominator
        );
        const auto next_remainder = static_cast<std::uint8_t>(
            scaled_cycles % cpu_cycle_denominator
        );
        if (oscillator_ticks != 0U) {
            const auto calendar_status =
                calendar_.advance_oscillator_ticks(oscillator_ticks);
            const auto fault = calendar_cycle_fault(calendar_status);
            if (fault != BusFault::none) {
                return fault;
            }
        }
        calendar_cpu_cycle_remainder_ = next_remainder;
    }
    sci_.advance_cycles(cycles);
    timer_.advance_cycles(cycles);
    return BusFault::none;
}

InterruptRequest Jr800Bus::maskable_interrupt_request() const noexcept {
    const auto timer_request = timer_.interrupt_request();
    if (!timer_request.known) {
        return {InterruptSource::none, false};
    }
    if (timer_request.source.has_value()) {
        switch (*timer_request.source) {
        case Hd6301v1TimerInterruptSource::input_capture:
            return {InterruptSource::timer_input_capture, true};
        case Hd6301v1TimerInterruptSource::output_compare:
            return {InterruptSource::timer_output_compare, true};
        case Hd6301v1TimerInterruptSource::overflow:
            return {InterruptSource::timer_overflow, true};
        }
    }

    const auto sci_request = sci_.interrupt_request();
    if (!sci_request.known) {
        return {InterruptSource::none, false};
    }
    if (sci_request.asserted) {
        return {InterruptSource::serial, true};
    }
    return {};
}

BusReadResult Jr800Bus::read8(
    std::uint16_t address,
    AccessKind kind
) noexcept {
    if (experimental_calendar_configuration_.has_value()
        && jr800_memory_region(address)
            == Jr800MemoryRegion::calendar_clock) {
        return read_experimental_calendar(address, kind);
    }

    if (experimental_lcd_configuration_.has_value()
        && decode_jr800_lcd_address(address).handled) {
        return read_experimental_lcd(address, kind);
    }

    const auto port_read = ports_.read8(address);
    if (port_read.handled) {
        if (!port_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        const auto value = *port_read.value;
        notify_read(address, value, kind);
        return {BusFault::none, value};
    }

    const auto ram_control_read = ram_control_.read8(address);
    if (ram_control_read.handled) {
        if (!ram_control_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        const auto value = *ram_control_read.value;
        notify_read(address, value, kind);
        return {BusFault::none, value};
    }

    const auto timer_read = timer_.read8(address);
    if (timer_read.handled) {
        if (!timer_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        const auto value = *timer_read.value;
        notify_read(address, value, kind);
        return {BusFault::none, value};
    }

    const auto sci_read = sci_.read8(address);
    if (sci_read.handled) {
        if (!sci_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        const auto value = *sci_read.value;
        notify_read(address, value, kind);
        return {BusFault::none, value};
    }

    const auto keyboard_read = keyboard_.read8(address);
    if (keyboard_read.handled) {
        if (!keyboard_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        const auto value = *keyboard_read.value;
        notify_read(address, value, kind);
        return {BusFault::none, value};
    }

    if (jr800_memory_region(address) == Jr800MemoryRegion::cpu_internal_ram
        && !ram_control_.ram_enabled()) {
        return {BusFault::unsupported_access, std::nullopt};
    }

    const auto read = memory_.read8(address);
    if (!read.succeeded()) {
        if (read.status == Jr800MemoryStatus::unsupported_region
            && kind == AccessKind::data_read && can_ignore_io(address)) {
            ++ignored_io_access_count_;
            notify_read(address, 0xFFU, kind);
            return {BusFault::none, 0xFFU};
        }
        return {bus_fault(read.status), std::nullopt};
    }
    const auto value = *read.value;
    notify_read(address, value, kind);
    return {BusFault::none, value};
}

BusDiscardedReadResult Jr800Bus::read8_discard(
    std::uint16_t address
) noexcept {
    if (experimental_calendar_configuration_.has_value()
        && jr800_memory_region(address)
            == Jr800MemoryRegion::calendar_clock) {
        const auto read = read_experimental_calendar(
            address,
            AccessKind::data_read
        );
        return {read.fault};
    }

    if (experimental_lcd_configuration_.has_value()
        && decode_jr800_lcd_address(address).handled) {
        const auto read = read_experimental_lcd(
            address,
            AccessKind::data_read
        );
        return {read.fault};
    }

    const auto port_read = ports_.read8(address);
    if (port_read.handled) {
        if (!port_read.value.has_value()) {
            return {BusFault::uninitialized_read};
        }
        notify_read(address, *port_read.value, AccessKind::data_read);
        return {BusFault::none};
    }

    const auto ram_control_read = ram_control_.read8(address);
    if (ram_control_read.handled) {
        if (!ram_control_read.value.has_value()) {
            return {BusFault::uninitialized_read};
        }
        notify_read(address, *ram_control_read.value, AccessKind::data_read);
        return {BusFault::none};
    }

    const auto timer_read = timer_.read8(address);
    if (timer_read.handled) {
        if (!timer_read.value.has_value()) {
            return {BusFault::uninitialized_read};
        }
        notify_read(address, *timer_read.value, AccessKind::data_read);
        return {BusFault::none};
    }

    const auto sci_read = sci_.read8(address);
    if (sci_read.handled) {
        if (!sci_read.value.has_value()) {
            return {BusFault::uninitialized_read};
        }
        notify_read(address, *sci_read.value, AccessKind::data_read);
        return {BusFault::none};
    }

    const auto keyboard_read = keyboard_.read8(address);
    if (keyboard_read.handled) {
        if (!keyboard_read.value.has_value()) {
            return {BusFault::uninitialized_read};
        }
        notify_read(address, *keyboard_read.value, AccessKind::data_read);
        return {BusFault::none};
    }

    if (jr800_memory_region(address) == Jr800MemoryRegion::cpu_internal_ram
        && !ram_control_.ram_enabled()) {
        return {BusFault::unsupported_access};
    }

    const auto read = memory_.read8(address);
    if (read.status == Jr800MemoryStatus::uninitialized_ram) {
        notify_read(address, std::nullopt, AccessKind::data_read);
        return {BusFault::none};
    }
    if (!read.succeeded()) {
        if (read.status == Jr800MemoryStatus::unsupported_region
            && can_ignore_io(address)) {
            ++ignored_io_access_count_;
            notify_read(address, 0xFFU, AccessKind::data_read);
            return {BusFault::none};
        }
        return {bus_fault(read.status)};
    }
    notify_read(address, *read.value, AccessKind::data_read);
    return {BusFault::none};
}

BusReadResult Jr800Bus::inspect8(std::uint16_t address) const noexcept {
    if (experimental_calendar_configuration_.has_value()
        && jr800_memory_region(address)
            == Jr800MemoryRegion::calendar_clock) {
        const auto local_address = experimental_calendar_address(address);
        const auto upper_bits = experimental_calendar_upper_bits();
        if (!local_address.has_value() || !upper_bits.has_value()) {
            return {BusFault::unsupported_access, std::nullopt};
        }
        const auto read = calendar_.read(*local_address);
        if (!read.succeeded()) {
            return {calendar_fault(read.status), std::nullopt};
        }
        return {
            BusFault::none,
            static_cast<std::uint8_t>(
                *upper_bits | *read.value
            ),
        };
    }

    if (experimental_lcd_configuration_.has_value()) {
        const auto decoded = decode_jr800_lcd_address(address);
        if (decoded.handled) {
            if (!decoded.selection.has_value()
                || decoded.selection->target
                    != Jr800LcdRegister::control_status) {
                return {BusFault::unsupported_access, std::nullopt};
            }
            const auto state = lcd_.inspect_controller(
                decoded.selection->controller_index
            );
            if (!state.has_value() || !state->status.fully_known()) {
                return {BusFault::uninitialized_read, std::nullopt};
            }
            return {BusFault::none, state->status.value};
        }
    }

    const auto port_read = ports_.read8(address);
    if (port_read.handled) {
        if (!port_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        return {BusFault::none, *port_read.value};
    }

    const auto ram_control_read = ram_control_.read8(address);
    if (ram_control_read.handled) {
        if (!ram_control_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        return {BusFault::none, *ram_control_read.value};
    }

    const auto timer_read = timer_.inspect8(address);
    if (timer_read.handled) {
        if (!timer_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        return {BusFault::none, *timer_read.value};
    }

    const auto sci_read = sci_.read8(address);
    if (sci_read.handled) {
        if (!sci_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        return {BusFault::none, *sci_read.value};
    }

    const auto keyboard_read = keyboard_.inspect8(address);
    if (keyboard_read.handled) {
        if (!keyboard_read.value.has_value()) {
            return {BusFault::uninitialized_read, std::nullopt};
        }
        return {BusFault::none, *keyboard_read.value};
    }

    if (jr800_memory_region(address) == Jr800MemoryRegion::cpu_internal_ram
        && !ram_control_.ram_enabled()) {
        return {BusFault::unsupported_access, std::nullopt};
    }

    const auto read = memory_.read8(address);
    if (!read.succeeded()) {
        if (read.status == Jr800MemoryStatus::unsupported_region
            && can_ignore_io(address)) {
            return {BusFault::none, 0xFFU};
        }
        return {bus_fault(read.status), std::nullopt};
    }
    return {BusFault::none, *read.value};
}

BusWriteResult Jr800Bus::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    if (experimental_calendar_configuration_.has_value()
        && jr800_memory_region(address)
            == Jr800MemoryRegion::calendar_clock) {
        return write_experimental_calendar(address, value);
    }

    if (experimental_lcd_configuration_.has_value()
        && decode_jr800_lcd_address(address).handled) {
        return write_experimental_lcd(address, value);
    }

    const auto port_write = ports_.write8(address, value);
    if (port_write.handled) {
        if (address == 0x0001U) {
            timer_.set_input_capture_enabled(
                (ports_.port2_data_direction() & 0x01U) == 0U
            );
        }
        notify_write(
            address,
            value,
            port_write.previous_value_known
                ? std::optional<std::uint8_t>{port_write.previous_value}
                : std::nullopt
        );
        return {
            BusFault::none,
            port_write.previous_value,
            port_write.previous_value_known,
        };
    }

    const auto ram_control_write = ram_control_.write8(address, value);
    if (ram_control_write.handled) {
        notify_write(
            address,
            value,
            ram_control_write.previous_value_known
                ? std::optional<std::uint8_t>{
                    ram_control_write.previous_value
                }
                : std::nullopt
        );
        return {
            BusFault::none,
            ram_control_write.previous_value,
            ram_control_write.previous_value_known,
        };
    }

    const auto timer_write = timer_.write8(address, value);
    if (timer_write.handled) {
        notify_write(
            address,
            value,
            timer_write.previous_value_known
                ? std::optional<std::uint8_t>{timer_write.previous_value}
                : std::nullopt
        );
        return {
            BusFault::none,
            timer_write.previous_value,
            timer_write.previous_value_known,
        };
    }

    const auto sci_write = sci_.write8(address, value);
    if (sci_write.handled) {
        notify_write(
            address,
            value,
            sci_write.previous_value_known
                ? std::optional<std::uint8_t>{sci_write.previous_value}
                : std::nullopt
        );
        return {
            BusFault::none,
            sci_write.previous_value,
            sci_write.previous_value_known,
        };
    }

    if (jr800_memory_region(address) == Jr800MemoryRegion::cpu_internal_ram
        && !ram_control_.ram_enabled()) {
        return {BusFault::unsupported_access, 0U, false};
    }

    const auto previous = memory_.read8(address);
    const auto write = memory_.write8(address, value);
    if (!write.succeeded()) {
        if (write.status == Jr800MemoryStatus::unsupported_region
            && can_ignore_io(address)) {
            ++ignored_io_access_count_;
            notify_write(address, value, std::nullopt);
            return {BusFault::none, 0U, false};
        }
        return {bus_fault(write.status), 0U, false};
    }

    const auto previous_known = previous.succeeded();
    const auto previous_value = static_cast<std::uint8_t>(
        previous_known ? *previous.value : 0U
    );
    notify_write(
        address,
        value,
        previous_known
            ? std::optional<std::uint8_t>{previous_value}
            : std::nullopt
    );
    return {BusFault::none, previous_value, previous_known};
}

bool Jr800Bus::rom_loaded() const noexcept {
    return memory_.rom_loaded();
}

bool Jr800Bus::can_ignore_io(std::uint16_t address) const noexcept {
    if (!ignore_unsupported_io_) {
        return false;
    }
    // E-418 is an explicit host policy, not an open-bus hardware model.
    switch (jr800_memory_region(address)) {
    case Jr800MemoryRegion::cpu_internal_registers:
    case Jr800MemoryRegion::reserved:
    case Jr800MemoryRegion::calendar_clock:
    case Jr800MemoryRegion::lcd:
    case Jr800MemoryRegion::keyboard:
        return true;
    default:
        return false;
    }
}

std::optional<std::uint64_t>
Jr800Bus::ignored_io_access_count() const noexcept {
    return ignore_unsupported_io_
        ? std::optional<std::uint64_t>{ignored_io_access_count_}
        : std::nullopt;
}

BusReadResult Jr800Bus::read_experimental_calendar(
    std::uint16_t address,
    AccessKind kind
) noexcept {
    const auto local_address = experimental_calendar_address(address);
    const auto upper_bits = experimental_calendar_upper_bits();
    if (!local_address.has_value() || !upper_bits.has_value()) {
        return {BusFault::unsupported_access, std::nullopt};
    }
    const auto read = calendar_.read(*local_address);
    if (!read.succeeded()) {
        return {calendar_fault(read.status), std::nullopt};
    }
    const auto value = static_cast<std::uint8_t>(
        *upper_bits | *read.value
    );
    notify_read(address, value, kind);
    return {BusFault::none, value};
}

BusWriteResult Jr800Bus::write_experimental_calendar(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    const auto local_address = experimental_calendar_address(address);
    const auto upper_bits = experimental_calendar_upper_bits();
    if (!local_address.has_value() || !upper_bits.has_value()) {
        return {BusFault::unsupported_access, 0U, false};
    }
    const auto write = calendar_.write(*local_address, value);
    if (!write.succeeded()) {
        return {calendar_fault(write.status), 0U, false};
    }
    const auto previous = write.previous_value_known
        ? std::optional<std::uint8_t>{static_cast<std::uint8_t>(
            *upper_bits | write.previous_value
        )}
        : std::nullopt;
    notify_write(address, value, previous);
    return {
        BusFault::none,
        previous.value_or(0U),
        previous.has_value(),
    };
}

std::optional<std::uint8_t> Jr800Bus::experimental_calendar_address(
    std::uint16_t address
) const noexcept {
    const auto shift = static_cast<std::uint8_t>(
        experimental_calendar_configuration_->address_source
    );
    if (shift > 5U) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>((address >> shift) & 0x0FU);
}

std::optional<std::uint8_t>
Jr800Bus::experimental_calendar_upper_bits() const noexcept {
    const auto value = static_cast<std::uint8_t>(
        experimental_calendar_configuration_->upper_read_bits
    );
    if (value != 0x00U && value != 0xF0U) {
        return std::nullopt;
    }
    return value;
}

BusReadResult Jr800Bus::read_experimental_lcd(
    std::uint16_t address,
    AccessKind kind
) noexcept {
    const auto read = lcd_.read8(address);
    if (!read.succeeded()) {
        return {lcd_fault(read.status), std::nullopt};
    }
    if (!read.selection.has_value()) {
        return {BusFault::unsupported_access, std::nullopt};
    }

    std::optional<std::uint8_t> value;
    bool substituted = false;
    if (read.fully_known()) {
        value = read.value;
    } else if (read.selection->target == Jr800LcdRegister::display_data) {
        value = experimental_lcd_configuration_->unknown_data_read_value;
        substituted = true;
    } else {
        return {BusFault::uninitialized_read, std::nullopt};
    }

    if (read.selection->target == Jr800LcdRegister::display_data) {
        const auto completion = lcd_.complete_busy_period(
            read.selection->controller_index
        );
        if (completion != Jr800LcdAccessStatus::ok) {
            return {lcd_fault(completion), std::nullopt};
        }
    }

    if (substituted) {
        ++lcd_substituted_data_read_count_;
    }

    notify_read(address, *value, kind);
    return {BusFault::none, value};
}

BusWriteResult Jr800Bus::write_experimental_lcd(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    const auto write = lcd_.write8(address, value);
    if (!write.succeeded()) {
        return {lcd_fault(write.status), 0U, false};
    }
    if (!write.selection.has_value()) {
        return {BusFault::unsupported_access, 0U, false};
    }

    const auto completion = lcd_.complete_busy_period(
        write.selection->controller_index
    );
    if (completion != Jr800LcdAccessStatus::ok) {
        return {lcd_fault(completion), 0U, false};
    }

    notify_write(address, value, std::nullopt);
    return {BusFault::none, 0U, false};
}

std::optional<Jr800LcdControllerState> Jr800Bus::inspect_lcd_controller(
    std::uint8_t controller_index
) const noexcept {
    if (!experimental_lcd_configuration_.has_value()) {
        return std::nullopt;
    }
    return lcd_.inspect_controller(controller_index);
}

std::optional<std::uint8_t> Jr800Bus::lcd_display_ram_value(
    std::uint8_t controller_index,
    std::uint8_t x,
    std::uint8_t y
) const noexcept {
    if (!experimental_lcd_configuration_.has_value()) {
        return std::nullopt;
    }
    return lcd_.display_ram_value(controller_index, x, y);
}

std::optional<bool> Jr800Bus::lcd_panel_dot(
    std::size_t column,
    std::size_t row
) const noexcept {
    if (!experimental_lcd_configuration_.has_value()) {
        return std::nullopt;
    }
    return lcd_.display_panel_dot(column, row);
}

std::optional<std::uint8_t> Jr800Bus::lcd_indicator_ram_value(
    Jr800LcdIndicator indicator
) const noexcept {
    if (!experimental_lcd_configuration_.has_value()) {
        return std::nullopt;
    }
    return lcd_.indicator_ram_value(indicator);
}

std::optional<std::uint64_t>
Jr800Bus::lcd_substituted_data_read_count() const noexcept {
    if (!experimental_lcd_configuration_.has_value()) {
        return std::nullopt;
    }
    return lcd_substituted_data_read_count_;
}

}  // namespace jr800::core
