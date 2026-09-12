; SPDX-License-Identifier: MIT
.global market_port
.global market_day
.global market_limit
.global market_capacity
.global market_cash
.global market_goal
.global market_cargo
.global market_load
.global market_good
.global market_mode
.global market_destination
.global market_prices
.global market_quotes
.global market_offset
.global market_calc_day
.global market_price
.global market_trade
.global market_sail
.global market_sell_all
.global market_message
.global market_reason
.section .text, code
game_start:
    CLR market_port
    CLR market_day
    CLR market_load
    CLR market_good
    CLR market_mode
    CLR market_destination
    CLR market_cargo
    CLR market_cargo + 1
    CLR market_cargo + 2
    CLR market_reason
    CLR market_message
    CLR selection_active
    LDAA stage
    LDAB #6
    MUL
    ADDD #market_levels
    XGDX
    LDAA 0,X
    STAA market_capacity
    CLR market_cash
    LDAA 1,X
    STAA market_cash + 1
    LDD 2,X
    STD market_goal
    LDAA 4,X
    STAA market_limit
    LDAA 5,X
    STAA market_offset
    JSR market_reprice
    LDAA #255
    STAA market_dirty
    STAA market_old_view
    RTS
game_aux:
    CMPA #1
    BNE market_sell_all
    LDAA #1
    STAA selection_active
    LDAA market_port
    INCA
    ANDA #3
    STAA market_destination
    CLR market_message
    JSR market_quote_all
    JMP market_changed
market_sell_all:
    CLR market_index
market_sell_loop:
    LDAB market_index
    LDX #market_prices
    ABX
    LDAA 0,X
    LDX #market_cargo
    ABX
    LDAB 0,X
    MUL
    ADDD market_cash
    STD market_cash
    LDAB market_index
    LDX #market_cargo
    ABX
    CLR 0,X
    INC market_index
    LDAA market_index
    CMPA #3
    BNE market_sell_loop
    CLR market_load
    JSR market_goal_check
    JMP market_changed
game_update:
    JSR cursor_blink_tick
    CLR resume_pending
    TST selection_active
    BEQ market_trade_input
    JMP market_port_input
market_trade_input:
    LDAA input_event
    BITA #1
    BEQ market_good_down
    TST market_good
    BNE market_good_up
    LDAA #4
    STAA market_good
market_good_up:
    DEC market_good
    JSR market_selection_changed
    BRA market_mode_input
market_good_down:
    BITA #2
    BEQ market_mode_input
    INC market_good
    LDAA market_good
    CMPA #4
    BCS market_good_changed
    CLR market_good
market_good_changed:
    JSR market_selection_changed
market_mode_input:
    LDAA input_event
    BITA #4
    BEQ market_mode_sell
    TST market_mode
    BEQ market_confirm_trade
    CLR market_mode
    JSR market_selection_changed
    BRA market_confirm_trade
market_mode_sell:
    BITA #8
    BEQ market_confirm_trade
    TST market_mode
    BNE market_confirm_trade
    INC market_mode
    JSR market_selection_changed
market_confirm_trade:
    LDAA input_event
    BITA #16
    BEQ market_input_done
    LDAB market_good
    CMPB #3
    BNE market_confirm_goods
    LDAA #1
    JMP game_aux
market_confirm_goods:
    JMP market_trade
market_port_input:
    LDAA input_event
    BITA #5
    BEQ market_port_next
    DEC market_destination
    BRA market_port_selected
market_port_next:
    BITA #10
    BEQ market_confirm_sail
    INC market_destination
market_port_selected:
    LDAA market_destination
    ANDA #3
    STAA market_destination
    CLR market_message
    JSR market_selection_changed
market_confirm_sail:
    LDAA input_event
    BITA #16
    BEQ market_input_done
    JMP market_sail
market_input_done:
    RTS
market_trade:
    LDAB market_good
    CMPB #3
    BCC market_input_done
    LDX #market_prices
    ABX
    LDAB 0,X
    CLRA
    STD market_amount
    TST market_mode
    BNE market_sell_one
    LDAA market_load
    CMPA market_capacity
    BCC market_input_done
    LDD market_cash
    SUBD market_amount
    BCS market_input_done
    STD market_cash
    LDAB market_good
    LDX #market_cargo
    ABX
    INC 0,X
    INC market_load
    JMP market_changed
market_sell_one:
    LDAB market_good
    LDX #market_cargo
    ABX
    TST 0,X
    BEQ market_input_done
    DEC 0,X
    DEC market_load
    LDD market_cash
    ADDD market_amount
    STD market_cash
    JSR market_goal_check
    JMP market_changed
market_goal_check:
    LDD market_cash
    SUBD market_goal
    BCS market_goal_done
    LDAA #4
    STAA phase
market_goal_done:
    RTS
market_sail:
    LDAA market_destination
    CMPA market_port
    BNE market_trip
    CLR selection_active
    JSR input_gate
    JMP market_changed
market_trip:
    JSR market_distance
    STAA market_trip_days
    ASLA
    TAB
    CLRA
    STD market_amount
    LDD market_cash
    SUBD market_amount
    BCC market_has_fare
    LDAA #1
    STAA market_message
    JMP market_selection_changed
market_has_fare:
    STD market_remaining_cash
    LDAA market_day
    ADDA market_trip_days
    CMPA market_limit
    BCS market_depart
    CLR selection_active
    LDAA #1
    STAA market_reason
    LDAA #5
    STAA phase
    JMP market_changed
market_depart:
    STAA market_day
    LDD market_remaining_cash
    STD market_cash
    LDAA market_destination
    STAA market_port
    CLR selection_active
    CLR market_message
    JSR market_reprice
    JSR input_gate
    LDD market_cash
    SUBD #2
    BCC market_arrived
    TST market_load
    BNE market_arrived
    LDAA #2
    STAA market_reason
    LDAA #5
    STAA phase
market_arrived:
    JMP market_changed
; A = destination; A = shortest number of days around four ports.
market_distance:
    SUBA market_port
    ANDA #3
    CMPA #3
    BNE market_distance_done
    LDAA #1
market_distance_done:
    RTS
; A = port, B = good, market_calc_day = day; returns A = price.
market_price:
    STAB market_calc_good
    LDAB #3
    MUL
    ADDB market_calc_good
    LDX #market_bases
    ABX
    LDAA 0,X
    STAA market_base
    LDAA market_calc_day
    ADDA market_offset
    ANDA #7
    STAA market_season
    LDAA market_calc_good
    ASLA
    ASLA
    ASLA
    ADDA market_season
    TAB
    LDX #market_seasons
    ABX
    LDAA 0,X
    ADDA market_base
    BMI market_min_price
    BEQ market_min_price
    RTS
market_min_price:
    LDAA #1
    RTS
market_reprice:
    LDAA market_day
    STAA market_calc_day
    CLR market_index
market_reprice_loop:
    LDAA market_port
    LDAB market_index
    JSR market_price
    LDAB market_index
    LDX #market_prices
    ABX
    STAA 0,X
    INC market_index
    LDAA market_index
    CMPA #3
    BNE market_reprice_loop
    RTS
market_quote_all:
    CLR market_quote_port
    CLR market_quote_index
market_quote_port_loop:
    LDAA market_quote_port
    JSR market_distance
    ADDA market_day
    STAA market_calc_day
    CLR market_index
market_quote_good_loop:
    LDAA market_quote_port
    LDAB market_index
    JSR market_price
    LDAB market_quote_index
    LDX #market_quotes
    ABX
    STAA 0,X
    INC market_quote_index
    INC market_index
    LDAA market_index
    CMPA #3
    BNE market_quote_good_loop
    INC market_quote_port
    LDAA market_quote_port
    CMPA #4
    BNE market_quote_port_loop
    RTS
market_selection_changed:
    LDAA market_dirty
    ORAA #4
    STAA market_dirty
    LDAA #1
    STAA redraw
    RTS
market_changed:
    LDAA market_dirty
    ORAA #2
    STAA market_dirty
    LDAA #1
    STAA redraw
    RTS
game_tile:
    CLRA
    RTS
game_render:
    JSR cursor_blink_prepare
    LDAA selection_active
    CMPA market_old_view
    BNE market_layout
    TST resume_pending
    BEQ market_render_view
market_layout:
    LDAA selection_active
    STAA market_old_view
    JSR paint_clear
market_render_view:
    TST selection_active
    BEQ market_render_values
    LDAA market_destination
    JSR market_distance
    STAA market_trip_days
market_render_values:
    ; Hide only selection arrows; prices, cargo and trip calculations stay real.
    LDAA market_good
    PSHA
    LDAA market_destination
    PSHA
    TST cursor_blink_mask
    BNE market_blink_hud
    LDAA #255
    STAA market_good
    STAA market_destination
market_blink_hud:
    JSR visual_hud
    PULA
    STAA market_destination
    PULA
    STAA market_good
    CLR market_dirty
    RTS
.section .bss, bss
market_port: .space 1
market_day: .space 1
market_limit: .space 1
market_capacity: .space 1
market_cash: .space 2
market_goal: .space 2
market_cargo: .space 3
market_load: .space 1
market_good: .space 1
market_mode: .space 1
market_destination: .space 1
market_prices: .space 3
market_quotes: .space 12
market_offset: .space 1
market_calc_day: .space 1
market_calc_good: .space 1
market_base: .space 1
market_season: .space 1
market_index: .space 1
market_quote_port: .space 1
market_quote_index: .space 1
market_trip_days: .space 1
market_amount: .space 2
market_remaining_cash: .space 2
market_reason: .space 1
market_message: .space 1
market_dirty: .space 1
market_old_view: .space 1
market_full: .space 1
market_paint_row: .space 1
market_paint_col: .space 1
market_text_pointer: .space 2
.section .data, data
market_port_names: .byte 78,79,82,84,72,0,69,65,83,84,32,0,83,79,85,84,72,0,87,69,83,84,32,0
market_good_names: .byte 82,73,67,69,32,0,79,82,69,32,32,0,83,80,73,67,69,0
market_day_label: .byte 68,65,89,0
market_slash_label: .byte 47,0
market_cash_label: .byte 67,65,83,72,0
market_goal_label: .byte 71,79,65,76,0
market_goods_label: .byte 71,79,79,68,83,0
market_price_label: .byte 80,82,73,67,69,0
market_hold_label: .byte 72,79,76,68,0
market_load_label: .byte 76,79,65,68,0
market_menu_label: .byte 82,69,84,85,82,78,58,32,83,65,73,76,32,47,32,77,69,78,85,0
market_blank_label: .byte 32,0
market_arrow_label: .byte 62,0
market_buy_label: .byte 66,85,89,32,0
market_sell_label: .byte 83,69,76,76,0
market_buy_action: .byte 83,80,65,67,69,32,66,85,89,32,0
market_sell_action: .byte 83,80,65,67,69,32,83,69,76,76,0
market_time_label: .byte 84,73,77,69,32,0
market_broke_label: .byte 66,82,79,75,69,0
market_choose_label: .byte 67,72,79,79,83,69,0
market_from_label: .byte 70,82,79,77,0
market_trip_label: .byte 84,82,73,80,0
market_fare_label: .byte 70,65,82,69,0
market_port_label: .byte 80,79,82,84,0
market_sail_help: .byte 83,80,65,67,69,32,83,65,73,76,32,82,69,84,85,82,78,32,66,65,67,75,0
market_no_fare_label: .byte 78,79,32,70,65,82,69,32,0

; The JR-800 input timer drives focus blinking; input immediately restores it.
.global cursor_blink_mask
.global cursor_blink_clock
.section .text, code
cursor_blink_prepare:
    TST hud_ready
    BEQ cursor_blink_show
    RTS
cursor_blink_tick:
    TST input_event
    BNE cursor_blink_show
    LDAA input_ticks
    SUBA cursor_blink_clock
    CMPA #25
    BCS cursor_blink_idle
    LDAA cursor_blink_mask
    EORA #128
    BRA cursor_blink_store
cursor_blink_show:
    LDAA #128
cursor_blink_store:
    CMPA cursor_blink_mask
    BEQ cursor_blink_time
    STAA cursor_blink_mask
    LDAA #1
    STAA redraw
cursor_blink_time:
    LDAA input_ticks
    STAA cursor_blink_clock
cursor_blink_idle:
    RTS
.section .bss, bss
cursor_blink_mask: .space 1
cursor_blink_clock: .space 1
