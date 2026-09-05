// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#define JR800_NODISCARD [[nodiscard]]
#else
#define JR800_NODISCARD
#endif

#define JR800_WASM_ABI_VERSION 37U
#define JR800_HARDWARE_CONFIGURATION_WORD_COUNT 32U
#define JR800_STATE_WORD_COUNT 21U
#define JR800_STOP_WORD_COUNT 24U
#define JR800_HISTORY_WORD_COUNT 33U
#define JR800_ACCESS_WORD_COUNT 11U
#define JR800_ACCESS_FILTER_WORD_COUNT 3U
#define JR800_SOURCE_WORD_COUNT 5U
#define JR800_DISASSEMBLY_WORD_COUNT 6U
#define JR800_SUSPENDED_ADVANCE_WORD_COUNT 5U
#define JR800_STEP_OUT_STATE_WORD_COUNT 3U
#define JR800_EXPRESSION_WATCH_WORD_COUNT 6U
#define JR800_SYMBOL_WATCH_WORD_COUNT 6U
#define JR800_KEYBOARD_ACTIVITY_WORD_COUNT 4U
#define JR800_LCD_INDICATOR_COUNT 16U
#define JR800_LCD_INDICATOR_RAW_WORD_COUNT 2U
#define JR800_NATIVE_PROGRAM_WAV_ISSUE_WORD_COUNT 2U
#define JR800_LCD_PANEL_WIDTH 192U
#define JR800_LCD_PANEL_HEIGHT 64U
#define JR800_LCD_PANEL_DOT_COUNT \
    (JR800_LCD_PANEL_WIDTH * JR800_LCD_PANEL_HEIGHT)

typedef struct jr800_machine jr800_machine;

typedef enum jr800_status {
    JR800_STATUS_OK = 0,
    JR800_STATUS_INVALID_ARGUMENT = 1,
    JR800_STATUS_NO_APPLICATION = 2,
    JR800_STATUS_INVALID_APPLICATION = 3,
    JR800_STATUS_UNKNOWN_PROFILE = 4,
    JR800_STATUS_UNREVIEWED_PROFILE = 5,
    JR800_STATUS_SEGMENT_OUT_OF_RANGE = 6,
    JR800_STATUS_INVALID_DEBUG_INFO = 7,
    JR800_STATUS_TARGET_MISMATCH = 8,
    JR800_STATUS_INTEGRITY_MISMATCH = 9,
    JR800_STATUS_DETACHED = 10,
    JR800_STATUS_BUFFER_TOO_SMALL = 11,
    JR800_STATUS_NOT_FOUND = 12,
    JR800_STATUS_INTERNAL_ERROR = 13,
    JR800_STATUS_NO_ROM = 14,
    JR800_STATUS_WRONG_MACHINE_KIND = 15,
    JR800_STATUS_INVALID_LOGICAL_ROM = 16,
    JR800_STATUS_BACKING_STORE_UNAVAILABLE = 17,
    JR800_STATUS_UNINITIALIZED_READ = 18,
    JR800_STATUS_UNSUPPORTED_ACCESS = 19,
    JR800_STATUS_INVALID_EXPRESSION = 20,
    JR800_STATUS_AMBIGUOUS_SYMBOL = 21,
    JR800_STATUS_SYMBOL_NOT_ADDRESS = 22,
    JR800_STATUS_INVALID_JR8ROM = 23,
    JR800_STATUS_INCOMPLETE_JR8ROM = 24,
    JR800_STATUS_INVALID_NATIVE_PROGRAM_WAV = 25
} jr800_status;

typedef enum jr800_native_program_wav_issue_code {
    JR800_NATIVE_PROGRAM_WAV_ISSUE_NONE = 0,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_INVALID_WAV = 1,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_UNSUPPORTED_WAV = 2,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_NO_SIGNAL = 3,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_UNEXPECTED_BURST_COUNT = 4,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_SYNCHRONIZATION_FAILED = 5,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_TRUNCATED_BLOCK = 6,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_FRAMING_ERROR = 7,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_CHECKSUM_MISMATCH = 8,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_UNSUPPORTED_HEADER = 9,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_INVALID_LENGTH = 10,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_INVALID_PROGRAM_RANGE = 11,
    JR800_NATIVE_PROGRAM_WAV_ISSUE_AMBIGUOUS_HEADER_BYTE_ORDER = 12
} jr800_native_program_wav_issue_code;

typedef struct jr800_native_program_wav_issue {
    uint32_t code;
    uint32_t burst_index;
} jr800_native_program_wav_issue;

typedef enum jr800_profile_id {
    JR800_PROFILE_UNRESOLVED = 0,
    JR800_PROFILE_MC6801 = 1,
    JR800_PROFILE_HD6301V1 = 2
} jr800_profile_id;

typedef enum jr800_stop_reason {
    JR800_STOP_STEP_COMPLETE = 0,
    JR800_STOP_INSTRUCTION_LIMIT = 1,
    JR800_STOP_EXECUTION_BREAKPOINT = 2,
    JR800_STOP_MEMORY_WATCHPOINT = 3,
    JR800_STOP_CPU_FAULT = 4,
    JR800_STOP_DETACHED = 5,
    JR800_STOP_SLEEPING = 6,
    JR800_STOP_ADDRESS_REACHED = 7,
    JR800_STOP_STEP_OUT_COMPLETE = 8,
    JR800_STOP_BREAKPOINT_CONDITION_ERROR = 9
} jr800_stop_reason;

typedef enum jr800_expression_evaluation_error {
    JR800_EXPRESSION_OK = 0,
    JR800_EXPRESSION_UNKNOWN_STATE = 1,
    JR800_EXPRESSION_MEMORY_ACCESS = 2,
    JR800_EXPRESSION_DIVISION_BY_ZERO = 3,
    JR800_EXPRESSION_INVALID_SHIFT = 4,
    JR800_EXPRESSION_ADDRESS_OUT_OF_RANGE = 5,
    JR800_EXPRESSION_SYMBOL_NOT_FOUND = 6,
    JR800_EXPRESSION_AMBIGUOUS_SYMBOL = 7
} jr800_expression_evaluation_error;

typedef enum jr800_symbol_binding {
    JR800_SYMBOL_LOCAL = 1,
    JR800_SYMBOL_GLOBAL = 2
} jr800_symbol_binding;

typedef enum jr800_symbol_kind {
    JR800_SYMBOL_ADDRESS = 1,
    JR800_SYMBOL_ABSOLUTE = 2
} jr800_symbol_kind;

typedef enum jr800_cpu_execution_state {
    JR800_CPU_ACTIVE = 0,
    JR800_CPU_SLEEPING = 1,
    JR800_CPU_WAITING_FOR_INTERRUPT = 2
} jr800_cpu_execution_state;

typedef enum jr800_cpu_fault {
    JR800_FAULT_NONE = 0,
    JR800_FAULT_UNSUPPORTED_OPCODE = 1,
    JR800_FAULT_UNIMPLEMENTED_OPERATION = 2,
    JR800_FAULT_BUS_ACCESS = 3,
    JR800_FAULT_UNKNOWN_STATE = 4,
    JR800_FAULT_UNKNOWN_INTERRUPT_REQUEST = 5,
    JR800_FAULT_BUS_ADVANCE = 6
} jr800_cpu_fault;

typedef enum jr800_access_kind {
    JR800_ACCESS_INSTRUCTION_FETCH = 0,
    JR800_ACCESS_DATA_READ = 1,
    JR800_ACCESS_DATA_WRITE = 2
} jr800_access_kind;

typedef enum jr800_access_trace_mask {
    JR800_ACCESS_TRACE_INSTRUCTION_FETCH = 0x01,
    JR800_ACCESS_TRACE_DATA_READ = 0x02,
    JR800_ACCESS_TRACE_DATA_WRITE = 0x04,
    JR800_ACCESS_TRACE_DATA = 0x06,
    JR800_ACCESS_TRACE_ALL = 0x07
} jr800_access_trace_mask;

typedef enum jr800_memory_watchpoint_mode {
    JR800_WATCHPOINT_READ = 1,
    JR800_WATCHPOINT_WRITE = 2,
    JR800_WATCHPOINT_ACCESS = 3
} jr800_memory_watchpoint_mode;

typedef enum jr800_bus_fault {
    JR800_BUS_FAULT_NONE = 0,
    JR800_BUS_FAULT_BACKING_STORE_UNAVAILABLE = 1,
    JR800_BUS_FAULT_UNINITIALIZED_READ = 2,
    JR800_BUS_FAULT_UNSUPPORTED_ACCESS = 3,
    JR800_BUS_FAULT_READ_ONLY_WRITE = 4,
    JR800_BUS_FAULT_DEVICE_STATE_UNKNOWN = 5,
    JR800_BUS_FAULT_DEVICE_STATE_UNSUPPORTED = 6
} jr800_bus_fault;

typedef enum jr800_cpu_state_part {
    JR800_STATE_PART_NONE = 0,
    JR800_STATE_PART_PROGRAM_COUNTER = 1,
    JR800_STATE_PART_STACK_POINTER = 2,
    JR800_STATE_PART_INDEX_REGISTER = 3,
    JR800_STATE_PART_ACCUMULATOR_A = 4,
    JR800_STATE_PART_ACCUMULATOR_B = 5,
    JR800_STATE_PART_CONDITION_CODE = 6
} jr800_cpu_state_part;

typedef enum jr800_step_kind {
    JR800_STEP_DORMANT = 0,
    JR800_STEP_INSTRUCTION = 1,
    JR800_STEP_INTERRUPT_ENTRY = 2,
    JR800_STEP_SLEEP_RESUME = 3
} jr800_step_kind;

typedef enum jr800_interrupt_source {
    JR800_INTERRUPT_NONE = 0,
    JR800_INTERRUPT_TIMER_INPUT_CAPTURE = 1,
    JR800_INTERRUPT_TIMER_OUTPUT_COMPARE = 2,
    JR800_INTERRUPT_TIMER_OVERFLOW = 3,
    JR800_INTERRUPT_SERIAL = 4
} jr800_interrupt_source;

typedef enum jr800_cpu_register_mask {
    JR800_REGISTER_PC = 0x01,
    JR800_REGISTER_SP = 0x02,
    JR800_REGISTER_X = 0x04,
    JR800_REGISTER_A = 0x08,
    JR800_REGISTER_B = 0x10
} jr800_cpu_register_mask;

typedef enum jr800_calendar_address_source {
    JR800_CALENDAR_CPU_A0_TO_A3 = 0,
    JR800_CALENDAR_CPU_A1_TO_A4 = 1,
    JR800_CALENDAR_CPU_A2_TO_A5 = 2,
    JR800_CALENDAR_CPU_A3_TO_A6 = 3,
    JR800_CALENDAR_CPU_A4_TO_A7 = 4,
    JR800_CALENDAR_CPU_A5_TO_A8 = 5
} jr800_calendar_address_source;

typedef enum jr800_calendar_upper_read_bits {
    JR800_CALENDAR_UPPER_ZERO = 0x00,
    JR800_CALENDAR_UPPER_ONE = 0xF0
} jr800_calendar_upper_read_bits;

typedef enum jr800_calendar_cpu_cycle_ratio {
    JR800_CALENDAR_CPU_CYCLE_RATIO_EXPLICIT_TICKS_ONLY = 0,
    JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ = 1
} jr800_calendar_cpu_cycle_ratio;

typedef enum jr800_calendar_alarm_terminal_state {
    JR800_CALENDAR_ALARM_TERMINAL_DISCONNECTED = 0,
    JR800_CALENDAR_ALARM_TERMINAL_UNKNOWN = 1,
    JR800_CALENDAR_ALARM_TERMINAL_RELEASED = 2,
    JR800_CALENDAR_ALARM_TERMINAL_PULL_LOW = 3
} jr800_calendar_alarm_terminal_state;

typedef enum jr800_port2_timer_output_state {
    JR800_PORT2_TIMER_OUTPUT_UNAVAILABLE = 0,
    JR800_PORT2_TIMER_OUTPUT_DISABLED = 1,
    JR800_PORT2_TIMER_OUTPUT_UNKNOWN = 2,
    JR800_PORT2_TIMER_OUTPUT_LOW = 3,
    JR800_PORT2_TIMER_OUTPUT_HIGH = 4
} jr800_port2_timer_output_state;

typedef enum jr800_lcd_dot_state {
    JR800_LCD_DOT_UNKNOWN = 0,
    JR800_LCD_DOT_OFF = 1,
    JR800_LCD_DOT_ON = 2
} jr800_lcd_dot_state;

typedef enum jr800_lcd_indicator {
    JR800_LCD_INDICATOR_PAGE_1 = 0,
    JR800_LCD_INDICATOR_PAGE_2 = 1,
    JR800_LCD_INDICATOR_PAGE_3 = 2,
    JR800_LCD_INDICATOR_PAGE_4 = 3,
    JR800_LCD_INDICATOR_PAGE_5 = 4,
    JR800_LCD_INDICATOR_PAGE_6 = 5,
    JR800_LCD_INDICATOR_PAGE_7 = 6,
    JR800_LCD_INDICATOR_PAGE_8 = 7,
    JR800_LCD_INDICATOR_CAPITAL_LOCK = 8,
    JR800_LCD_INDICATOR_GRAPHICS_INPUT = 9,
    JR800_LCD_INDICATOR_KANA_INPUT = 10,
    JR800_LCD_INDICATOR_INSERT_MODE = 11,
    JR800_LCD_INDICATOR_CONTROL_MODE = 12,
    JR800_LCD_INDICATOR_RADIAN_MODE = 13,
    JR800_LCD_INDICATOR_DEGREE_MODE = 14,
    JR800_LCD_INDICATOR_BATTERY_WARNING = 15
} jr800_lcd_indicator;

typedef enum jr800_key {
    JR800_KEY_SHIFT = 0,
    JR800_KEY_CONTROL = 1,
    JR800_KEY_MENU = 2,
    JR800_KEY_RETURN = 3,
    JR800_KEY_SPACE = 4,
    JR800_KEY_MAIN_1 = 5,
    JR800_KEY_LETTER_A = 6,
    JR800_KEY_LETTER_X = 7,
    JR800_KEY_KEYPAD_INSERT_RUB = 8,
    JR800_KEY_KEYPAD_VERTICAL_ARROWS = 9,
    JR800_KEY_KEYPAD_HORIZONTAL_ARROWS = 10,
    JR800_KEY_KEYPAD_0 = 11,
    JR800_KEY_KEYPAD_1 = 12,
    JR800_KEY_KEYPAD_2 = 13,
    JR800_KEY_KEYPAD_3 = 14,
    JR800_KEY_KEYPAD_4 = 15,
    JR800_KEY_KEYPAD_5 = 16,
    JR800_KEY_KEYPAD_6 = 17,
    JR800_KEY_KEYPAD_7 = 18,
    JR800_KEY_BREAK = 19,
    JR800_KEY_HOME_CLS = 20,
    JR800_KEY_MAIN_0 = 21,
    JR800_KEY_MAIN_2 = 22,
    JR800_KEY_MAIN_3 = 23,
    JR800_KEY_MAIN_4 = 24,
    JR800_KEY_MAIN_5 = 25,
    JR800_KEY_MAIN_6 = 26,
    JR800_KEY_MAIN_7 = 27,
    JR800_KEY_MAIN_8 = 28,
    JR800_KEY_MAIN_9 = 29,
    JR800_KEY_MAIN_CARET = 30,
    JR800_KEY_LETTER_B = 31,
    JR800_KEY_LETTER_C = 32,
    JR800_KEY_LETTER_D = 33,
    JR800_KEY_LETTER_E = 34,
    JR800_KEY_LETTER_F = 35,
    JR800_KEY_LETTER_G = 36,
    JR800_KEY_LETTER_H = 37,
    JR800_KEY_LETTER_I = 38,
    JR800_KEY_LETTER_J = 39,
    JR800_KEY_LETTER_K = 40,
    JR800_KEY_LETTER_L = 41,
    JR800_KEY_LETTER_M = 42,
    JR800_KEY_LETTER_N = 43,
    JR800_KEY_LETTER_O = 44,
    JR800_KEY_LETTER_P = 45,
    JR800_KEY_LETTER_Q = 46,
    JR800_KEY_LETTER_R = 47,
    JR800_KEY_LETTER_S = 48,
    JR800_KEY_LETTER_T = 49,
    JR800_KEY_LETTER_U = 50,
    JR800_KEY_LETTER_V = 51,
    JR800_KEY_LETTER_W = 52,
    JR800_KEY_LETTER_Y = 53,
    JR800_KEY_LETTER_Z = 54,
    JR800_KEY_COLON = 55,
    JR800_KEY_SEMICOLON = 56,
    JR800_KEY_COMMA = 57,
    JR800_KEY_PERIOD = 58,
    JR800_KEY_PF_1 = 59,
    JR800_KEY_PF_2 = 60,
    JR800_KEY_PF_3 = 61,
    JR800_KEY_PF_4 = 62,
    JR800_KEY_PF_5 = 63,
    JR800_KEY_PF_6 = 64,
    JR800_KEY_PF_7 = 65,
    JR800_KEY_PF_8 = 66,
    JR800_KEY_PF_9 = 67,
    JR800_KEY_PF_10 = 68,
    JR800_KEY_KEYPAD_8 = 69,
    JR800_KEY_KEYPAD_9 = 70,
    JR800_KEY_KEYPAD_MULTIPLY = 71,
    JR800_KEY_KEYPAD_ADD = 72,
    JR800_KEY_KEYPAD_EQUAL = 73,
    JR800_KEY_KEYPAD_SUBTRACT = 74,
    JR800_KEY_KEYPAD_DECIMAL = 75,
    JR800_KEY_KEYPAD_DIVIDE = 76
} jr800_key;

typedef struct jr800_hardware_configuration {
    uint32_t abi_version;
    uint32_t reset_stack_pointer_enabled;
    uint32_t reset_stack_pointer_value;
    uint32_t reset_index_register_enabled;
    uint32_t reset_index_register_value;
    uint32_t reset_accumulator_a_enabled;
    uint32_t reset_accumulator_a_value;
    uint32_t reset_accumulator_b_enabled;
    uint32_t reset_accumulator_b_value;
    uint32_t reset_condition_code_known_mask;
    uint32_t reset_condition_code_value;
    uint32_t internal_ram_enabled;
    uint32_t internal_ram_initial_value;
    uint32_t standard_ram_enabled;
    uint32_t standard_ram_initial_value;
    uint32_t expansion_ram_enabled;
    uint32_t expansion_ram_initial_value;
    uint32_t lcd_enabled;
    uint32_t lcd_unknown_data_read_value;
    uint32_t calendar_enabled;
    uint32_t calendar_address_source;
    uint32_t calendar_upper_read_bits;
    uint32_t calendar_cpu_cycle_ratio;
    uint32_t port1_pin_known_mask;
    uint32_t port1_pin_value;
    uint32_t port2_pin_known_mask;
    uint32_t port2_pin_value;
    uint32_t ram_standby_known;
    uint32_t ram_standby_valid;
    uint32_t keyboard_window_known;
    uint32_t keyboard_window_value;
    uint32_t ignore_unsupported_io;
} jr800_hardware_configuration;

/* All transport records contain only uint32_t words for a stable WASM layout. */
typedef struct jr800_machine_state {
    uint32_t abi_version;
    uint32_t profile;
    uint32_t pc;
    uint32_t sp;
    uint32_t x;
    uint32_t a;
    uint32_t b;
    uint32_t condition_code;
    uint32_t execution_state;
    uint32_t cycle_count_low;
    uint32_t cycle_count_high;
    uint32_t register_known_mask;
    uint32_t condition_code_known_mask;
    uint32_t calendar_alarm_terminal;
    uint32_t port2_timer_output;
    uint32_t lcd_substituted_data_read_count_valid;
    uint32_t lcd_substituted_data_read_count_low;
    uint32_t lcd_substituted_data_read_count_high;
    uint32_t ignored_io_access_count_valid;
    uint32_t ignored_io_access_count_low;
    uint32_t ignored_io_access_count_high;
} jr800_machine_state;

typedef struct jr800_stop_info {
    uint32_t reason;
    uint32_t fault;
    uint32_t trigger_address;
    uint32_t trigger_access_valid;
    uint32_t trigger_access;
    uint32_t instructions_executed_low;
    uint32_t instructions_executed_high;
    uint32_t pc_before;
    uint32_t pc_after;
    uint32_t byte0;
    uint32_t byte1;
    uint32_t byte2;
    uint32_t instruction_length;
    uint32_t bytes_fetched;
    uint32_t cycles;
    uint32_t bus_fault;
    uint32_t fault_access;
    uint32_t state_fault;
    uint32_t step_kind;
    uint32_t interrupt_source;
    uint32_t continuation_address_valid;
    uint32_t continuation_address;
    uint32_t condition_error;
    uint32_t condition_fault_address;
} jr800_stop_info;

typedef struct jr800_step_out_state {
    uint32_t continued;
    uint32_t nesting_depth_low;
    uint32_t nesting_depth_high;
} jr800_step_out_state;

typedef struct jr800_history_entry {
    uint32_t sequence_low;
    uint32_t sequence_high;
    uint32_t cycle_begin_low;
    uint32_t cycle_begin_high;
    uint32_t first_access_sequence_low;
    uint32_t first_access_sequence_high;
    uint32_t access_count;
    uint32_t pc_before;
    uint32_t pc_after;
    uint32_t byte0;
    uint32_t byte1;
    uint32_t byte2;
    uint32_t instruction_length;
    uint32_t bytes_fetched;
    uint32_t cycles;
    uint32_t fault;
    uint32_t state_pc;
    uint32_t state_sp;
    uint32_t state_x;
    uint32_t state_a;
    uint32_t state_b;
    uint32_t state_condition_code;
    uint32_t state_execution_state;
    uint32_t state_cycle_count_low;
    uint32_t state_cycle_count_high;
    uint32_t bus_fault;
    uint32_t fault_address;
    uint32_t fault_access;
    uint32_t state_fault;
    uint32_t step_kind;
    uint32_t interrupt_source;
    uint32_t state_register_known_mask;
    uint32_t state_condition_code_known_mask;
} jr800_history_entry;

typedef struct jr800_access_record {
    uint32_t sequence_low;
    uint32_t sequence_high;
    uint32_t instruction_cycle_low;
    uint32_t instruction_cycle_high;
    uint32_t instruction_pc;
    uint32_t address;
    uint32_t value;
    uint32_t value_known;
    uint32_t previous_value;
    uint32_t previous_value_known;
    uint32_t kind;
} jr800_access_record;

typedef struct jr800_access_filter {
    uint32_t first_address;
    uint32_t last_address;
    uint32_t kind_mask;
} jr800_access_filter;

typedef struct jr800_source_location {
    uint32_t address;
    uint32_t length;
    uint32_t source_file_index;
    uint32_t line;
    uint32_t column;
} jr800_source_location;

typedef struct jr800_disassembly {
    uint32_t address;
    uint32_t byte0;
    uint32_t byte1;
    uint32_t byte2;
    uint32_t length;
    uint32_t supported;
} jr800_disassembly;

typedef struct jr800_suspended_advance {
    uint32_t suspended;
    uint32_t cycles_elapsed;
    uint32_t interrupt_known;
    uint32_t interrupt_source;
    uint32_t bus_fault;
} jr800_suspended_advance;

typedef struct jr800_keyboard_activity {
    uint32_t read_attempts_low;
    uint32_t read_attempts_high;
    uint32_t distinct_addresses_low;
    uint32_t distinct_addresses_high;
} jr800_keyboard_activity;

typedef struct jr800_lcd_indicator_raw {
    uint32_t value_known;
    uint32_t value;
} jr800_lcd_indicator_raw;

typedef struct jr800_expression_watch_result {
    uint32_t value_low;
    uint32_t value_high;
    uint32_t error;
    uint32_t bus_fault;
    uint32_t fault_address;
    uint32_t state_fault;
} jr800_expression_watch_result;

typedef struct jr800_symbol_watch_result {
    uint32_t value;
    uint32_t binding;
    uint32_t kind;
    uint32_t size;
    uint32_t source_file_index_valid;
    uint32_t source_file_index;
} jr800_symbol_watch_result;

JR800_NODISCARD uint32_t jr800_machine_abi_version(void);
JR800_NODISCARD jr800_machine* jr800_machine_create(void);
JR800_NODISCARD jr800_machine* jr800_machine_create_jr800(
    const jr800_hardware_configuration* configuration
);
void jr800_machine_destroy(jr800_machine* machine);

JR800_NODISCARD jr800_status jr800_machine_load_application(
    jr800_machine* machine,
    const uint8_t* bytes,
    uint32_t byte_count,
    uint32_t initial_stack_pointer
);
JR800_NODISCARD jr800_status jr800_machine_load_debug_info(
    jr800_machine* machine,
    const uint8_t* bytes,
    uint32_t byte_count
);
JR800_NODISCARD jr800_status jr800_machine_load_logical_rom(
    jr800_machine* machine,
    const uint8_t* bytes,
    uint32_t byte_count
);
JR800_NODISCARD jr800_status jr800_machine_load_jr8rom(
    jr800_machine* machine,
    const uint8_t* bytes,
    uint32_t byte_count
);
JR800_NODISCARD jr800_status jr800_machine_load_program(
    jr800_machine* machine,
    const uint8_t* bytes,
    uint32_t byte_count
);
JR800_NODISCARD jr800_status jr800_machine_load_native_program_wav(
    jr800_machine* machine,
    const uint8_t* bytes,
    uint32_t byte_count,
    jr800_native_program_wav_issue* issue
);
JR800_NODISCARD jr800_status jr800_machine_reset(jr800_machine* machine);

JR800_NODISCARD jr800_status jr800_machine_get_state(
    const jr800_machine* machine,
    jr800_machine_state* state
);
JR800_NODISCARD jr800_status jr800_machine_step(
    jr800_machine* machine,
    jr800_stop_info* stop
);
JR800_NODISCARD jr800_status jr800_machine_step_over(
    jr800_machine* machine,
    uint32_t instruction_limit,
    jr800_stop_info* stop
);
JR800_NODISCARD jr800_status jr800_machine_step_out(
    jr800_machine* machine,
    uint32_t instruction_limit,
    jr800_step_out_state* state,
    jr800_stop_info* stop
);
JR800_NODISCARD jr800_status jr800_machine_run(
    jr800_machine* machine,
    uint32_t instruction_limit,
    jr800_stop_info* stop
);
JR800_NODISCARD jr800_status jr800_machine_run_to(
    jr800_machine* machine,
    uint32_t address,
    uint32_t instruction_limit,
    jr800_stop_info* stop
);
JR800_NODISCARD jr800_status jr800_machine_advance_suspended_cycles(
    jr800_machine* machine,
    uint32_t cycle_limit,
    jr800_suspended_advance* result
);
JR800_NODISCARD jr800_status
jr800_machine_advance_calendar_oscillator_ticks(
    jr800_machine* machine,
    uint32_t ticks
);
JR800_NODISCARD jr800_status jr800_machine_adjust_calendar_seconds(
    jr800_machine* machine
);

JR800_NODISCARD jr800_status jr800_machine_set_keyboard_bus_response(
    jr800_machine* machine,
    uint32_t address,
    uint32_t value,
    uint32_t known
);
JR800_NODISCARD jr800_status jr800_machine_set_keyboard_key_state(
    jr800_machine* machine,
    uint32_t key,
    uint32_t pressed
);
JR800_NODISCARD jr800_status jr800_machine_get_keyboard_activity(
    const jr800_machine* machine,
    jr800_keyboard_activity* activity
);
JR800_NODISCARD jr800_status jr800_machine_clear_keyboard_activity(
    jr800_machine* machine
);

JR800_NODISCARD jr800_status jr800_machine_set_execution_breakpoint(
    jr800_machine* machine,
    uint32_t address,
    uint32_t enabled
);
JR800_NODISCARD jr800_status
jr800_machine_set_conditional_execution_breakpoint(
    jr800_machine* machine,
    uint32_t address,
    const char* condition,
    uint32_t condition_size
);
JR800_NODISCARD jr800_status jr800_machine_set_expression_watch(
    jr800_machine* machine,
    uint32_t watch_id,
    const char* expression,
    uint32_t expression_size
);
JR800_NODISCARD jr800_status jr800_machine_clear_expression_watch(
    jr800_machine* machine,
    uint32_t watch_id
);
JR800_NODISCARD jr800_status jr800_machine_evaluate_expression_watch(
    const jr800_machine* machine,
    uint32_t watch_id,
    jr800_expression_watch_result* result
);
JR800_NODISCARD jr800_status jr800_machine_set_symbol_watch(
    jr800_machine* machine,
    uint32_t watch_id,
    const char* symbol_name,
    uint32_t symbol_name_size
);
JR800_NODISCARD jr800_status jr800_machine_clear_symbol_watch(
    jr800_machine* machine,
    uint32_t watch_id
);
JR800_NODISCARD jr800_status jr800_machine_evaluate_symbol_watch(
    const jr800_machine* machine,
    uint32_t watch_id,
    jr800_symbol_watch_result* result
);
JR800_NODISCARD jr800_status jr800_machine_set_memory_watchpoint(
    jr800_machine* machine,
    uint32_t address,
    uint32_t mode,
    uint32_t enabled
);

JR800_NODISCARD uint32_t jr800_machine_history_count(
    const jr800_machine* machine
);
JR800_NODISCARD uint32_t jr800_machine_copy_history(
    const jr800_machine* machine,
    jr800_history_entry* entries,
    uint32_t capacity
);
JR800_NODISCARD jr800_status jr800_machine_access_count(
    const jr800_machine* machine,
    const jr800_access_filter* filter,
    uint32_t* count
);
JR800_NODISCARD jr800_status jr800_machine_copy_accesses(
    const jr800_machine* machine,
    const jr800_access_filter* filter,
    jr800_access_record* records,
    uint32_t capacity,
    uint32_t* copied
);
void jr800_machine_clear_history(jr800_machine* machine);

JR800_NODISCARD jr800_status jr800_machine_read_memory(
    const jr800_machine* machine,
    uint32_t address,
    uint8_t* bytes,
    uint32_t byte_count
);
JR800_NODISCARD jr800_status jr800_machine_copy_lcd_panel(
    const jr800_machine* machine,
    uint8_t* dots,
    uint32_t capacity
);
JR800_NODISCARD jr800_status jr800_machine_copy_lcd_indicators(
    const jr800_machine* machine,
    jr800_lcd_indicator_raw* indicators,
    uint32_t capacity
);
JR800_NODISCARD jr800_status jr800_machine_source_at(
    const jr800_machine* machine,
    uint32_t address,
    jr800_source_location* location
);
JR800_NODISCARD jr800_status jr800_machine_source_address(
    const jr800_machine* machine,
    const char* source_path,
    uint32_t source_path_size,
    uint32_t line,
    uint32_t* address
);
JR800_NODISCARD jr800_status jr800_machine_symbol_address(
    const jr800_machine* machine,
    const char* symbol_name,
    uint32_t symbol_name_size,
    uint32_t* address
);
JR800_NODISCARD uint32_t jr800_machine_source_path_size(
    const jr800_machine* machine,
    uint32_t source_file_index
);
JR800_NODISCARD jr800_status jr800_machine_copy_source_path(
    const jr800_machine* machine,
    uint32_t source_file_index,
    char* text,
    uint32_t capacity
);
JR800_NODISCARD jr800_status jr800_machine_disassemble(
    const jr800_machine* machine,
    uint32_t address,
    jr800_disassembly* disassembly
);
JR800_NODISCARD uint32_t jr800_machine_disassembly_text_size(
    const jr800_machine* machine,
    uint32_t address
);
JR800_NODISCARD jr800_status jr800_machine_copy_disassembly_text(
    const jr800_machine* machine,
    uint32_t address,
    char* text,
    uint32_t capacity
);

#ifdef __cplusplus
}
#endif

#undef JR800_NODISCARD
