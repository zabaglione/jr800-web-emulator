; SPDX-License-Identifier: MIT
.global orbit_enemies
.global orbit_wave
.global orbit_position
.global orbit_total
.global orbit_left
.global orbit_core
.global orbit_pulse
.global orbit_aim
.global orbit_running
.global orbit_flash
.global orbit_score
.global orbit_steps
.global orbit_turn
.global orbit_world
.global orbit_shoot
.global orbit_burst
.global orbit_weapon_clock
.section .text, code
game_start:
    JSR grid_reset
    CLR orbit_position
    CLR orbit_aim
    CLR orbit_running
    CLR orbit_flash
    CLR orbit_score
    CLR orbit_score + 1
    CLR orbit_steps
    CLR orbit_turn
    LDAA #55
    STAA cursor
    LDAA #5
    STAA orbit_core
    LDAA #1
    STAA orbit_pulse
    LDAA stage
    ADDA #12
    STAA orbit_total
    STAA orbit_left
    LDAA stage
    LDAB #24
    MUL
    ADDD #orbit_levels
    STD orbit_pointer
    CLR orbit_index
orbit_load_wave:
    LDX orbit_pointer
    LDAA 0,X
    INX
    STX orbit_pointer
    LDAB orbit_index
    LDX #orbit_wave
    ABX
    STAA 0,X
    LDX #orbit_enemies
    ABX
    CLR 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_load_wave
    CLR orbit_index
orbit_load_background:
    LDAB orbit_index
    LDX #orbit_background
    ABX
    LDAA 0,X
    LDX #board
    ABX
    STAA 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #112
    BNE orbit_load_background
    LDAA input_ticks
    STAA orbit_clock
    SUBA #4
    STAA orbit_weapon_clock
    RTS
game_aux:
    CMPA #2
    BNE orbit_burst
    JMP game_start
orbit_burst:
    TST orbit_pulse
    BEQ orbit_burst_done
    CLR orbit_pulse
    CLR orbit_index
orbit_burst_loop:
    LDAB orbit_index
    LDX #orbit_enemies
    ABX
    LDAA 0,X
    BEQ orbit_burst_next
    CLR 0,X
    LDAB #10
    MUL
    ADDD orbit_score
    STD orbit_score
    DEC orbit_left
orbit_burst_next:
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_burst_loop
    JMP orbit_check_end
orbit_burst_done:
    RTS
game_update:
    TST resume_pending
    BEQ orbit_input
    CLR resume_pending
    LDAA input_ticks
    STAA orbit_clock
orbit_input:
    LDAA input_event
    BITA #1
    BEQ orbit_down
    CLR orbit_aim
    BRA orbit_turned
orbit_down:
    BITA #2
    BEQ orbit_left_input
    LDAA #4
    STAA orbit_aim
    BRA orbit_turned
orbit_left_input:
    BITA #4
    BEQ orbit_right_input
    LDAA orbit_aim
    DECA
    ANDA #7
    STAA orbit_aim
    BRA orbit_turned
orbit_right_input:
    BITA #8
    BEQ orbit_fire_input
    INC orbit_aim
    LDAA orbit_aim
    ANDA #7
    STAA orbit_aim
orbit_turned:
    LDAA #1
    STAA redraw
orbit_fire_input:
    LDAA input_event
    BITA #16
    BEQ orbit_timer
    TST orbit_running
    BNE orbit_weapon_ready
    LDAA #1
    STAA orbit_running
    STAA redraw
    LDAA input_ticks
    STAA orbit_clock
orbit_weapon_ready:
    LDAA input_ticks
    SUBA orbit_weapon_clock
    CMPA #4
    BCS orbit_timer
    LDAA input_ticks
    STAA orbit_weapon_clock
    LDAA #2
    STAA orbit_flash
    LDAA #1
    STAA redraw
    JSR orbit_shoot
orbit_timer:
    LDAA phase
    CMPA #2
    BNE orbit_update_done
    TST orbit_running
    BEQ orbit_update_done
    LDAA stage
    LSRA
    LSRA
    TAB
    LDX #orbit_periods
    ABX
    LDAA input_ticks
    SUBA orbit_clock
    CMPA 0,X
    BCS orbit_update_done
    LDAA input_ticks
    STAA orbit_clock
    JSR orbit_world
    LDAA #1
    STAA redraw
orbit_update_done:
    RTS
orbit_shoot:
    LDAB orbit_aim
orbit_shoot_ring:
    LDX #orbit_enemies
    ABX
    TST 0,X
    BNE orbit_hit
    ADDB #8
    CMPB #24
    BCS orbit_shoot_ring
    RTS
orbit_hit:
    DEC 0,X
    BNE orbit_hit_score
    DEC orbit_left
orbit_hit_score:
    LDD orbit_score
    ADDD #10
    STD orbit_score
    JMP orbit_check_end
orbit_world:
    INC orbit_steps
    TST orbit_flash
    BEQ orbit_world_tick
    DEC orbit_flash
orbit_world_tick:
    LDAA orbit_turn
    EORA #1
    STAA orbit_turn
    BEQ orbit_advance
    RTS
orbit_advance:
    CLR orbit_index
orbit_leak_loop:
    LDAB orbit_index
    LDX #orbit_enemies
    ABX
    TST 0,X
    BEQ orbit_leak_next
    DEC orbit_left
    TST orbit_core
    BEQ orbit_leak_next
    DEC orbit_core
orbit_leak_next:
    INC orbit_index
    LDAA orbit_index
    CMPA #8
    BNE orbit_leak_loop
    LDX #orbit_next
    LDAB #24
    CLRA
orbit_next_clear:
    STAA 0,X
    INX
    DECB
    BNE orbit_next_clear
    LDAA #8
    STAA orbit_index
orbit_shift_loop:
    LDAB orbit_index
    LDX #orbit_enemies
    ABX
    LDAA 0,X
    STAA orbit_value
    LDAA orbit_index
    SUBA #8
    STAA orbit_target
    LDAA stage
    CMPA #8
    BCS orbit_shift_store
    LDAA orbit_target
    ANDA #248
    STAA orbit_target_ring
    LDAA orbit_target
    INCA
    ANDA #7
    ADDA orbit_target_ring
    STAA orbit_target
orbit_shift_store:
    LDAB orbit_target
    LDX #orbit_next
    ABX
    LDAA orbit_value
    STAA 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_shift_loop
    CLR orbit_index
orbit_copy_next:
    LDAB orbit_index
    LDX #orbit_next
    ABX
    LDAA 0,X
    LDX #orbit_enemies
    ABX
    STAA 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_copy_next
    JSR orbit_check_end
    LDAA phase
    CMPA #2
    BNE orbit_world_done
    LDAA orbit_position
    CMPA orbit_total
    BCC orbit_world_done
    TAB
    LDX #orbit_wave
    ABX
    LDAA 0,X
    STAA orbit_code
    ANDA #7
    ADDA #16
    TAB
    LDX #orbit_enemies
    ABX
    LDAA orbit_code
    LSRA
    LSRA
    LSRA
    INCA
    STAA 0,X
    INC orbit_position
orbit_world_done:
    RTS
orbit_check_end:
    TST orbit_core
    BNE orbit_check_left
    LDAA #5
    STAA phase
    RTS
orbit_check_left:
    TST orbit_left
    BNE orbit_end_done
    LDAA #4
    STAA phase
orbit_end_done:
    RTS
grid_value:
    CMPB cursor
    BNE orbit_tile_slot
    LDAA orbit_aim
    ADDA #ORBIT_PLAYER
    RTS
orbit_tile_slot:
    STAB orbit_tile_cell
    LDX #orbit_slots
    ABX
    LDAB 0,X
    CMPB #255
    BEQ orbit_tile_background
    LDX #orbit_enemies
    ABX
    LDAA 0,X
    BEQ orbit_tile_flash
    DECA
    ADDA #ORBIT_ENEMY
    RTS
orbit_tile_flash:
    TST orbit_flash
    BEQ orbit_tile_background
    TBA
    ANDA #7
    CMPA orbit_aim
    BNE orbit_tile_background
    LDAA #ORBIT_BEAM
    RTS
orbit_tile_background:
    LDAB orbit_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    CLR orbit_hud_force
    TST resume_pending
    BEQ orbit_hud_values
    INC orbit_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #orbit_left_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #orbit_core_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #orbit_pulse_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #orbit_score_label
    LDAA #132
    LDAB #7
    JSR paint_text
orbit_hud_values:
    TST orbit_hud_force
    BNE orbit_left_value
    LDAA orbit_left
    CMPA orbit_old_left
    BEQ orbit_core_check
orbit_left_value:
    LDAA orbit_left
    STAA orbit_old_left
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA orbit_left
    JSR paint_number
orbit_core_check:
    TST orbit_hud_force
    BNE orbit_core_value
    LDAA orbit_core
    CMPA orbit_old_core
    BEQ orbit_pulse_check
orbit_core_value:
    LDAA orbit_core
    STAA orbit_old_core
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA orbit_core
    JSR paint_number
orbit_pulse_check:
    TST orbit_hud_force
    BNE orbit_pulse_value
    LDAA orbit_pulse
    CMPA orbit_old_pulse
    BEQ orbit_score_check
orbit_pulse_value:
    LDAA orbit_pulse
    STAA orbit_old_pulse
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA orbit_pulse
    JSR paint_number
orbit_score_check:
    TST orbit_hud_force
    BNE orbit_score_value
    LDD orbit_score
    SUBD orbit_old_score
    BEQ orbit_running_check
orbit_score_value:
    LDD orbit_score
    STD orbit_old_score
    LDAA #138
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDD orbit_score
    JSR paint_number16
orbit_running_check:
    TST orbit_hud_force
    BNE orbit_running_value
    LDAA orbit_running
    CMPA orbit_old_running
    BEQ orbit_render_done
orbit_running_value:
    LDAA orbit_running
    STAA orbit_old_running
    LDX #orbit_fire_label
    TSTA
    BNE orbit_running_text
    LDX #orbit_start_label
orbit_running_text:
    LDAA #78
    CLRB
    JMP paint_text
orbit_render_done:
    RTS
.section .bss, bss
orbit_enemies: .space 24
orbit_next: .space 24
orbit_wave: .space 24
orbit_position: .space 1
orbit_total: .space 1
orbit_left: .space 1
orbit_core: .space 1
orbit_pulse: .space 1
orbit_aim: .space 1
orbit_running: .space 1
orbit_flash: .space 1
orbit_score: .space 2
orbit_steps: .space 1
orbit_turn: .space 1
orbit_clock: .space 1
orbit_weapon_clock: .space 1
orbit_pointer: .space 2
orbit_index: .space 1
orbit_value: .space 1
orbit_target: .space 1
orbit_target_ring: .space 1
orbit_code: .space 1
orbit_tile_cell: .space 1
orbit_hud_force: .space 1
orbit_old_left: .space 1
orbit_old_core: .space 1
orbit_old_pulse: .space 1
orbit_old_score: .space 2
orbit_old_running: .space 1
.section .data, data
orbit_periods: .byte 22,20,18
orbit_left_label: .byte 76,69,70,84,0
orbit_core_label: .byte 67,79,82,69,0
orbit_pulse_label: .byte 80,85,76,83,69,0
orbit_score_label: .byte 83,0
orbit_fire_label: .byte 83,80,65,67,69,32,70,73,82,69,0
orbit_start_label: .byte 83,80,65,67,69,32,71,79,32,32,0
