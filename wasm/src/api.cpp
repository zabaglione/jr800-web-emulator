// SPDX-License-Identifier: MIT

#include "jr800/wasm/api.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/core/jr800_machine.hpp"
#include "jr800/core/jr800_memory.hpp"
#include "jr800/core/synthetic_machine.hpp"
#include "jr800/debugger/debugger.hpp"
#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/jr8rom.hpp"
#include "jr800/formats/linked_error.hpp"
#include "jr800/formats/native_msave.hpp"
#include "jr800/isa/instruction_metadata.hpp"
#include "jr800/runtime/application_loader.hpp"
#include "jr800/runtime/program_save_capture.hpp"

static_assert(sizeof(jr800_program_info) == 28U);
static_assert(sizeof(jr800_program_saves_state) == 8U);
static_assert(sizeof(jr800_machine_state) == JR800_STATE_WORD_COUNT * sizeof(std::uint32_t));
static_assert(sizeof(jr800_stop_info) == JR800_STOP_WORD_COUNT * sizeof(std::uint32_t));
static_assert(
    sizeof(jr800_step_out_state)
        == JR800_STEP_OUT_STATE_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(sizeof(jr800_history_entry) == JR800_HISTORY_WORD_COUNT * sizeof(std::uint32_t));
static_assert(sizeof(jr800_access_record) == JR800_ACCESS_WORD_COUNT * sizeof(std::uint32_t));
static_assert(
    sizeof(jr800_access_filter)
        == JR800_ACCESS_FILTER_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(sizeof(jr800_source_location) == JR800_SOURCE_WORD_COUNT * sizeof(std::uint32_t));
static_assert(sizeof(jr800_disassembly) == JR800_DISASSEMBLY_WORD_COUNT * sizeof(std::uint32_t));
static_assert(
    sizeof(jr800_hardware_configuration)
        == JR800_HARDWARE_CONFIGURATION_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    sizeof(jr800_suspended_advance)
        == JR800_SUSPENDED_ADVANCE_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    sizeof(jr800_expression_watch_result)
        == JR800_EXPRESSION_WATCH_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    sizeof(jr800_symbol_watch_result)
        == JR800_SYMBOL_WATCH_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    sizeof(jr800_keyboard_activity)
        == JR800_KEYBOARD_ACTIVITY_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    sizeof(jr800_lcd_indicator_raw)
        == JR800_LCD_INDICATOR_RAW_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    sizeof(jr800_native_program_wav_issue)
        == JR800_NATIVE_PROGRAM_WAV_ISSUE_WORD_COUNT * sizeof(std::uint32_t)
);
static_assert(
    JR800_NATIVE_PROGRAM_WAV_ISSUE_AMBIGUOUS_HEADER_BYTE_ORDER
        == static_cast<int>(
            jr800::formats::NativeMsaveIssueCode::ambiguous_header_byte_order
        ) + 1
);
enum class MachineKind : std::uint8_t {
    synthetic_application,
    jr800,
};

struct jr800_machine {
    MachineKind kind{MachineKind::synthetic_application};
    std::unique_ptr<jr800::core::SyntheticMachine> synthetic_machine;
    std::unique_ptr<jr800::core::Jr800Machine> hardware_machine;
    std::unique_ptr<jr800::core::Jr800Machine> initial_hardware_state;
    jr800::debugger::Debugger debugger{256U, 1024U};
    std::optional<jr800::formats::jr8app::Application> application;
    std::uint16_t initial_stack_pointer{0x01FFU};
    bool logical_rom_loaded{};
    std::unique_ptr<jr800::runtime::ProgramSaveCapture> program_saves;

    jr800_machine()
        : synthetic_machine(
            std::make_unique<jr800::core::SyntheticMachine>()
        ) {
        if (!debugger.attach(execution())) {
            throw std::bad_alloc{};
        }
    }

    explicit jr800_machine(
        jr800::core::Jr800ExperimentalMachineConfiguration configuration,
        jr800::core::Jr800ExperimentalResetStateConfiguration
            reset_state_configuration
    )
        : kind(MachineKind::jr800),
          hardware_machine(
              std::make_unique<jr800::core::Jr800Machine>(
                  configuration,
                  reset_state_configuration
              )
          ) {
        if (!debugger.attach(execution())) {
            throw std::bad_alloc{};
        }
    }

    [[nodiscard]] jr800::core::Machine& execution() noexcept {
        return kind == MachineKind::synthetic_application
            ? synthetic_machine->execution()
            : hardware_machine->execution();
    }

    [[nodiscard]] const jr800::core::Machine& execution() const noexcept {
        return kind == MachineKind::synthetic_application
            ? synthetic_machine->execution()
            : hardware_machine->execution();
    }

    [[nodiscard]] bool ready() const noexcept {
        return kind == MachineKind::synthetic_application
            ? application.has_value()
            : logical_rom_loaded;
    }

    [[nodiscard]] jr800_status not_ready_status() const noexcept {
        return kind == MachineKind::synthetic_application
            ? JR800_STATUS_NO_APPLICATION
            : JR800_STATUS_NO_ROM;
    }
};

namespace {

constexpr std::array<
    jr800::core::Jr800LcdIndicator,
    JR800_LCD_INDICATOR_COUNT
> lcd_indicators{
    jr800::core::Jr800LcdIndicator::page_1,
    jr800::core::Jr800LcdIndicator::page_2,
    jr800::core::Jr800LcdIndicator::page_3,
    jr800::core::Jr800LcdIndicator::page_4,
    jr800::core::Jr800LcdIndicator::page_5,
    jr800::core::Jr800LcdIndicator::page_6,
    jr800::core::Jr800LcdIndicator::page_7,
    jr800::core::Jr800LcdIndicator::page_8,
    jr800::core::Jr800LcdIndicator::capital_lock,
    jr800::core::Jr800LcdIndicator::graphics_input,
    jr800::core::Jr800LcdIndicator::kana_input,
    jr800::core::Jr800LcdIndicator::insert_mode,
    jr800::core::Jr800LcdIndicator::control_mode,
    jr800::core::Jr800LcdIndicator::radian_mode,
    jr800::core::Jr800LcdIndicator::degree_mode,
    jr800::core::Jr800LcdIndicator::battery_warning,
};

std::uint32_t low_word(std::uint64_t value) noexcept {
    return static_cast<std::uint32_t>(value & 0xFFFF'FFFFULL);
}

std::uint32_t high_word(std::uint64_t value) noexcept {
    return static_cast<std::uint32_t>(value >> 32U);
}

std::uint64_t combine_words(
    std::uint32_t low,
    std::uint32_t high
) noexcept {
    return static_cast<std::uint64_t>(low)
        | (static_cast<std::uint64_t>(high) << 32U);
}

std::uint32_t profile_id(jr800::isa::CpuProfile profile) noexcept {
    using jr800::isa::CpuProfile;
    switch (profile) {
    case CpuProfile::jr800_unresolved:
        return JR800_PROFILE_UNRESOLVED;
    case CpuProfile::mc6801:
        return JR800_PROFILE_MC6801;
    case CpuProfile::hd6301v1:
        return JR800_PROFILE_HD6301V1;
    }
    return JR800_PROFILE_UNRESOLVED;
}

std::uint32_t calendar_alarm_terminal(
    const jr800_machine& machine
) noexcept {
    if (machine.kind != MachineKind::jr800) {
        return JR800_CALENDAR_ALARM_TERMINAL_DISCONNECTED;
    }

    const auto state =
        machine.hardware_machine->calendar_alarm_terminal_state();
    if (!state.connected) {
        return JR800_CALENDAR_ALARM_TERMINAL_DISCONNECTED;
    }
    if (!state.pull_low.has_value()) {
        return JR800_CALENDAR_ALARM_TERMINAL_UNKNOWN;
    }
    return *state.pull_low
        ? JR800_CALENDAR_ALARM_TERMINAL_PULL_LOW
        : JR800_CALENDAR_ALARM_TERMINAL_RELEASED;
}

std::uint32_t port2_timer_output(const jr800_machine& machine) noexcept {
    if (machine.kind != MachineKind::jr800) {
        return JR800_PORT2_TIMER_OUTPUT_UNAVAILABLE;
    }

    const auto state = machine.hardware_machine->port2_timer_output_state();
    if (!state.output_enabled) {
        return JR800_PORT2_TIMER_OUTPUT_DISABLED;
    }
    if (!state.level.has_value()) {
        return JR800_PORT2_TIMER_OUTPUT_UNKNOWN;
    }
    return *state.level
        ? JR800_PORT2_TIMER_OUTPUT_HIGH
        : JR800_PORT2_TIMER_OUTPUT_LOW;
}

std::optional<std::uint64_t> lcd_substituted_data_read_count(
    const jr800_machine& machine
) noexcept {
    if (machine.kind != MachineKind::jr800) {
        return std::nullopt;
    }
    return machine.hardware_machine->lcd_substituted_data_read_count();
}

std::optional<std::uint64_t> ignored_io_access_count(
    const jr800_machine& machine
) noexcept {
    if (machine.kind != MachineKind::jr800) {
        return std::nullopt;
    }
    return machine.hardware_machine->ignored_io_access_count();
}

std::uint32_t stop_reason(jr800::debugger::StopReason reason) noexcept {
    using jr800::debugger::StopReason;
    switch (reason) {
    case StopReason::step_complete:
        return JR800_STOP_STEP_COMPLETE;
    case StopReason::instruction_limit:
        return JR800_STOP_INSTRUCTION_LIMIT;
    case StopReason::execution_breakpoint:
        return JR800_STOP_EXECUTION_BREAKPOINT;
    case StopReason::memory_watchpoint:
        return JR800_STOP_MEMORY_WATCHPOINT;
    case StopReason::cpu_fault:
        return JR800_STOP_CPU_FAULT;
    case StopReason::detached:
        return JR800_STOP_DETACHED;
    case StopReason::sleeping:
        return JR800_STOP_SLEEPING;
    case StopReason::address_reached:
        return JR800_STOP_ADDRESS_REACHED;
    case StopReason::step_out_complete:
        return JR800_STOP_STEP_OUT_COMPLETE;
    case StopReason::breakpoint_condition_error:
        return JR800_STOP_BREAKPOINT_CONDITION_ERROR;
    }
    return JR800_STOP_DETACHED;
}

std::uint32_t expression_error(
    jr800::debugger::ExpressionEvaluationError error
) noexcept {
    using Error = jr800::debugger::ExpressionEvaluationError;
    switch (error) {
    case Error::none:
        return JR800_EXPRESSION_OK;
    case Error::unknown_state:
        return JR800_EXPRESSION_UNKNOWN_STATE;
    case Error::memory_access:
        return JR800_EXPRESSION_MEMORY_ACCESS;
    case Error::division_by_zero:
        return JR800_EXPRESSION_DIVISION_BY_ZERO;
    case Error::invalid_shift:
        return JR800_EXPRESSION_INVALID_SHIFT;
    case Error::address_out_of_range:
        return JR800_EXPRESSION_ADDRESS_OUT_OF_RANGE;
    case Error::symbol_not_found:
        return JR800_EXPRESSION_SYMBOL_NOT_FOUND;
    case Error::ambiguous_symbol:
        return JR800_EXPRESSION_AMBIGUOUS_SYMBOL;
    }
    return JR800_EXPRESSION_INVALID_SHIFT;
}

std::uint32_t execution_state(
    jr800::core::CpuExecutionState state
) noexcept {
    using jr800::core::CpuExecutionState;
    switch (state) {
    case CpuExecutionState::active:
        return JR800_CPU_ACTIVE;
    case CpuExecutionState::sleeping:
        return JR800_CPU_SLEEPING;
    case CpuExecutionState::waiting_for_interrupt:
        return JR800_CPU_WAITING_FOR_INTERRUPT;
    }
    return JR800_CPU_ACTIVE;
}

std::uint32_t cpu_fault(jr800::core::CpuFault fault) noexcept {
    using jr800::core::CpuFault;
    switch (fault) {
    case CpuFault::none:
        return JR800_FAULT_NONE;
    case CpuFault::unsupported_opcode:
        return JR800_FAULT_UNSUPPORTED_OPCODE;
    case CpuFault::unimplemented_operation:
        return JR800_FAULT_UNIMPLEMENTED_OPERATION;
    case CpuFault::bus_access:
        return JR800_FAULT_BUS_ACCESS;
    case CpuFault::unknown_state:
        return JR800_FAULT_UNKNOWN_STATE;
    case CpuFault::unknown_interrupt_request:
        return JR800_FAULT_UNKNOWN_INTERRUPT_REQUEST;
    case CpuFault::bus_advance:
        return JR800_FAULT_BUS_ADVANCE;
    }
    return JR800_FAULT_UNSUPPORTED_OPCODE;
}

std::uint32_t access_kind(jr800::core::AccessKind kind) noexcept {
    using jr800::core::AccessKind;
    switch (kind) {
    case AccessKind::instruction_fetch:
        return JR800_ACCESS_INSTRUCTION_FETCH;
    case AccessKind::data_read:
        return JR800_ACCESS_DATA_READ;
    case AccessKind::data_write:
        return JR800_ACCESS_DATA_WRITE;
    }
    return JR800_ACCESS_DATA_READ;
}

std::uint32_t bus_fault(jr800::core::BusFault fault) noexcept {
    using jr800::core::BusFault;
    switch (fault) {
    case BusFault::none:
        return JR800_BUS_FAULT_NONE;
    case BusFault::backing_store_unavailable:
        return JR800_BUS_FAULT_BACKING_STORE_UNAVAILABLE;
    case BusFault::uninitialized_read:
        return JR800_BUS_FAULT_UNINITIALIZED_READ;
    case BusFault::unsupported_access:
        return JR800_BUS_FAULT_UNSUPPORTED_ACCESS;
    case BusFault::read_only_write:
        return JR800_BUS_FAULT_READ_ONLY_WRITE;
    case BusFault::device_state_unknown:
        return JR800_BUS_FAULT_DEVICE_STATE_UNKNOWN;
    case BusFault::device_state_unsupported:
        return JR800_BUS_FAULT_DEVICE_STATE_UNSUPPORTED;
    }
    return JR800_BUS_FAULT_UNSUPPORTED_ACCESS;
}

std::uint32_t state_part(jr800::core::CpuStatePart part) noexcept {
    using jr800::core::CpuStatePart;
    switch (part) {
    case CpuStatePart::none:
        return JR800_STATE_PART_NONE;
    case CpuStatePart::program_counter:
        return JR800_STATE_PART_PROGRAM_COUNTER;
    case CpuStatePart::stack_pointer:
        return JR800_STATE_PART_STACK_POINTER;
    case CpuStatePart::index_register:
        return JR800_STATE_PART_INDEX_REGISTER;
    case CpuStatePart::accumulator_a:
        return JR800_STATE_PART_ACCUMULATOR_A;
    case CpuStatePart::accumulator_b:
        return JR800_STATE_PART_ACCUMULATOR_B;
    case CpuStatePart::condition_code:
        return JR800_STATE_PART_CONDITION_CODE;
    }
    return JR800_STATE_PART_NONE;
}

std::uint32_t step_kind(jr800::core::StepKind kind) noexcept {
    using jr800::core::StepKind;
    switch (kind) {
    case StepKind::dormant:
        return JR800_STEP_DORMANT;
    case StepKind::instruction:
        return JR800_STEP_INSTRUCTION;
    case StepKind::interrupt_entry:
        return JR800_STEP_INTERRUPT_ENTRY;
    case StepKind::sleep_resume:
        return JR800_STEP_SLEEP_RESUME;
    }
    return JR800_STEP_DORMANT;
}

std::uint32_t interrupt_source(
    jr800::core::InterruptSource source
) noexcept {
    using jr800::core::InterruptSource;
    switch (source) {
    case InterruptSource::none:
        return JR800_INTERRUPT_NONE;
    case InterruptSource::timer_input_capture:
        return JR800_INTERRUPT_TIMER_INPUT_CAPTURE;
    case InterruptSource::timer_output_compare:
        return JR800_INTERRUPT_TIMER_OUTPUT_COMPARE;
    case InterruptSource::timer_overflow:
        return JR800_INTERRUPT_TIMER_OVERFLOW;
    case InterruptSource::serial:
        return JR800_INTERRUPT_SERIAL;
    }
    return JR800_INTERRUPT_NONE;
}

jr800_status inspect_status(jr800::core::BusFault fault) noexcept {
    using jr800::core::BusFault;
    switch (fault) {
    case BusFault::none:
        return JR800_STATUS_OK;
    case BusFault::backing_store_unavailable:
        return JR800_STATUS_BACKING_STORE_UNAVAILABLE;
    case BusFault::uninitialized_read:
        return JR800_STATUS_UNINITIALIZED_READ;
    case BusFault::unsupported_access:
    case BusFault::read_only_write:
    case BusFault::device_state_unsupported:
        return JR800_STATUS_UNSUPPORTED_ACCESS;
    case BusFault::device_state_unknown:
        return JR800_STATUS_UNINITIALIZED_READ;
    }
    return JR800_STATUS_INTERNAL_ERROR;
}

jr800_status calendar_operation_status(
    jr800::core::Jr800CalendarOperationStatus status
) noexcept {
    using jr800::core::Jr800CalendarOperationStatus;
    switch (status) {
    case Jr800CalendarOperationStatus::ok:
        return JR800_STATUS_OK;
    case Jr800CalendarOperationStatus::calendar_disconnected:
    case Jr800CalendarOperationStatus::unsupported_state:
        return JR800_STATUS_UNSUPPORTED_ACCESS;
    case Jr800CalendarOperationStatus::unknown_state:
        return JR800_STATUS_UNINITIALIZED_READ;
    }
    return JR800_STATUS_INTERNAL_ERROR;
}

bool valid_boolean(std::uint32_t value) noexcept {
    return value <= 1U;
}

bool valid_byte(std::uint32_t value) noexcept {
    return value <= 0xFFU;
}

std::optional<jr800::core::Jr800Key> keyboard_key(
    std::uint32_t key
) noexcept {
    using jr800::core::Jr800Key;
    switch (key) {
    case JR800_KEY_SHIFT:
        return Jr800Key::shift;
    case JR800_KEY_CONTROL:
        return Jr800Key::control;
    case JR800_KEY_MENU:
        return Jr800Key::menu;
    case JR800_KEY_RETURN:
        return Jr800Key::return_key;
    case JR800_KEY_SPACE:
        return Jr800Key::space;
    case JR800_KEY_MAIN_1:
        return Jr800Key::main_1;
    case JR800_KEY_LETTER_A:
        return Jr800Key::letter_a;
    case JR800_KEY_LETTER_X:
        return Jr800Key::letter_x;
    case JR800_KEY_KEYPAD_INSERT_RUB:
        return Jr800Key::keypad_insert_rub;
    case JR800_KEY_KEYPAD_VERTICAL_ARROWS:
        return Jr800Key::keypad_vertical_arrows;
    case JR800_KEY_KEYPAD_HORIZONTAL_ARROWS:
        return Jr800Key::keypad_horizontal_arrows;
    case JR800_KEY_KEYPAD_0:
        return Jr800Key::keypad_0;
    case JR800_KEY_KEYPAD_1:
        return Jr800Key::keypad_1;
    case JR800_KEY_KEYPAD_2:
        return Jr800Key::keypad_2;
    case JR800_KEY_KEYPAD_3:
        return Jr800Key::keypad_3;
    case JR800_KEY_KEYPAD_4:
        return Jr800Key::keypad_4;
    case JR800_KEY_KEYPAD_5:
        return Jr800Key::keypad_5;
    case JR800_KEY_KEYPAD_6:
        return Jr800Key::keypad_6;
    case JR800_KEY_KEYPAD_7:
        return Jr800Key::keypad_7;
    case JR800_KEY_BREAK:
        return Jr800Key::break_key;
    case JR800_KEY_HOME_CLS:
        return Jr800Key::home_cls;
    case JR800_KEY_MAIN_0:
        return Jr800Key::main_0;
    case JR800_KEY_MAIN_2:
        return Jr800Key::main_2;
    case JR800_KEY_MAIN_3:
        return Jr800Key::main_3;
    case JR800_KEY_MAIN_4:
        return Jr800Key::main_4;
    case JR800_KEY_MAIN_5:
        return Jr800Key::main_5;
    case JR800_KEY_MAIN_6:
        return Jr800Key::main_6;
    case JR800_KEY_MAIN_7:
        return Jr800Key::main_7;
    case JR800_KEY_MAIN_8:
        return Jr800Key::main_8;
    case JR800_KEY_MAIN_9:
        return Jr800Key::main_9;
    case JR800_KEY_MAIN_CARET:
        return Jr800Key::main_caret;
    case JR800_KEY_LETTER_B:
        return Jr800Key::letter_b;
    case JR800_KEY_LETTER_C:
        return Jr800Key::letter_c;
    case JR800_KEY_LETTER_D:
        return Jr800Key::letter_d;
    case JR800_KEY_LETTER_E:
        return Jr800Key::letter_e;
    case JR800_KEY_LETTER_F:
        return Jr800Key::letter_f;
    case JR800_KEY_LETTER_G:
        return Jr800Key::letter_g;
    case JR800_KEY_LETTER_H:
        return Jr800Key::letter_h;
    case JR800_KEY_LETTER_I:
        return Jr800Key::letter_i;
    case JR800_KEY_LETTER_J:
        return Jr800Key::letter_j;
    case JR800_KEY_LETTER_K:
        return Jr800Key::letter_k;
    case JR800_KEY_LETTER_L:
        return Jr800Key::letter_l;
    case JR800_KEY_LETTER_M:
        return Jr800Key::letter_m;
    case JR800_KEY_LETTER_N:
        return Jr800Key::letter_n;
    case JR800_KEY_LETTER_O:
        return Jr800Key::letter_o;
    case JR800_KEY_LETTER_P:
        return Jr800Key::letter_p;
    case JR800_KEY_LETTER_Q:
        return Jr800Key::letter_q;
    case JR800_KEY_LETTER_R:
        return Jr800Key::letter_r;
    case JR800_KEY_LETTER_S:
        return Jr800Key::letter_s;
    case JR800_KEY_LETTER_T:
        return Jr800Key::letter_t;
    case JR800_KEY_LETTER_U:
        return Jr800Key::letter_u;
    case JR800_KEY_LETTER_V:
        return Jr800Key::letter_v;
    case JR800_KEY_LETTER_W:
        return Jr800Key::letter_w;
    case JR800_KEY_LETTER_Y:
        return Jr800Key::letter_y;
    case JR800_KEY_LETTER_Z:
        return Jr800Key::letter_z;
    case JR800_KEY_COLON:
        return Jr800Key::colon;
    case JR800_KEY_SEMICOLON:
        return Jr800Key::semicolon;
    case JR800_KEY_COMMA:
        return Jr800Key::comma;
    case JR800_KEY_PERIOD:
        return Jr800Key::period;
    case JR800_KEY_PF_1:
        return Jr800Key::pf_1;
    case JR800_KEY_PF_2:
        return Jr800Key::pf_2;
    case JR800_KEY_PF_3:
        return Jr800Key::pf_3;
    case JR800_KEY_PF_4:
        return Jr800Key::pf_4;
    case JR800_KEY_PF_5:
        return Jr800Key::pf_5;
    case JR800_KEY_PF_6:
        return Jr800Key::pf_6;
    case JR800_KEY_PF_7:
        return Jr800Key::pf_7;
    case JR800_KEY_PF_8:
        return Jr800Key::pf_8;
    case JR800_KEY_PF_9:
        return Jr800Key::pf_9;
    case JR800_KEY_PF_10:
        return Jr800Key::pf_10;
    case JR800_KEY_KEYPAD_8:
        return Jr800Key::keypad_8;
    case JR800_KEY_KEYPAD_9:
        return Jr800Key::keypad_9;
    case JR800_KEY_KEYPAD_MULTIPLY:
        return Jr800Key::keypad_multiply;
    case JR800_KEY_KEYPAD_ADD:
        return Jr800Key::keypad_add;
    case JR800_KEY_KEYPAD_EQUAL:
        return Jr800Key::keypad_equal;
    case JR800_KEY_KEYPAD_SUBTRACT:
        return Jr800Key::keypad_subtract;
    case JR800_KEY_KEYPAD_DECIMAL:
        return Jr800Key::keypad_decimal;
    case JR800_KEY_KEYPAD_DIVIDE:
        return Jr800Key::keypad_divide;
    }
    return std::nullopt;
}

bool disabled_value_is_zero(
    std::uint32_t enabled,
    std::uint32_t value
) noexcept {
    return enabled != 0U || value == 0U;
}

struct ParsedJr800Configuration {
    jr800::core::Jr800ExperimentalMachineConfiguration machine;
    jr800::core::Jr800ExperimentalResetStateConfiguration reset_state;
};

std::optional<bool> reset_condition_flag(
    const jr800_hardware_configuration& source,
    jr800::core::ConditionCode flag
) noexcept {
    const auto mask = jr800::core::condition_mask(flag);
    if ((source.reset_condition_code_known_mask & mask) == 0U) {
        return std::nullopt;
    }
    return (source.reset_condition_code_value & mask) != 0U;
}

std::optional<ParsedJr800Configuration>
machine_configuration(const jr800_hardware_configuration& source) noexcept {
    if (source.abi_version != JR800_WASM_ABI_VERSION
        || !valid_boolean(source.reset_stack_pointer_enabled)
        || !valid_boolean(source.reset_index_register_enabled)
        || !valid_boolean(source.reset_accumulator_a_enabled)
        || !valid_boolean(source.reset_accumulator_b_enabled)
        || !valid_boolean(source.internal_ram_enabled)
        || !valid_boolean(source.standard_ram_enabled)
        || !valid_boolean(source.expansion_ram_enabled)
        || !valid_boolean(source.lcd_enabled)
        || !valid_boolean(source.calendar_enabled)
        || !valid_boolean(source.ram_standby_known)
        || !valid_boolean(source.ram_standby_valid)
        || !valid_boolean(source.keyboard_window_known)
        || !valid_boolean(source.ignore_unsupported_io)
        || source.reset_stack_pointer_value > 0xFFFFU
        || source.reset_index_register_value > 0xFFFFU
        || !valid_byte(source.reset_accumulator_a_value)
        || !valid_byte(source.reset_accumulator_b_value)
        || (source.reset_condition_code_known_mask & ~0x2FU) != 0U
        || (source.reset_condition_code_value
                & ~source.reset_condition_code_known_mask) != 0U
        || !valid_byte(source.internal_ram_initial_value)
        || !valid_byte(source.standard_ram_initial_value)
        || !valid_byte(source.expansion_ram_initial_value)
        || !valid_byte(source.lcd_unknown_data_read_value)
        || !valid_byte(source.port1_pin_known_mask)
        || !valid_byte(source.port1_pin_value)
        || source.port2_pin_known_mask > 0x1FU
        || source.port2_pin_value > 0x1FU
        || (source.port1_pin_value & ~source.port1_pin_known_mask) != 0U
        || (source.port2_pin_value & ~source.port2_pin_known_mask) != 0U
        || !valid_byte(source.keyboard_window_value)
        || !disabled_value_is_zero(
            source.reset_stack_pointer_enabled,
            source.reset_stack_pointer_value
        )
        || !disabled_value_is_zero(
            source.reset_index_register_enabled,
            source.reset_index_register_value
        )
        || !disabled_value_is_zero(
            source.reset_accumulator_a_enabled,
            source.reset_accumulator_a_value
        )
        || !disabled_value_is_zero(
            source.reset_accumulator_b_enabled,
            source.reset_accumulator_b_value
        )
        || !disabled_value_is_zero(
            source.internal_ram_enabled,
            source.internal_ram_initial_value
        )
        || !disabled_value_is_zero(
            source.standard_ram_enabled,
            source.standard_ram_initial_value
        )
        || !disabled_value_is_zero(
            source.expansion_ram_enabled,
            source.expansion_ram_initial_value
        )
        || !disabled_value_is_zero(
            source.lcd_enabled,
            source.lcd_unknown_data_read_value
        )
        || !disabled_value_is_zero(
            source.keyboard_window_known,
            source.keyboard_window_value
        )
        || (!source.ram_standby_known && source.ram_standby_valid)
        || (source.expansion_ram_enabled && !source.standard_ram_enabled)) {
        return std::nullopt;
    }

    using AddressSource =
        jr800::core::Jr800ExperimentalCalendarAddressSource;
    using UpperReadBits =
        jr800::core::Jr800ExperimentalCalendarUpperReadBits;
    if (source.calendar_enabled) {
        if (source.calendar_address_source
                > static_cast<std::uint32_t>(AddressSource::cpu_a5_to_a8)
            || (source.calendar_upper_read_bits
                    != static_cast<std::uint32_t>(UpperReadBits::all_zero)
                && source.calendar_upper_read_bits
                    != static_cast<std::uint32_t>(UpperReadBits::all_one))
            || source.calendar_cpu_cycle_ratio
                > JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ) {
            return std::nullopt;
        }
    } else if (source.calendar_address_source != 0U
               || source.calendar_upper_read_bits != 0U
               || source.calendar_cpu_cycle_ratio != 0U) {
        return std::nullopt;
    }

    ParsedJr800Configuration configuration;
    configuration.machine.ignore_unsupported_io = source.ignore_unsupported_io != 0U;
    configuration.reset_state = {
        .stack_pointer = source.reset_stack_pointer_enabled
            ? std::optional<std::uint16_t>{
                static_cast<std::uint16_t>(
                    source.reset_stack_pointer_value
                ),
            }
            : std::nullopt,
        .index_register = source.reset_index_register_enabled
            ? std::optional<std::uint16_t>{
                static_cast<std::uint16_t>(
                    source.reset_index_register_value
                ),
            }
            : std::nullopt,
        .accumulator_a = source.reset_accumulator_a_enabled
            ? std::optional<std::uint8_t>{
                static_cast<std::uint8_t>(
                    source.reset_accumulator_a_value
                ),
            }
            : std::nullopt,
        .accumulator_b = source.reset_accumulator_b_enabled
            ? std::optional<std::uint8_t>{
                static_cast<std::uint8_t>(
                    source.reset_accumulator_b_value
                ),
            }
            : std::nullopt,
        .half_carry = reset_condition_flag(
            source,
            jr800::core::ConditionCode::half_carry
        ),
        .negative = reset_condition_flag(
            source,
            jr800::core::ConditionCode::negative
        ),
        .zero = reset_condition_flag(
            source,
            jr800::core::ConditionCode::zero
        ),
        .overflow = reset_condition_flag(
            source,
            jr800::core::ConditionCode::overflow
        ),
        .carry = reset_condition_flag(
            source,
            jr800::core::ConditionCode::carry
        ),
    };
    if (source.internal_ram_enabled) {
        configuration.machine.internal_ram =
            jr800::core::Jr800ExperimentalInternalRamConfiguration{
                static_cast<std::uint8_t>(
                    source.internal_ram_initial_value
                ),
            };
    }
    if (source.standard_ram_enabled) {
        configuration.machine.memory =
            jr800::core::Jr800ExperimentalMemoryConfiguration{
                static_cast<std::uint8_t>(
                    source.standard_ram_initial_value
                ),
                source.expansion_ram_enabled
                    ? std::optional<std::uint8_t>{
                        static_cast<std::uint8_t>(
                            source.expansion_ram_initial_value
                        ),
                    }
                    : std::nullopt,
            };
    }
    if (source.lcd_enabled) {
        configuration.machine.lcd =
            jr800::core::Jr800ExperimentalLcdConfiguration{
                static_cast<std::uint8_t>(
                    source.lcd_unknown_data_read_value
                ),
            };
    }
    if (source.calendar_enabled) {
        configuration.machine.calendar =
            jr800::core::Jr800ExperimentalCalendarConfiguration{
                static_cast<AddressSource>(source.calendar_address_source),
                static_cast<UpperReadBits>(source.calendar_upper_read_bits),
                source.calendar_cpu_cycle_ratio
                        == JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ
                    ? std::optional<
                        jr800::core::Jr800ExperimentalCalendarCpuCycleRatio
                    >{
                        jr800::core::Jr800ExperimentalCalendarCpuCycleRatio::
                            e030_nominal_1_2288_mhz,
                    }
                    : std::nullopt,
            };
    }
    return configuration;
}

bool apply_jr800_inputs(
    jr800::core::Jr800Machine& machine,
    const jr800_hardware_configuration& configuration
) noexcept {
    machine.set_port1_pin_state(
        static_cast<std::uint8_t>(configuration.port1_pin_value),
        static_cast<std::uint8_t>(configuration.port1_pin_known_mask)
    );
    machine.set_port2_pin_state(
        static_cast<std::uint8_t>(configuration.port2_pin_value),
        static_cast<std::uint8_t>(configuration.port2_pin_known_mask)
    );
    machine.set_ram_standby_power_valid(
        configuration.ram_standby_valid != 0U,
        configuration.ram_standby_known != 0U
    );
    if (configuration.keyboard_window_known) {
        for (std::uint32_t address = 0x0C00U; address <= 0x0FFFU; ++address) {
            if (!machine.set_keyboard_bus_response(
                    static_cast<std::uint16_t>(address),
                    static_cast<std::uint8_t>(
                        configuration.keyboard_window_value
                    ),
                    true
                )) {
                return false;
            }
        }
    }
    return true;
}

jr800_status load_status(jr800::runtime::LoadApplicationResult result) noexcept {
    using jr800::runtime::LoadApplicationResult;
    switch (result) {
    case LoadApplicationResult::unsupported_basic_rom: return JR800_STATUS_UNSUPPORTED_BASIC_ROM;
    case LoadApplicationResult::basic_not_ready: return JR800_STATUS_BASIC_NOT_READY;
    case LoadApplicationResult::invalid_basic_program: return JR800_STATUS_INVALID_BASIC_PROGRAM;
    case LoadApplicationResult::basic_load_failed: return JR800_STATUS_BASIC_LOAD_FAILED;
    case LoadApplicationResult::loaded:
        return JR800_STATUS_OK;
    case LoadApplicationResult::invalid_format:
        return JR800_STATUS_INVALID_APPLICATION;
    case LoadApplicationResult::unknown_profile:
        return JR800_STATUS_UNKNOWN_PROFILE;
    case LoadApplicationResult::unreviewed_profile:
        return JR800_STATUS_UNREVIEWED_PROFILE;
    case LoadApplicationResult::segment_out_of_range:
        return JR800_STATUS_SEGMENT_OUT_OF_RANGE;
    case LoadApplicationResult::entry_point_not_loaded:
        return JR800_STATUS_ENTRY_POINT_NOT_LOADED;
    case LoadApplicationResult::target_mismatch:
        return JR800_STATUS_TARGET_MISMATCH;
    }
    return JR800_STATUS_INTERNAL_ERROR;
}

jr800_status load_jr800_application(
    jr800_machine& machine,
    const jr800::formats::jr8app::Application& application,
    bool run_after_load, jr800_program_info* info
) {
    const auto loaded = jr800::runtime::load_application(
        *machine.hardware_machine,
        application, run_after_load
    );
    if (loaded != jr800::runtime::LoadApplicationResult::loaded) {
        return load_status(loaded);
    }
    if (info != nullptr) {
        *info = {};
        info->kind = static_cast<std::uint32_t>(application.kind);
        info->name_length = static_cast<std::uint32_t>(application.name.size());
        std::copy(application.name.begin(), application.name.end(), info->name);
        info->byte_count = static_cast<std::uint32_t>(application.basic_data.size());
        for (const auto& segment : application.segments) info->byte_count += segment.logical_size;
    }
    machine.debugger.clear_history();
    machine.debugger.clear_debug_info();
    machine.debugger.clear_execution_breakpoints();
    machine.debugger.clear_memory_watchpoints();
    machine.debugger.clear_expression_watches();
    machine.debugger.clear_symbol_watches();
    return JR800_STATUS_OK;
}

void copy_native_program_wav_issue(
    const jr800::formats::NativeMsaveIssue& source,
    jr800_native_program_wav_issue& destination
) noexcept {
    destination = {
        static_cast<std::uint32_t>(source.code) + 1U,
        static_cast<std::uint32_t>(std::min<std::size_t>(
            source.burst_index,
            UINT32_MAX
        )),
    };
}

jr800_status debug_status(jr800::debugger::DebugInfoLoadResult result) noexcept {
    using jr800::debugger::DebugInfoLoadResult;
    switch (result) {
    case DebugInfoLoadResult::loaded:
        return JR800_STATUS_OK;
    case DebugInfoLoadResult::detached:
        return JR800_STATUS_DETACHED;
    case DebugInfoLoadResult::invalid_format:
        return JR800_STATUS_INVALID_DEBUG_INFO;
    case DebugInfoLoadResult::target_mismatch:
        return JR800_STATUS_TARGET_MISMATCH;
    case DebugInfoLoadResult::integrity_mismatch:
        return JR800_STATUS_INTEGRITY_MISMATCH;
    }
    return JR800_STATUS_INTERNAL_ERROR;
}

void copy_state(
    const jr800::core::CpuState& source,
    jr800::isa::CpuProfile profile,
    std::uint32_t calendar_alarm_terminal_state,
    std::uint32_t port2_timer_output_state,
    std::optional<std::uint64_t> lcd_substituted_read_count,
    std::optional<std::uint64_t> ignored_access_count,
    jr800_machine_state& destination
) noexcept {
    const auto substituted_read_count = lcd_substituted_read_count.value_or(0U);
    const auto ignored_count = ignored_access_count.value_or(0U);
    destination = {
        JR800_WASM_ABI_VERSION,
        profile_id(profile),
        source.pc,
        source.sp,
        source.x,
        source.a,
        source.b,
        source.condition_code,
        execution_state(source.execution_state),
        low_word(source.cycle_count),
        high_word(source.cycle_count),
        source.knowledge.registers,
        source.knowledge.condition_code,
        calendar_alarm_terminal_state,
        port2_timer_output_state,
        lcd_substituted_read_count.has_value() ? 1U : 0U,
        low_word(substituted_read_count),
        high_word(substituted_read_count),
        ignored_access_count.has_value() ? 1U : 0U,
        low_word(ignored_count),
        high_word(ignored_count),
    };
}

void copy_expression_watch_result(
    const jr800::debugger::ExpressionEvaluationResult& source,
    jr800_expression_watch_result& destination
) noexcept {
    destination = {
        low_word(source.value),
        high_word(source.value),
        expression_error(source.error),
        bus_fault(source.bus_fault),
        source.fault_address,
        state_part(source.state_fault),
    };
}

std::uint32_t symbol_binding(
    jr800::formats::jr8dbg::SymbolBinding binding
) noexcept {
    using Binding = jr800::formats::jr8dbg::SymbolBinding;
    switch (binding) {
    case Binding::local:
        return JR800_SYMBOL_LOCAL;
    case Binding::global:
        return JR800_SYMBOL_GLOBAL;
    }
    return JR800_SYMBOL_LOCAL;
}

std::uint32_t symbol_kind(jr800::formats::jr8dbg::SymbolKind kind) noexcept {
    using Kind = jr800::formats::jr8dbg::SymbolKind;
    switch (kind) {
    case Kind::address:
        return JR800_SYMBOL_ADDRESS;
    case Kind::absolute:
        return JR800_SYMBOL_ABSOLUTE;
    }
    return JR800_SYMBOL_ADDRESS;
}

void copy_symbol_watch_result(
    const jr800::debugger::SymbolWatchValue& source,
    jr800_symbol_watch_result& destination
) noexcept {
    destination = {
        source.value,
        symbol_binding(source.binding),
        symbol_kind(source.kind),
        source.size,
        source.source_file_index.has_value() ? 1U : 0U,
        source.source_file_index.value_or(0U),
    };
}

void copy_stop(
    const jr800::debugger::StopInfo& source,
    jr800_stop_info& destination
) noexcept {
    destination = {
        stop_reason(source.reason),
        cpu_fault(source.step.fault),
        source.trigger_address,
        source.trigger_access.has_value() ? 1U : 0U,
        source.trigger_access.has_value()
            ? access_kind(*source.trigger_access)
            : static_cast<std::uint32_t>(JR800_ACCESS_DATA_READ),
        low_word(source.instructions_executed),
        high_word(source.instructions_executed),
        source.step.pc_before,
        source.step.pc_after,
        source.step.bytes[0],
        source.step.bytes[1],
        source.step.bytes[2],
        source.step.instruction_length,
        source.step.bytes_fetched,
        source.step.cycles,
        bus_fault(source.step.bus_fault),
        access_kind(source.step.fault_access),
        state_part(source.step.state_fault),
        step_kind(source.step.kind),
        interrupt_source(source.step.interrupt_source),
        source.continuation_address.has_value() ? 1U : 0U,
        source.continuation_address.value_or(0U),
        expression_error(source.condition_error),
        source.condition_error
                == jr800::debugger::ExpressionEvaluationError::memory_access
            ? source.step.fault_address
            : 0U,
    };
}

void copy_history_entry(
    const jr800::debugger::ExecutionHistoryEntry& source,
    jr800_history_entry& destination
) noexcept {
    destination = {
        low_word(source.sequence),
        high_word(source.sequence),
        low_word(source.cycle_begin),
        high_word(source.cycle_begin),
        low_word(source.first_access_sequence),
        high_word(source.first_access_sequence),
        source.access_count,
        source.pc_before,
        source.pc_after,
        source.bytes[0],
        source.bytes[1],
        source.bytes[2],
        source.instruction_length,
        source.bytes_fetched,
        source.cycles,
        cpu_fault(source.fault),
        source.state_after.pc,
        source.state_after.sp,
        source.state_after.x,
        source.state_after.a,
        source.state_after.b,
        source.state_after.condition_code,
        execution_state(source.state_after.execution_state),
        low_word(source.state_after.cycle_count),
        high_word(source.state_after.cycle_count),
        bus_fault(source.bus_fault),
        source.fault_address,
        access_kind(source.fault_access),
        state_part(source.state_fault),
        step_kind(source.kind),
        interrupt_source(source.interrupt_source),
        source.state_after.knowledge.registers,
        source.state_after.knowledge.condition_code,
    };
}

void copy_access_record(
    const jr800::core::BusAccessEvent& source,
    jr800_access_record& destination
) noexcept {
    destination = {
        low_word(source.sequence),
        high_word(source.sequence),
        low_word(source.instruction_cycle),
        high_word(source.instruction_cycle),
        source.instruction_pc,
        source.address,
        source.value,
        source.value_known ? 1U : 0U,
        source.previous_value,
        source.previous_value_known ? 1U : 0U,
        access_kind(source.kind),
    };
}

bool valid_address(std::uint32_t address) noexcept {
    return address <= 0xFFFFU;
}

std::optional<jr800::debugger::AccessTraceFilter> access_trace_filter(
    const jr800_access_filter* filter
) noexcept {
    if (filter == nullptr || !valid_address(filter->first_address)
        || !valid_address(filter->last_address)
        || filter->kind_mask == 0U
        || (filter->kind_mask & ~JR800_ACCESS_TRACE_ALL) != 0U) {
        return std::nullopt;
    }
    const jr800::debugger::AccessTraceFilter result{
        static_cast<std::uint16_t>(filter->first_address),
        static_cast<std::uint16_t>(filter->last_address),
        static_cast<jr800::debugger::AccessTraceMask>(filter->kind_mask),
    };
    return jr800::debugger::is_valid_access_trace_filter(result)
        ? std::optional{result}
        : std::nullopt;
}

bool valid_range(std::uint32_t address, std::uint32_t size) noexcept {
    return address <= 65'536U && size <= 65'536U - address;
}

jr800_status copy_text(const std::string& source, char* text, std::uint32_t capacity) noexcept {
    const auto required = source.size() + 1U;
    if (text == nullptr || capacity < required) {
        return JR800_STATUS_BUFFER_TOO_SMALL;
    }
    std::memcpy(text, source.c_str(), required);
    return JR800_STATUS_OK;
}

}  // namespace

extern "C" {

std::uint32_t jr800_machine_abi_version() {
    return JR800_WASM_ABI_VERSION;
}

jr800_machine* jr800_machine_create() {
    try {
        return new jr800_machine{};
    } catch (const std::exception&) {
        return nullptr;
    }
}

jr800_machine* jr800_machine_create_jr800(
    const jr800_hardware_configuration* configuration
) {
    if (configuration == nullptr) {
        return nullptr;
    }
    const auto parsed = machine_configuration(*configuration);
    if (!parsed.has_value()) {
        return nullptr;
    }
    try {
        auto machine = std::make_unique<jr800_machine>(
            parsed->machine,
            parsed->reset_state
        );
        if (!apply_jr800_inputs(
                *machine->hardware_machine,
                *configuration
            )) {
            return nullptr;
        }
        machine->initial_hardware_state = machine->hardware_machine->clone();
        return machine.release();
    } catch (const std::exception&) {
        return nullptr;
    }
}

void jr800_machine_destroy(jr800_machine* machine) {
    delete machine;
}

jr800_status jr800_machine_load_application(
    jr800_machine* machine,
    const std::uint8_t* bytes,
    std::uint32_t byte_count,
    std::uint32_t initial_stack_pointer
) {
    if (machine == nullptr || bytes == nullptr || byte_count == 0U
        || !valid_address(initial_stack_pointer)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    try {
        auto application = jr800::formats::jr8app::read(
            std::span<const std::uint8_t>{bytes, byte_count}
        );
        auto validation_machine = std::make_unique<
            jr800::core::SyntheticMachine
        >();
        const auto validation = jr800::runtime::load_application(
            *validation_machine,
            application,
            static_cast<std::uint16_t>(initial_stack_pointer)
        );
        if (validation != jr800::runtime::LoadApplicationResult::loaded) {
            return load_status(validation);
        }
        const auto loaded = jr800::runtime::load_application(
            *machine->synthetic_machine,
            application,
            static_cast<std::uint16_t>(initial_stack_pointer)
        );
        if (loaded != jr800::runtime::LoadApplicationResult::loaded) {
            return load_status(loaded);
        }
        machine->application = std::move(application);
        machine->initial_stack_pointer = static_cast<std::uint16_t>(initial_stack_pointer);
        machine->debugger.clear_history();
        machine->debugger.clear_debug_info();
        machine->debugger.clear_execution_breakpoints();
        machine->debugger.clear_memory_watchpoints();
        machine->debugger.clear_expression_watches();
        return JR800_STATUS_OK;
    } catch (const jr800::formats::linked::Error&) {
        return JR800_STATUS_INVALID_APPLICATION;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_load_debug_info(
    jr800_machine* machine,
    const std::uint8_t* bytes,
    std::uint32_t byte_count
) {
    if (machine == nullptr || bytes == nullptr || byte_count == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->application.has_value()) {
        return JR800_STATUS_NO_APPLICATION;
    }
    try {
        const auto debug_info = jr800::formats::jr8dbg::read(
            std::span<const std::uint8_t>{bytes, byte_count}
        );
        return debug_status(machine->debugger.load_debug_info(
            debug_info,
            machine->application->integrity_sha256
        ));
    } catch (const jr800::formats::linked::Error&) {
        return JR800_STATUS_INVALID_DEBUG_INFO;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_load_logical_rom(
    jr800_machine* machine,
    const std::uint8_t* bytes,
    std::uint32_t byte_count
) {
    if (machine == nullptr || bytes == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (byte_count != jr800::core::jr800_logical_rom_size) {
        return JR800_STATUS_INVALID_LOGICAL_ROM;
    }
    try {
        auto candidate = machine->initial_hardware_state->clone();
        const auto loaded = candidate->load_logical_rom(
            std::span<const std::uint8_t>{bytes, byte_count}
        );
        if (loaded != jr800::core::Jr800MemoryStatus::ok) {
            return JR800_STATUS_INVALID_LOGICAL_ROM;
        }
        const auto reset =
            candidate->initialize_from_reset_entry();
        if (!reset.succeeded()) {
            return inspect_status(reset.fault);
        }
        machine->hardware_machine->copy_state_from(*candidate);
        machine->initial_hardware_state = std::move(candidate);
        machine->logical_rom_loaded = true;
        if (!machine->program_saves) {
            machine->program_saves = std::make_unique<jr800::runtime::ProgramSaveCapture>(*machine->hardware_machine);
        } else {
            machine->program_saves->reset();
        }
        machine->debugger.clear_history();
        machine->debugger.clear_debug_info();
        machine->debugger.clear_execution_breakpoints();
        machine->debugger.clear_memory_watchpoints();
        machine->debugger.clear_expression_watches();
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_load_jr8rom(
    jr800_machine* machine,
    const std::uint8_t* bytes,
    std::uint32_t byte_count
) {
    if (machine == nullptr || bytes == nullptr || byte_count == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (byte_count > jr800::formats::jr8rom::maximum_encoded_size) {
        return JR800_STATUS_INVALID_JR8ROM;
    }
    try {
        const auto image = jr800::formats::jr8rom::read(
            std::span<const std::uint8_t>{bytes, byte_count}
        );
        const auto logical_rom = jr800::formats::jr8rom::extract_range(
            image,
            jr800::core::jr800_logical_rom_base,
            jr800::core::jr800_logical_rom_size
        );
        if (!logical_rom.has_value()) {
            return JR800_STATUS_INCOMPLETE_JR8ROM;
        }
        return jr800_machine_load_logical_rom(
            machine,
            logical_rom->data(),
            static_cast<std::uint32_t>(logical_rom->size())
        );
    } catch (const jr800::formats::linked::Error& error) {
        return error.code() == jr800::formats::linked::ErrorCode::integrity_mismatch
            ? JR800_STATUS_INTEGRITY_MISMATCH
            : JR800_STATUS_INVALID_JR8ROM;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_load_program(
    jr800_machine* machine,
    const std::uint8_t* bytes,
    std::uint32_t byte_count,
    std::uint32_t run_after_load, jr800_program_info* info
) {
    if (machine == nullptr || bytes == nullptr || byte_count == 0U || run_after_load > 1U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->logical_rom_loaded) {
        return JR800_STATUS_NO_ROM;
    }
    try {
        const auto application = jr800::formats::jr8app::read(
            std::span<const std::uint8_t>{bytes, byte_count}
        );
        return load_jr800_application(*machine, application, run_after_load != 0U, info);
    } catch (const jr800::formats::linked::Error&) {
        return JR800_STATUS_INVALID_APPLICATION;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_load_native_program_wav(
    jr800_machine* machine,
    const std::uint8_t* bytes,
    std::uint32_t byte_count,
    jr800_native_program_wav_issue* issue,
    std::uint32_t run_after_load, jr800_program_info* info
) {
    if (machine == nullptr || bytes == nullptr || byte_count == 0U
        || issue == nullptr || run_after_load > 1U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->logical_rom_loaded) {
        return JR800_STATUS_NO_ROM;
    }
    *issue = {
        JR800_NATIVE_PROGRAM_WAV_ISSUE_NONE,
        0U,
    };
    try {
        const auto decoded = jr800::formats::decode_native_program_wav(
            std::span<const std::uint8_t>{bytes, byte_count}
        );
        if (!decoded.issues.empty()) {
            copy_native_program_wav_issue(decoded.issues.front(), *issue);
            return JR800_STATUS_INVALID_NATIVE_PROGRAM_WAV;
        }
        if (!decoded.file.has_value()) {
            issue->code = JR800_NATIVE_PROGRAM_WAV_ISSUE_INVALID_WAV;
            return JR800_STATUS_INVALID_NATIVE_PROGRAM_WAV;
        }

        const auto application = jr800::formats::native_program_application(*decoded.file);
        return load_jr800_application(*machine, application, run_after_load != 0U, info);
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_reset(jr800_machine* machine) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        if (machine->kind == MachineKind::jr800) {
            if (!machine->logical_rom_loaded) {
                return JR800_STATUS_NO_ROM;
            }
            // E-429: host session restart restores the configured boot state.
            // The core's physical reset-entry operation is intentionally separate.
            machine->hardware_machine->copy_state_from(*machine->initial_hardware_state);
            machine->debugger.clear_history();
            if (machine->program_saves) machine->program_saves->reset();
            return JR800_STATUS_OK;
        }
        if (!machine->application.has_value()) {
            return JR800_STATUS_NO_APPLICATION;
        }
        const auto result = jr800::runtime::load_application(
            *machine->synthetic_machine,
            *machine->application,
            machine->initial_stack_pointer
        );
        if (result == jr800::runtime::LoadApplicationResult::loaded) {
            machine->debugger.clear_history();
        }
        return load_status(result);
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_get_program_saves(const jr800_machine* machine,
    jr800_program_saves_state* state) {
    if (machine == nullptr || state == nullptr) return JR800_STATUS_INVALID_ARGUMENT;
    *state = {};
    if (machine->program_saves) {
        state->state = static_cast<std::uint32_t>(machine->program_saves->state());
        state->count = static_cast<std::uint32_t>(machine->program_saves->files().size());
    }
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_get_saved_program_info(const jr800_machine* machine,
    std::uint32_t index, jr800_program_info* info) {
    if (machine == nullptr || info == nullptr) return JR800_STATUS_INVALID_ARGUMENT;
    if (!machine->program_saves || index >= machine->program_saves->files().size()) return JR800_STATUS_NOT_FOUND;
    const auto& file = machine->program_saves->files()[index];
    *info = {};
    info->kind = static_cast<std::uint32_t>(file.kind);
    info->byte_count = static_cast<std::uint32_t>(file.payload.size());
    info->name_length = static_cast<std::uint32_t>(file.filename.size());
    std::copy(file.filename.begin(), file.filename.end(), info->name);
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_export_saved_program(const jr800_machine* machine,
    std::uint32_t index, std::uint32_t format, std::uint8_t* bytes,
    std::uint32_t capacity, std::uint32_t* byte_count) {
    if (machine == nullptr || byte_count == nullptr || (bytes == nullptr && capacity != 0U)
        || (format != 1U && format != 2U)) return JR800_STATUS_INVALID_ARGUMENT;
    if (!machine->program_saves || index >= machine->program_saves->files().size()) return JR800_STATUS_NOT_FOUND;
    try {
        const auto& file = machine->program_saves->files()[index];
        const auto output = format == 1U
            ? jr800::formats::jr8app::write(jr800::formats::native_program_application(file))
            : jr800::formats::encode_native_program_wav(file);
        *byte_count = static_cast<std::uint32_t>(output.size());
        if (bytes == nullptr) return JR800_STATUS_OK;
        if (capacity < output.size()) return JR800_STATUS_BUFFER_TOO_SMALL;
        std::copy(output.begin(), output.end(), bytes);
        return JR800_STATUS_OK;
    } catch (const std::exception&) { return JR800_STATUS_INTERNAL_ERROR; }
}

jr800_status jr800_machine_clear_program_saves(jr800_machine* machine) {
    if (machine == nullptr) return JR800_STATUS_INVALID_ARGUMENT;
    if (!machine->program_saves) return JR800_STATUS_OK;
    if (machine->program_saves->state() == jr800::runtime::ProgramSaveState::recording) return JR800_STATUS_INVALID_ARGUMENT;
    try { machine->program_saves->clear(); return JR800_STATUS_OK; }
    catch (const std::exception&) { return JR800_STATUS_INTERNAL_ERROR; }
}

jr800_status jr800_machine_get_state(
    const jr800_machine* machine,
    jr800_machine_state* state
) {
    if (machine == nullptr || state == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    copy_state(
        machine->execution().cpu().state(),
        machine->execution().cpu().profile(),
        calendar_alarm_terminal(*machine),
        port2_timer_output(*machine),
        lcd_substituted_data_read_count(*machine),
        ignored_io_access_count(*machine),
        *state
    );
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_step(jr800_machine* machine, jr800_stop_info* stop) {
    if (machine == nullptr || stop == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    try {
        copy_stop(machine->debugger.step(), *stop);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_step_over(
    jr800_machine* machine,
    std::uint32_t instruction_limit,
    jr800_stop_info* stop
) {
    if (machine == nullptr || instruction_limit == 0U || stop == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    try {
        copy_stop(machine->debugger.step_over(instruction_limit), *stop);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_step_out(
    jr800_machine* machine,
    std::uint32_t instruction_limit,
    jr800_step_out_state* state,
    jr800_stop_info* stop
) {
    if (machine == nullptr || instruction_limit == 0U || state == nullptr
        || stop == nullptr || !valid_boolean(state->continued)
        || (state->continued == 0U
            && (state->nesting_depth_low != 0U
                || state->nesting_depth_high != 0U))) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    try {
        const auto result = machine->debugger.step_out(
            instruction_limit,
            jr800::debugger::StepOutState{
                state->continued != 0U,
                combine_words(
                    state->nesting_depth_low,
                    state->nesting_depth_high
                ),
            }
        );
        copy_stop(result.stop, *stop);
        *state = {
            result.state.continued ? 1U : 0U,
            low_word(result.state.nesting_depth),
            high_word(result.state.nesting_depth),
        };
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_run(
    jr800_machine* machine,
    std::uint32_t instruction_limit,
    jr800_stop_info* stop
) {
    if (machine == nullptr || stop == nullptr || instruction_limit == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    try {
        copy_stop(machine->debugger.run(instruction_limit), *stop);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_run_to(
    jr800_machine* machine,
    std::uint32_t address,
    std::uint32_t instruction_limit,
    jr800_stop_info* stop
) {
    if (machine == nullptr || !valid_address(address)
        || instruction_limit == 0U || stop == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    try {
        copy_stop(
            machine->debugger.run_to(
                static_cast<std::uint16_t>(address),
                instruction_limit
            ),
            *stop
        );
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_advance_suspended_cycles(
    jr800_machine* machine,
    std::uint32_t cycle_limit,
    jr800_suspended_advance* result
) {
    if (machine == nullptr || result == nullptr || cycle_limit == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    try {
        const auto advance =
            machine->execution().advance_suspended_cycles(cycle_limit);
        *result = {
            advance.suspended ? 1U : 0U,
            advance.cycles_elapsed,
            advance.interrupt_request.known ? 1U : 0U,
            interrupt_source(advance.interrupt_request.source),
            bus_fault(advance.bus_fault),
        };
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_advance_calendar_oscillator_ticks(
    jr800_machine* machine,
    std::uint32_t ticks
) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    return calendar_operation_status(
        machine->hardware_machine->advance_calendar_oscillator_ticks(ticks)
    );
}

jr800_status jr800_machine_adjust_calendar_seconds(jr800_machine* machine) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    return calendar_operation_status(
        machine->hardware_machine->adjust_calendar_seconds()
    );
}

jr800_status jr800_machine_set_calendar_datetime(
    jr800_machine* machine,
    const jr800_calendar_datetime* value
) {
    if (machine == nullptr || value == nullptr
        || value->year > 2099U || value->month > 12U || value->day > 31U
        || value->hour > 23U || value->minute > 59U || value->second > 59U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const jr800::core::CalendarDateTime datetime{
        static_cast<std::uint16_t>(value->year),
        static_cast<std::uint8_t>(value->month),
        static_cast<std::uint8_t>(value->day),
        static_cast<std::uint8_t>(value->hour),
        static_cast<std::uint8_t>(value->minute),
        static_cast<std::uint8_t>(value->second),
    };
    if (!datetime.valid()) return JR800_STATUS_INVALID_ARGUMENT;
    return calendar_operation_status(
        machine->hardware_machine->set_calendar_datetime(datetime)
    );
}

jr800_status jr800_machine_set_keyboard_bus_response(
    jr800_machine* machine,
    std::uint32_t address,
    std::uint32_t value,
    std::uint32_t known
) {
    if (machine == nullptr || !valid_address(address) || !valid_byte(value)
        || !valid_boolean(known) || (known == 0U && value != 0U)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    return machine->hardware_machine->set_keyboard_bus_response(
        static_cast<std::uint16_t>(address),
        static_cast<std::uint8_t>(value),
        known != 0U
    ) ? JR800_STATUS_OK : JR800_STATUS_INVALID_ARGUMENT;
}

jr800_status jr800_machine_set_keyboard_key_state(
    jr800_machine* machine,
    std::uint32_t key,
    std::uint32_t pressed
) {
    if (machine == nullptr || !valid_boolean(pressed)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    const auto decoded_key = keyboard_key(key);
    if (!decoded_key.has_value()) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    return machine->hardware_machine->set_keyboard_key_state(
        *decoded_key,
        pressed != 0U
    ) ? JR800_STATUS_OK : JR800_STATUS_INVALID_ARGUMENT;
}

jr800_status jr800_machine_get_keyboard_activity(
    const jr800_machine* machine,
    jr800_keyboard_activity* activity
) {
    if (machine == nullptr || activity == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->logical_rom_loaded) {
        return JR800_STATUS_NO_ROM;
    }
    const auto source = machine->hardware_machine->keyboard_activity();
    *activity = {
        low_word(source.read_attempts),
        high_word(source.read_attempts),
        low_word(source.distinct_addresses),
        high_word(source.distinct_addresses),
    };
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_clear_keyboard_activity(
    jr800_machine* machine
) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->logical_rom_loaded) {
        return JR800_STATUS_NO_ROM;
    }
    machine->hardware_machine->clear_keyboard_activity();
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_set_execution_breakpoint(
    jr800_machine* machine,
    std::uint32_t address,
    std::uint32_t enabled
) {
    if (machine == nullptr || !valid_address(address) || enabled > 1U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    machine->debugger.set_execution_breakpoint(
        static_cast<std::uint16_t>(address),
        enabled != 0U
    );
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_set_conditional_execution_breakpoint(
    jr800_machine* machine,
    std::uint32_t address,
    const char* condition,
    std::uint32_t condition_size
) {
    if (machine == nullptr || !valid_address(address) || condition == nullptr
        || condition_size == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto result =
            machine->debugger.set_conditional_execution_breakpoint(
                static_cast<std::uint16_t>(address),
                std::string_view{condition, condition_size}
            );
        return result.succeeded()
            ? JR800_STATUS_OK
            : JR800_STATUS_INVALID_EXPRESSION;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_set_expression_watch(
    jr800_machine* machine,
    std::uint32_t watch_id,
    const char* expression,
    std::uint32_t expression_size
) {
    if (machine == nullptr || expression == nullptr || expression_size == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto diagnostic = machine->debugger.set_expression_watch(
            watch_id,
            std::string_view{expression, expression_size}
        );
        return diagnostic.succeeded()
            ? JR800_STATUS_OK
            : JR800_STATUS_INVALID_EXPRESSION;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_clear_expression_watch(
    jr800_machine* machine,
    std::uint32_t watch_id
) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    return machine->debugger.clear_expression_watch(watch_id)
        ? JR800_STATUS_OK
        : JR800_STATUS_NOT_FOUND;
}

jr800_status jr800_machine_evaluate_expression_watch(
    const jr800_machine* machine,
    std::uint32_t watch_id,
    jr800_expression_watch_result* result
) {
    if (machine == nullptr || result == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->ready()) {
        return machine->not_ready_status();
    }
    const auto evaluation =
        machine->debugger.evaluate_expression_watch(watch_id);
    if (!evaluation.has_value()) {
        return JR800_STATUS_NOT_FOUND;
    }
    copy_expression_watch_result(*evaluation, *result);
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_set_symbol_watch(
    jr800_machine* machine,
    std::uint32_t watch_id,
    const char* symbol_name,
    std::uint32_t symbol_name_size
) {
    if (machine == nullptr || symbol_name == nullptr
        || symbol_name_size == 0U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const std::string_view name{symbol_name, symbol_name_size};
    if (name.find('\0') != std::string_view::npos) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto registration = machine->debugger.set_symbol_watch(
            watch_id,
            name
        );
        switch (registration.status) {
        case jr800::debugger::SymbolWatchRegistrationStatus::registered:
            return JR800_STATUS_OK;
        case jr800::debugger::SymbolWatchRegistrationStatus::not_found:
            return JR800_STATUS_NOT_FOUND;
        case jr800::debugger::SymbolWatchRegistrationStatus::ambiguous:
            return JR800_STATUS_AMBIGUOUS_SYMBOL;
        }
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
    return JR800_STATUS_INTERNAL_ERROR;
}

jr800_status jr800_machine_clear_symbol_watch(
    jr800_machine* machine,
    std::uint32_t watch_id
) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    return machine->debugger.clear_symbol_watch(watch_id)
        ? JR800_STATUS_OK
        : JR800_STATUS_NOT_FOUND;
}

jr800_status jr800_machine_evaluate_symbol_watch(
    const jr800_machine* machine,
    std::uint32_t watch_id,
    jr800_symbol_watch_result* result
) {
    if (machine == nullptr || result == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const auto evaluation = machine->debugger.evaluate_symbol_watch(watch_id);
    if (!evaluation.has_value()) {
        return JR800_STATUS_NOT_FOUND;
    }
    copy_symbol_watch_result(*evaluation, *result);
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_set_memory_watchpoint(
    jr800_machine* machine,
    std::uint32_t address,
    std::uint32_t mode,
    std::uint32_t enabled
) {
    if (machine == nullptr || !valid_address(address)
        || mode < JR800_WATCHPOINT_READ || mode > JR800_WATCHPOINT_ACCESS
        || enabled > 1U) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    machine->debugger.set_memory_watchpoint(
        static_cast<std::uint16_t>(address),
        static_cast<jr800::debugger::MemoryWatchpointMode>(mode),
        enabled != 0U
    );
    return JR800_STATUS_OK;
}

std::uint32_t jr800_machine_history_count(const jr800_machine* machine) {
    if (machine == nullptr) {
        return 0U;
    }
    return static_cast<std::uint32_t>(machine->debugger.history_size());
}

std::uint32_t jr800_machine_copy_history(
    const jr800_machine* machine,
    jr800_history_entry* entries,
    std::uint32_t capacity
) {
    if (machine == nullptr || entries == nullptr || capacity == 0U) {
        return 0U;
    }
    try {
        const auto history = machine->debugger.history();
        const auto count = std::min<std::size_t>(history.size(), capacity);
        const auto offset = history.size() - count;
        for (std::size_t index = 0; index < count; ++index) {
            copy_history_entry(history[offset + index], entries[index]);
        }
        return static_cast<std::uint32_t>(count);
    } catch (const std::exception&) {
        return 0U;
    }
}

jr800_status jr800_machine_access_count(
    const jr800_machine* machine,
    const jr800_access_filter* filter,
    std::uint32_t* count
) {
    const auto parsed_filter = access_trace_filter(filter);
    if (machine == nullptr || !parsed_filter.has_value() || count == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto result = machine->debugger.memory_access_size(*parsed_filter);
        *count = static_cast<std::uint32_t>(result);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_copy_accesses(
    const jr800_machine* machine,
    const jr800_access_filter* filter,
    jr800_access_record* records,
    std::uint32_t capacity,
    std::uint32_t* copied
) {
    const auto parsed_filter = access_trace_filter(filter);
    if (machine == nullptr || !parsed_filter.has_value() || records == nullptr
        || capacity == 0U || copied == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto accesses = machine->debugger.memory_accesses(*parsed_filter);
        const auto count = std::min<std::size_t>(accesses.size(), capacity);
        const auto offset = accesses.size() - count;
        for (std::size_t index = 0; index < count; ++index) {
            copy_access_record(accesses[offset + index], records[index]);
        }
        *copied = static_cast<std::uint32_t>(count);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

void jr800_machine_clear_history(jr800_machine* machine) {
    if (machine != nullptr) {
        machine->debugger.clear_history();
    }
}

jr800_status jr800_machine_read_memory(
    const jr800_machine* machine,
    std::uint32_t address,
    std::uint8_t* bytes,
    std::uint32_t byte_count
) {
    if (machine == nullptr || bytes == nullptr || byte_count == 0U
        || !valid_range(address, byte_count)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::uint8_t> inspected(byte_count);
        for (std::uint32_t index = 0; index < byte_count; ++index) {
            const auto result = machine->execution().inspect8(
                static_cast<std::uint16_t>(address + index)
            );
            if (!result.succeeded()) {
                return inspect_status(result.fault);
            }
            inspected[index] = *result.value;
        }
        std::copy(inspected.begin(), inspected.end(), bytes);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_copy_lcd_panel(
    const jr800_machine* machine,
    std::uint8_t* dots,
    std::uint32_t capacity
) {
    if (machine == nullptr || dots == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (capacity < JR800_LCD_PANEL_DOT_COUNT) {
        return JR800_STATUS_BUFFER_TOO_SMALL;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->logical_rom_loaded) {
        return JR800_STATUS_NO_ROM;
    }
    if (!machine->hardware_machine->inspect_lcd_controller(0U).has_value()) {
        return JR800_STATUS_UNSUPPORTED_ACCESS;
    }

    try {
        std::vector<std::uint8_t> panel(JR800_LCD_PANEL_DOT_COUNT);
        for (std::size_t row = 0U; row < JR800_LCD_PANEL_HEIGHT; ++row) {
            for (std::size_t column = 0U;
                 column < JR800_LCD_PANEL_WIDTH;
                 ++column) {
                const auto dot = machine->hardware_machine->lcd_panel_dot(
                    column,
                    row
                );
                auto encoded = JR800_LCD_DOT_UNKNOWN;
                if (dot.has_value()) {
                    encoded = *dot ? JR800_LCD_DOT_ON : JR800_LCD_DOT_OFF;
                }
                panel[row * JR800_LCD_PANEL_WIDTH + column] =
                    static_cast<std::uint8_t>(encoded);
            }
        }
        std::copy(panel.begin(), panel.end(), dots);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_copy_lcd_indicators(
    const jr800_machine* machine,
    jr800_lcd_indicator_raw* indicators,
    std::uint32_t capacity
) {
    if (machine == nullptr || indicators == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (capacity < JR800_LCD_INDICATOR_COUNT) {
        return JR800_STATUS_BUFFER_TOO_SMALL;
    }
    if (machine->kind != MachineKind::jr800) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    if (!machine->logical_rom_loaded) {
        return JR800_STATUS_NO_ROM;
    }
    if (!machine->hardware_machine->inspect_lcd_controller(0U).has_value()) {
        return JR800_STATUS_UNSUPPORTED_ACCESS;
    }

    try {
        std::vector<jr800_lcd_indicator_raw> values(
            JR800_LCD_INDICATOR_COUNT
        );
        for (std::uint32_t index = 0U;
             index < JR800_LCD_INDICATOR_COUNT;
             ++index) {
            const auto value = machine->hardware_machine
                ->lcd_indicator_ram_value(
                    lcd_indicators[index]
                );
            values[index] = {
                value.has_value() ? 1U : 0U,
                value.value_or(0U),
            };
        }
        std::copy(values.begin(), values.end(), indicators);
        return JR800_STATUS_OK;
    } catch (const std::exception&) {
        return JR800_STATUS_INTERNAL_ERROR;
    }
}

jr800_status jr800_machine_source_at(
    const jr800_machine* machine,
    std::uint32_t address,
    jr800_source_location* location
) {
    if (machine == nullptr || location == nullptr || !valid_address(address)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const auto* source = machine->debugger.source_at(static_cast<std::uint16_t>(address));
    if (source == nullptr) {
        return JR800_STATUS_NOT_FOUND;
    }
    *location = {
        source->address,
        source->length,
        source->source_file_index,
        source->line,
        source->column,
    };
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_source_address(
    const jr800_machine* machine,
    const char* source_path,
    std::uint32_t source_path_size,
    std::uint32_t line,
    std::uint32_t* address
) {
    if (machine == nullptr || source_path == nullptr || source_path_size == 0U
        || line == 0U || address == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const std::string_view path{source_path, source_path_size};
    if (path.find('\0') != std::string_view::npos) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    const auto resolved = machine->debugger.source_address(path, line);
    if (!resolved.has_value()) {
        return JR800_STATUS_NOT_FOUND;
    }
    *address = *resolved;
    return JR800_STATUS_OK;
}

jr800_status jr800_machine_symbol_address(
    const jr800_machine* machine,
    const char* symbol_name,
    std::uint32_t symbol_name_size,
    std::uint32_t* address
) {
    if (machine == nullptr || symbol_name == nullptr || symbol_name_size == 0U
        || address == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const std::string_view name{symbol_name, symbol_name_size};
    if (name.find('\0') != std::string_view::npos) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    const auto resolved = machine->debugger.symbol_address(name);
    switch (resolved.status) {
    case jr800::debugger::SymbolAddressStatus::found:
        *address = resolved.address;
        return JR800_STATUS_OK;
    case jr800::debugger::SymbolAddressStatus::not_found:
        return JR800_STATUS_NOT_FOUND;
    case jr800::debugger::SymbolAddressStatus::ambiguous:
        return JR800_STATUS_AMBIGUOUS_SYMBOL;
    case jr800::debugger::SymbolAddressStatus::not_address:
        return JR800_STATUS_SYMBOL_NOT_ADDRESS;
    }
    return JR800_STATUS_INTERNAL_ERROR;
}

std::uint32_t jr800_machine_source_path_size(
    const jr800_machine* machine,
    std::uint32_t source_file_index
) {
    if (machine == nullptr) {
        return 0U;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return 0U;
    }
    const auto* source = machine->debugger.source_file(source_file_index);
    if (source == nullptr || source->path.size() >= 0xFFFF'FFFFULL) {
        return 0U;
    }
    return static_cast<std::uint32_t>(source->path.size() + 1U);
}

jr800_status jr800_machine_copy_source_path(
    const jr800_machine* machine,
    std::uint32_t source_file_index,
    char* text,
    std::uint32_t capacity
) {
    if (machine == nullptr) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    if (machine->kind != MachineKind::synthetic_application) {
        return JR800_STATUS_WRONG_MACHINE_KIND;
    }
    const auto* source = machine->debugger.source_file(source_file_index);
    if (source == nullptr) {
        return JR800_STATUS_NOT_FOUND;
    }
    return copy_text(source->path, text, capacity);
}

jr800_status jr800_machine_disassemble(
    const jr800_machine* machine,
    std::uint32_t address,
    jr800_disassembly* disassembly
) {
    if (machine == nullptr || disassembly == nullptr || !valid_address(address)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    const auto result = machine->debugger.disassemble(static_cast<std::uint16_t>(address));
    if (!result.has_value()) {
        return JR800_STATUS_DETACHED;
    }
    *disassembly = {
        result->address,
        result->bytes[0],
        result->bytes[1],
        result->bytes[2],
        result->length,
        result->supported ? 1U : 0U,
    };
    return JR800_STATUS_OK;
}

std::uint32_t jr800_machine_disassembly_text_size(
    const jr800_machine* machine,
    std::uint32_t address
) {
    if (machine == nullptr || !valid_address(address)) {
        return 0U;
    }
    const auto result = machine->debugger.disassemble(static_cast<std::uint16_t>(address));
    if (!result.has_value() || result->text.size() >= 0xFFFF'FFFFULL) {
        return 0U;
    }
    return static_cast<std::uint32_t>(result->text.size() + 1U);
}

jr800_status jr800_machine_copy_disassembly_text(
    const jr800_machine* machine,
    std::uint32_t address,
    char* text,
    std::uint32_t capacity
) {
    if (machine == nullptr || !valid_address(address)) {
        return JR800_STATUS_INVALID_ARGUMENT;
    }
    const auto result = machine->debugger.disassemble(static_cast<std::uint16_t>(address));
    if (!result.has_value()) {
        return JR800_STATUS_DETACHED;
    }
    return copy_text(result->text, text, capacity);
}

}  // extern "C"
