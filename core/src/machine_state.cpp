// SPDX-License-Identifier: MIT
#include "jr800/core/jr800_machine.hpp"
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace jr800::core {
// Fixed, little-endian field schema; no object layouts, pointers or ROM bytes.
// Change the format version whenever fields change. RTC remains live on restore.
class MachineStateCodec {
public:
    explicit MachineStateCodec(std::vector<std::uint8_t>& output) : output_(&output) {}
    explicit MachineStateCodec(std::span<const std::uint8_t> input) : input_(input) {}
    template<class... T> void operator()(T&... items) { (value(items), ...); }
    void finish() const {
        if (!output_ && position_ != input_.size()) throw std::invalid_argument("Trailing machine state bytes");
    }
private:
    std::vector<std::uint8_t>* output_{};
    std::span<const std::uint8_t> input_{};
    std::size_t position_{};
    void value(bool& item) {
        std::uint8_t byte = item ? 1 : 0;
        value(byte);
        if (byte > 1) throw std::invalid_argument("Invalid state boolean");
        item = byte != 0;
    }
    template<class T> requires (std::is_unsigned_v<T> && !std::is_same_v<T, bool>)
    void value(T& item) {
        if (output_) {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                output_->push_back(static_cast<std::uint8_t>(item >> (8U * i)));
        } else {
            if (input_.size() - position_ < sizeof(T)) throw std::invalid_argument("Truncated machine state");
            std::uint64_t result = 0;
            for (std::size_t i = 0; i < sizeof(T); ++i)
                result |= static_cast<std::uint64_t>(input_[position_++]) << (8U * i);
            item = static_cast<T>(result);
        }
    }
    template<class T> requires std::is_enum_v<T>
    void value(T& item) { auto raw = static_cast<std::underlying_type_t<T>>(item); value(raw); item = static_cast<T>(raw); }
    template<class T, std::size_t N> void value(std::array<T,N>& items) { for (auto& item : items) value(item); }
    template<class T> void value(std::optional<T>& item) {
        bool present = item.has_value(); value(present);
        if (!present) { item.reset(); return; }
        if (!item) item.emplace();
        value(*item);
    }
    void value(Hd6301v1Ports& s) {
        (*this)(s.port1_data_direction_, s.port1_data_, s.port2_data_direction_, s.port2_data_latch_, s.port4_data_direction_, s.port1_pin_value_, s.port1_pin_known_mask_, s.port2_pin_value_, s.port2_pin_known_mask_);
    }
    void value(Hd6301v1RamControl& s) {
        (*this)(s.standby_power_valid_, s.ram_enabled_);
    }
    void value(Hd6301v1Sci& s) {
        (*this)(s.rate_mode_bits_, s.control_bits_, s.receive_status_known_, s.status_known_cycles_remaining_, s.receive_pin_high_, s.receive_pin_known_);
    }
    void value(Hd6301v1Timer& s) {
        (*this)(s.control_bits_, s.status_bits_, s.free_running_counter_, s.pending_free_running_counter_high_, s.free_running_counter_high_write_pending_, s.output_compare_, s.output_compare_level_, s.input_capture_, s.overflow_clear_armed_, s.output_compare_clear_armed_, s.input_capture_clear_armed_, s.input_capture_pin_value_, s.input_capture_pin_known_, s.input_capture_enabled_, s.input_capture_status_known_, s.comparison_inhibit_cycles_);
    }
    void value(Hd44102& s) {
        (*this)(s.reset_asserted_, s.busy_, s.count_up_, s.display_off_, s.display_start_page_, s.x_address_, s.y_address_, s.output_register_, s.display_ram_, s.pending_operation_, s.pending_value_, s.pending_display_read_, s.pending_display_write_);
        const auto address_ok = [](const auto& p) { return !p || (p->x < 4 && p->y < 50 && p->next_y < 50); };
        if ((s.x_address_ && *s.x_address_ >= 4) || (s.y_address_ && *s.y_address_ >= 50)
            || (s.display_start_page_ && *s.display_start_page_ >= 4)
            || s.pending_operation_ > Hd44102::PendingOperation::write_display_data
            || !address_ok(s.pending_display_read_) || !address_ok(s.pending_display_write_))
            throw std::invalid_argument("Invalid LCD checkpoint address");
    }
    void value(Jr800Keyboard& s) {
        (*this)(s.values_, s.known_, s.pressed_masks_, s.read_addresses_, s.read_attempts_, s.distinct_addresses_);
    }
    void value(Jr800Lcd& s) {
        (*this)(s.controllers_);
    }
    void value(Jr800Memory& s) {
        (*this)(s.internal_ram_, s.internal_ram_valid_, s.standard_ram_, s.standard_ram_valid_, s.expansion_ram_, s.expansion_ram_initialized_);
    }
    void value(Jr800Bus& s) {
        (*this)(s.ports_, s.ram_control_, s.sci_, s.timer_, s.keyboard_, s.lcd_, s.memory_, s.experimental_lcd_configuration_, s.ignore_unsupported_io_, s.lcd_substituted_data_read_count_, s.ignored_io_access_count_);
    }
    void value(Hd44102::PendingDisplayRead& s) { (*this)(s.x, s.y, s.next_y); }
    void value(Hd44102::PendingDisplayWrite& s) { (*this)(s.x, s.y, s.next_y, s.value); }
    void value(Jr800ExperimentalLcdConfiguration& s) { (*this)(s.unknown_data_read_value); }
    void value(CpuStateKnowledge& s) { (*this)(s.registers, s.condition_code); }
    void value(CpuState& s) { (*this)(s.pc, s.sp, s.x, s.a, s.b, s.condition_code, s.cycle_count, s.execution_state, s.maskable_interrupt_delay_cycles, s.knowledge); }
    void value(Jr800ExperimentalResetStateConfiguration& s) { (*this)(s.stack_pointer, s.index_register, s.accumulator_a, s.accumulator_b, s.half_carry, s.negative, s.zero, s.overflow, s.carry); }
};
std::vector<std::uint8_t> Jr800Machine::save_state() const {
    auto copy = clone();
    std::vector<std::uint8_t> bytes;
    MachineStateCodec codec(bytes);
    std::uint32_t version = 1;
    auto cpu = execution_.cpu().state();
    auto profile = execution_.cpu().profile();
    codec(version, profile, cpu, copy->reset_state_configuration_, copy->bus_);
    return bytes;
}
void Jr800Machine::restore_state(std::span<const std::uint8_t> bytes) {
    auto candidate = clone();
    MachineStateCodec codec(bytes);
    std::uint32_t version{};
    auto profile = execution_.cpu().profile();
    auto cpu = execution_.cpu().state();
    codec(version);
    if (version != 1) throw std::invalid_argument("Unsupported machine state version");
    codec(profile, cpu, candidate->reset_state_configuration_, candidate->bus_);
    codec.finish();
    if (profile != isa::CpuProfile::hd6301v1 || cpu.execution_state > CpuExecutionState::waiting_for_interrupt)
        throw std::invalid_argument("Invalid machine state CPU");
    candidate->execution_.initialize_known_state(profile, cpu);
    copy_state_from(*candidate);
}
} // namespace jr800::core
