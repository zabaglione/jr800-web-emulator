; SPDX-License-Identifier: MIT
.global orchard_crops
.global orchard_growth
.global orchard_wet
.global orchard_dry
.global orchard_day
.global orchard_limit
.global orchard_goal
.global orchard_coins
.global orchard_actions
.global orchard_seed
.global orchard_rain
.global orchard_work
.global orchard_night
.section .text, code
game_start:
    JSR grid_reset
    CLR orchard_button
    CLR orchard_day_pending
    LDX #orchard_crops
    LDAB #72
    CLRA
orchard_zero:
    STAA 0,X
    INX
    DECB
    BNE orchard_zero
    CLR orchard_day
    CLR orchard_coins
    LDAA #10
    STAA orchard_coins + 1
    LDAA #3
    STAA orchard_actions
    LDAA #1
    STAA orchard_seed
    LDAA stage
    LDAB #16
    MUL
    ADDD #orchard_levels
    XGDX
    LDAA 0,X
    STAA orchard_limit
    LDAA 1,X
    STAA orchard_goal
    INX
    INX
    STX orchard_weather
    LDAA 0,X
    STAA orchard_rain
    RTS
game_aux:
    CMPA #1
    BEQ orchard_night
    LDAA orchard_seed
    EORA #3
    STAA orchard_seed
    LDAA #1
    STAA redraw
    RTS
game_update:
    JSR cursor_blink_tick
    CLR resume_pending
    TST orchard_button
    BNE orchard_button_input
    JSR grid_move
    CMPB #255
    BNE orchard_board_move
    LDAA input_event
    BITA #10
    BEQ orchard_space
    LDAA #1
    STAA orchard_button
    STAA redraw
    RTS
orchard_board_move:
    STAB cursor
    LDAA #1
    STAA redraw
orchard_space:
    LDAA input_event
    BITA #16
    BEQ orchard_update_done
    JMP orchard_work
orchard_update_done:
    RTS
orchard_button_input:
    LDAA input_event
    BITA #1
    BEQ orchard_button_side
    CLR orchard_button
    BRA orchard_button_changed
orchard_button_side:
    BITA #14
    BEQ orchard_button_action
    LDAA orchard_button
    EORA #3
    STAA orchard_button
orchard_button_changed:
    LDAA #1
    STAA redraw
orchard_button_action:
    LDAA input_event
    BITA #16
    BEQ orchard_update_done
    LDAA orchard_button
    JMP game_aux
orchard_night:
    LDAA orchard_day
    INCA
    CMPA orchard_limit
    BCS orchard_grow_begin
    LDAA #5
    STAA phase
    RTS
orchard_grow_begin:
    CLR orchard_index
orchard_plot:
    LDAB orchard_index
    LDX #orchard_crops
    ABX
    LDAA 0,X
    BEQ orchard_plot_next
    STAA orchard_crop
    TAB
    LDX #orchard_durations
    ABX
    LDAA 0,X
    STAA orchard_duration
    LDAB orchard_index
    LDX #orchard_growth
    ABX
    LDAA 0,X
    CMPA orchard_duration
    BEQ orchard_clear_wet
    LDX #orchard_wet
    ABX
    LDAA 0,X
    ORAA orchard_rain
    BEQ orchard_thirst
    LDX #orchard_growth
    ABX
    INC 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    BRA orchard_clear_wet
orchard_thirst:
    LDX #orchard_dry
    ABX
    INC 0,X
    LDAA 0,X
    CMPA #2
    BCS orchard_clear_wet
    CLR 0,X
    LDX #orchard_crops
    ABX
    CLR 0,X
    LDX #orchard_growth
    ABX
    CLR 0,X
orchard_clear_wet:
    LDX #orchard_wet
    ABX
    CLR 0,X
orchard_plot_next:
    INC orchard_index
    LDAA orchard_index
    CMPA #18
    BNE orchard_plot
    INC orchard_day
    LDAB orchard_day
    LDX orchard_weather
    ABX
    LDAA 0,X
    STAA orchard_rain
    LDAA #3
    STAA orchard_actions
    LDAA #1
    STAA redraw
    STAA orchard_day_pending
    RTS
orchard_work:
    TST orchard_actions
    BNE orchard_inspect
    RTS
orchard_inspect:
    LDAB cursor
    LDX #orchard_crops
    ABX
    LDAA 0,X
    BNE orchard_water_or_pick
    JMP orchard_plant
orchard_water_or_pick:
    STAA orchard_crop
    TAB
    LDX #orchard_durations
    ABX
    LDAA 0,X
    LDAB cursor
    LDX #orchard_growth
    ABX
    CMPA 0,X
    BEQ orchard_harvest
    LDX #orchard_wet
    ABX
    TST 0,X
    BEQ orchard_water
    RTS
orchard_water:
    INC 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    JMP orchard_spent
orchard_harvest:
    LDAB orchard_crop
    LDX #orchard_prices
    ABX
    LDAB 0,X
    CLRA
    ADDD orchard_coins
    STD orchard_coins
    LDAB cursor
    LDX #orchard_crops
    ABX
    CLR 0,X
    LDX #orchard_growth
    ABX
    CLR 0,X
    LDX #orchard_wet
    ABX
    CLR 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    JSR orchard_spent
    LDAA orchard_coins
    BNE orchard_clear
    LDAA orchard_coins + 1
    CMPA orchard_goal
    BCS orchard_work_done
orchard_clear:
    LDAA #4
    STAA phase
orchard_work_done:
    RTS
orchard_plant:
    LDAB orchard_seed
    LDX #orchard_costs
    ABX
    LDAB 0,X
    CLRA
    STD orchard_amount
    LDD orchard_coins
    SUBD orchard_amount
    BCS orchard_work_done
    STD orchard_coins
    LDAB cursor
    LDX #orchard_crops
    ABX
    LDAA orchard_seed
    STAA 0,X
    LDX #orchard_growth
    ABX
    CLR 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    LDX #orchard_wet
    ABX
    LDAA #1
    STAA 0,X
orchard_spent:
    DEC orchard_actions
    LDAA #1
    STAA redraw
    RTS
grid_value:
    STAB orchard_tile_cell
    LDX #orchard_crops
    ABX
    LDAA 0,X
    BEQ orchard_tile_done
    CMPA #2
    BEQ orchard_tile_berry
    LDX #orchard_growth
    ABX
    LDAA 0,X
    BEQ orchard_tile_bean_seed
    CMPA #3
    BEQ orchard_tile_bean_ripe
    LDAA #2
    BRA orchard_tile_water
orchard_tile_bean_seed:
    LDAA #1
    BRA orchard_tile_water
orchard_tile_bean_ripe:
    LDAA #3
    BRA orchard_tile_water
orchard_tile_berry:
    LDX #orchard_growth
    ABX
    LDAA 0,X
    CMPA #2
    BCS orchard_tile_berry_seed
    CMPA #5
    BEQ orchard_tile_berry_ripe
    LDAA #5
    BRA orchard_tile_water
orchard_tile_berry_seed:
    LDAA #4
    BRA orchard_tile_water
orchard_tile_berry_ripe:
    LDAA #6
orchard_tile_water:
    LDX #orchard_wet
    ABX
    TST 0,X
    BEQ orchard_tile_dry
    ADDA #6
    RTS
orchard_tile_dry:
    LDX #orchard_dry
    ABX
    TST 0,X
    BEQ orchard_tile_done
    ADDA #12
orchard_tile_done:
    RTS
game_render:
    JSR cursor_blink_prepare
    JSR cursor_blink_board
    JSR visual_hud
    TST orchard_day_pending
    BEQ orchard_render_done
    JMP orchard_day_banner
orchard_render_done:
    RTS
.global orchard_button
.section .bss, bss
orchard_button: .space 1
orchard_day_pending: .space 1
orchard_crops: .space 18
orchard_growth: .space 18
orchard_wet: .space 18
orchard_dry: .space 18
orchard_day: .space 1
orchard_limit: .space 1
orchard_goal: .space 1
orchard_coins: .space 2
orchard_actions: .space 1
orchard_seed: .space 1
orchard_rain: .space 1
orchard_weather: .space 2
orchard_index: .space 1
orchard_crop: .space 1
orchard_duration: .space 1
orchard_amount: .space 2
orchard_tile_cell: .space 1
orchard_hud_force: .space 1
orchard_old_coins: .space 2
orchard_old_actions: .space 1
orchard_old_day: .space 1
orchard_old_seed: .space 1
.section .data, data
orchard_durations: .byte 0,3,5
orchard_costs: .byte 0,2,3
orchard_prices: .byte 0,7,12
orchard_coins_label: .byte 67,79,73,78,83,0
orchard_actions_label: .byte 65,67,84,0
orchard_seed_label: .byte 83,69,69,68,0
orchard_goal_label: .byte 71,79,65,76,0
orchard_sun_label: .byte 83,85,78,32,0
orchard_rain_label: .byte 82,65,73,78,0
orchard_bean_label: .byte 66,69,65,78,32,0
orchard_berry_label: .byte 66,69,82,82,89,0
orchard_slash_label: .byte 47,0

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
cursor_blink_board:
    LDAA cursor
    PSHA
    TST orchard_button
    BNE orchard_blink_hide
    TST cursor_blink_mask
    BNE cursor_blink_paint
orchard_blink_hide:
    LDAA #255
    STAA cursor
cursor_blink_paint:
    JSR paint_board
    PULA
    STAA cursor
    RTS
.section .bss, bss
cursor_blink_mask: .space 1
cursor_blink_clock: .space 1

; Hold a clear day-change sign over the newly grown field, then restore it.
.global orchard_day_frame
.section .text, code
orchard_day_banner:
    CLR orchard_day_pending
    CLR orchard_banner_bytes
    CLR orchard_banner_bytes + 1
    JSR orchard_banner_flush
    LDX #framebuffer + 636
    STX orchard_banner_dest
    LDX #orchard_banner_saved
    STX orchard_banner_source
    LDAA #2
    STAA orchard_banner_rows
orchard_banner_save_row:
    LDAB #72
orchard_banner_save:
    LDX orchard_banner_dest
    LDAA 0,X
    CLR 0,X
    INX
    STX orchard_banner_dest
    LDX orchard_banner_source
    STAA 0,X
    INX
    STX orchard_banner_source
    DECB
    BNE orchard_banner_save
    LDD orchard_banner_dest
    ADDD #120
    STD orchard_banner_dest
    DEC orchard_banner_rows
    BNE orchard_banner_save_row
    LDX #orchard_next_label
    LDAA #72
    LDAB #3
    JSR paint_text
    LDX #orchard_day_label
    LDAA #72
    LDAB #4
    JSR paint_text
    LDAA #98
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA orchard_day
    INCA
    JSR paint_number
    LDX #framebuffer + 636
    STX orchard_banner_dest
    LDAA #2
    STAA orchard_banner_rows
orchard_banner_invert_row:
    LDX orchard_banner_dest
    LDAB #72
orchard_banner_invert:
    COM 0,X
    INX
    DECB
    BNE orchard_banner_invert
    XGDX
    ADDD #120
    STD orchard_banner_dest
    DEC orchard_banner_rows
    BNE orchard_banner_invert_row
    JSR orchard_banner_mark
    JSR orchard_banner_flush
orchard_day_frame:
    LDX #189
    LDD #48
    JSR sound_tone
    LDAA input_ticks
    STAA orchard_banner_clock
orchard_banner_wait:
    JSR input_poll
    LDAA input_ticks
    SUBA orchard_banner_clock
    CMPA #24
    BCS orchard_banner_wait
    LDX #orchard_banner_saved
    STX orchard_banner_source
    LDX #framebuffer + 636
    STX orchard_banner_dest
    LDAA #2
    STAA orchard_banner_rows
orchard_banner_restore_row:
    LDAB #72
orchard_banner_restore:
    LDX orchard_banner_source
    LDAA 0,X
    INX
    STX orchard_banner_source
    LDX orchard_banner_dest
    STAA 0,X
    INX
    STX orchard_banner_dest
    DECB
    BNE orchard_banner_restore
    LDD orchard_banner_dest
    ADDD #120
    STD orchard_banner_dest
    DEC orchard_banner_rows
    BNE orchard_banner_restore_row
    JSR orchard_banner_mark
    JSR orchard_banner_flush
    JSR input_gate
    JSR input_poll
    CLR redraw
    LDD orchard_banner_bytes
    STD dirty_bytes
    LDS #$5FFF
    JMP frame_ready
orchard_banner_mark:
    LDAA #3
    LDAB #60
    JSR dirty_mark
    LDAA #3
    LDAB #131
    JSR dirty_mark
    LDAA #4
    LDAB #60
    JSR dirty_mark
    LDAA #4
    LDAB #131
    JMP dirty_mark
orchard_banner_flush:
    JSR dirty_begin
orchard_banner_transfer:
    JSR dirty_next
    BEQ orchard_banner_sum
    JSR input_poll
    BRA orchard_banner_transfer
orchard_banner_sum:
    LDD dirty_bytes
    ADDD orchard_banner_bytes
    STD orchard_banner_bytes
    RTS
.section .data, data
orchard_next_label: .byte 78,69,88,84,32,68,65,89,0
orchard_day_label: .byte 68,65,89,0
.section .bss, bss
orchard_banner_saved: .space 144
orchard_banner_source: .space 2
orchard_banner_dest: .space 2
orchard_banner_rows: .space 1
orchard_banner_bytes: .space 2
orchard_banner_clock: .space 1
