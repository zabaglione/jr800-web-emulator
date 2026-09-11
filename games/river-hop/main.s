; SPDX-License-Identifier: MIT
.global river_lanes
.global river_periods
.global river_timers
.global river_homes
.global river_running
.global river_lives
.global river_time
.global river_clock
.global river_score
.global river_world
.global river_check
.global river_hop
.section .text, code
game_start:
    JSR grid_reset
    CLR river_score
    CLR river_score + 1
    LDX #river_homes
    LDAB #5
river_home_clear:
    CLR 0,X
    INX
    DECB
    BNE river_home_clear
    LDAA #5
    STAA river_lives
    LDAA stage
    LDAB #60
    MUL
    ADDD #river_levels
    STD river_pointer
    CLR river_index
river_load:
    LDX river_pointer
    LDAA 0,X
    INX
    STX river_pointer
    LDAB river_index
    LDX #river_lanes
    ABX
    STAA 0,X
    INC river_index
    LDAA river_index
    CMPA #60
    BNE river_load
    LDD river_periods
    STD river_timers
    LDD river_periods + 2
    STD river_timers + 2
river_spawn:
    LDAA #90
    STAA cursor
    LDAA #100
    STAA river_time
    CLR river_running
    RTS
game_aux:
    CMPA #2
    BNE river_pause
    JMP game_start
river_pause:
    CLR river_running
    JMP grid_changed
game_update:
    TST resume_pending
    BEQ river_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA river_clock
river_clock_ready:
    LDAA input_event
    BITA #16
    BEQ river_controls
    TST river_running
    BNE river_controls
    LDAA #1
    STAA river_running
    LDAA input_ticks
    STAA river_clock
    JSR grid_changed
river_controls:
    TST river_running
    BEQ river_idle
    JSR grid_move
    CMPB #255
    BEQ river_timer
    JSR river_hop
    JSR grid_changed
river_timer:
    TST river_running
    BEQ river_idle
    LDAA input_ticks
    SUBA river_clock
    CMPA #12
    BCS river_idle
    LDAA input_ticks
    STAA river_clock
    JSR river_world
    JMP grid_changed
river_idle:
    RTS
; B is the destination cell. A hop is checked before the traffic advances.
river_hop:
    STAB cursor
    JMP river_check
river_world:
    DEC river_time
    BNE river_time_left
    JMP river_died
river_time_left:
    CLR river_lane
river_lane_tick:
    LDAB river_lane
    LDX #river_timers
    ABX
    DEC 0,X
    BNE river_lane_next
    LDX #river_periods
    ABX
    LDAA 0,X
    LDX #river_timers
    ABX
    STAA 0,X
    LDX #river_directions
    ABX
    LDAA 0,X
    STAA river_direction
    LDX #river_rows
    ABX
    LDAA 0,X
    LDAB #14
    MUL
    STAB river_row_start
    LDAA river_lane
    LDAB #14
    MUL
    ADDD #river_lanes
    STD river_base
    XGDX
    LDAA river_direction
    BMI river_shift_left
    LDAA 13,X
    STAA river_edge
    LDAB #13
    ABX
river_right_loop:
    DEX
    LDAA 0,X
    STAA 1,X
    DECB
    BNE river_right_loop
    LDAA river_edge
    STAA 0,X
    BRA river_carry
river_shift_left:
    LDAA 0,X
    STAA river_edge
    LDAB #13
river_left_loop:
    LDAA 1,X
    STAA 0,X
    INX
    DECB
    BNE river_left_loop
    LDAA river_edge
    STAA 0,X
river_carry:
    LDAA river_lane
    CMPA #2
    BCC river_lane_next
    LDAA cursor
    SUBA river_row_start
    BCS river_lane_next
    CMPA #14
    BCC river_lane_next
    ADDA river_direction
    CMPA #14
    BCC river_died
    ADDA river_row_start
    STAA cursor
river_lane_next:
    INC river_lane
    LDAA river_lane
    CMPA #4
    BEQ river_lanes_done
    JMP river_lane_tick
river_lanes_done:
    JMP river_check
river_died:
    DEC river_lives
    BEQ river_failed
    JMP river_spawn
river_failed:
    CLR river_running
    LDAA #5
    STAA phase
    RTS
river_check:
    LDAA cursor
    CMPA #14
    BCS river_home_check
    CMPA #42
    BCS river_water_check
    CMPA #56
    BCS river_safe
    CMPA #84
    BCC river_safe
    SUBA #28
    TAB
    LDX #river_lanes
    ABX
    TST 0,X
    BNE river_died
river_safe:
    RTS
river_water_check:
    SUBA #14
    TAB
    LDX #river_lanes
    ABX
    TST 0,X
    BEQ river_died
    RTS
river_home_check:
    CLR river_index
river_find_home:
    LDAB river_index
    LDX #river_home_columns
    ABX
    CMPA 0,X
    BEQ river_home_found
    INC river_index
    LDAB river_index
    CMPB #5
    BNE river_find_home
    BRA river_died
river_home_found:
    LDX #river_homes
    ABX
    TST 0,X
    BNE river_died
    LDAA #1
    STAA 0,X
    INC grid_stat
    LDD river_score
    ADDB river_time
    ADCA #0
    ADDD #100
    STD river_score
    LDAA grid_stat
    CMPA #5
    BCC river_clear
    JMP river_spawn
river_clear:
    CLR river_running
    LDAA #4
    STAA phase
    RTS
grid_value:
    CMPB cursor
    BNE river_background
    LDAA #8
    RTS
river_background:
    CMPB #14
    BCS river_home_tile
    CMPB #42
    BCS river_water_tile
    CMPB #56
    BCS river_grass_tile
    CMPB #84
    BCC river_grass_tile
    SUBB #28
    LDX #river_lanes
    ABX
    TST 0,X
    BEQ river_road_tile
    LDAA #7
    RTS
river_road_tile:
    LDAA #3
    RTS
river_water_tile:
    SUBB #14
    LDX #river_lanes
    ABX
    TST 0,X
    BEQ river_open_water
    LDAA #6
    RTS
river_open_water:
    LDAA #2
    RTS
river_grass_tile:
    LDAA #1
    RTS
river_home_tile:
    STAB river_tile_cell
    CLR river_tile_index
river_home_tile_loop:
    LDAB river_tile_index
    LDX #river_home_columns
    ABX
    LDAA 0,X
    CMPA river_tile_cell
    BEQ river_home_tile_found
    INC river_tile_index
    LDAA river_tile_index
    CMPA #5
    BNE river_home_tile_loop
    BRA river_open_water
river_home_tile_found:
    LDX #river_homes
    ABX
    LDAA 0,X
    ADDA #4
    RTS
game_render:
    JSR paint_board
    CLR river_hud_force
    TST resume_pending
    BEQ river_score_check
    INC river_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDX #river_score_label
    LDAA #72
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #river_time_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #river_home_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #river_life_label
    LDAA #138
    LDAB #5
    JSR paint_text
river_score_check:
    TST river_hud_force
    BNE river_score_draw
    LDD river_score
    SUBD river_old_score
    BEQ river_time_check
river_score_draw:
    LDD river_score
    STD river_old_score
    LDAA #84
    STAA paint_x
    LDAA #0
    STAA paint_band
    LDD river_score
    JSR paint_number16
river_time_check:
    TST river_hud_force
    BNE river_time_draw
    LDAA river_time
    CMPA river_old_time
    BEQ river_home_hud_check
river_time_draw:
    LDAA river_time
    STAA river_old_time
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA river_time
    JSR paint_number
river_home_hud_check:
    TST river_hud_force
    BNE river_home_draw
    LDAA grid_stat
    CMPA river_old_home
    BEQ river_life_check
river_home_draw:
    LDAA grid_stat
    STAA river_old_home
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA grid_stat
    JSR paint_number
river_life_check:
    TST river_hud_force
    BNE river_life_draw
    LDAA river_lives
    CMPA river_old_life
    BEQ river_state_check
river_life_draw:
    LDAA river_lives
    STAA river_old_life
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA river_lives
    JSR paint_number
river_state_check:
    TST river_hud_force
    BNE river_state_draw
    LDAA river_running
    CMPA river_old_running
    BEQ river_render_done
river_state_draw:
    LDAA river_running
    STAA river_old_running
    LDX #river_start_label
    TSTA
    BEQ river_action_text
    LDX #grid_return_label
river_action_text:
    LDAA #138
    LDAB #7
    JMP paint_text
river_render_done:
    RTS
.section .bss, bss
river_lanes: .space 56
river_periods: .space 4
river_timers: .space 4
river_homes: .space 5
river_running: .space 1
river_lives: .space 1
river_time: .space 1
river_clock: .space 1
river_score: .space 2
river_pointer: .space 2
river_index: .space 1
river_lane: .space 1
river_direction: .space 1
river_row_start: .space 1
river_base: .space 2
river_edge: .space 1
river_tile_cell: .space 1
river_tile_index: .space 1
river_hud_force: .space 1
river_old_score: .space 2
river_old_time: .space 1
river_old_home: .space 1
river_old_life: .space 1
river_old_running: .space 1
.section .data, data
river_score_label: .byte 83,0
river_time_label: .byte 84,73,77,69,0
river_home_label: .byte 72,79,77,69,0
river_life_label: .byte 76,73,70,69,0
river_start_label: .byte 83,80,65,67,69,32,0
