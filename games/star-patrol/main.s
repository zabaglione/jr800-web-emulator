; SPDX-License-Identifier: MIT
.global star_enemies
.global star_cells
.global star_shields
.global star_left
.global star_lives
.global star_running
.global star_shot
.global star_bolt
.global star_offset
.global star_base
.global star_direction
.global star_bounces
.global star_march_clock
.global star_fire_clock
.global star_score
.global star_steps
.global star_world
.global star_hit
.global star_contact
.section .text, code
game_start:
    JSR grid_reset
    CLR star_steps
    CLR star_score
    CLR star_score + 1
    CLR star_offset
    CLR star_base
    CLR star_bounces
    CLR star_march_clock
    CLR star_fire_clock
    CLR star_left
    LDAA #1
    STAA star_direction
    LDAA #3
    STAA star_lives
    LDAA stage
    LDAB #20
    MUL
    ADDD #star_levels
    STD star_pointer
    CLR star_index
star_load:
    LDX star_pointer
    LDAA 0,X
    INX
    STX star_pointer
    TSTA
    BEQ star_store
    INC star_left
star_store:
    LDAB star_index
    LDX #star_enemies
    ABX
    STAA 0,X
    INC star_index
    LDAA star_index
    CMPA #20
    BNE star_load
    LDX #star_shields
    LDAB #16
    CLRA
star_clear_shields:
    STAA 0,X
    INX
    DECB
    BNE star_clear_shields
    LDAA #2
    STAA star_shields + 2
    STAA star_shields + 3
    STAA star_shields + 7
    STAA star_shields + 8
    STAA star_shields + 12
    STAA star_shields + 13
    JSR star_ready
    JMP star_view
star_ready:
    LDAA #104
    STAA cursor
    LDAA #255
    STAA star_shot
    STAA star_bolt
    CLR star_running
    LDAA input_ticks
    STAA star_clock
    RTS
game_aux:
    CMPA #2
    BNE star_pause
    JMP game_start
star_pause:
    CLR star_running
    RTS
game_update:
    TST resume_pending
    BEQ star_input
    CLR resume_pending
    LDAA input_ticks
    STAA star_clock
star_input:
    LDAA input_event
    BITA #4
    BEQ star_right
    LDAA cursor
    CMPA #96
    BEQ star_space
    DEC cursor
    BRA star_moved
star_right:
    BITA #8
    BEQ star_space
    LDAA cursor
    CMPA #111
    BEQ star_space
    INC cursor
star_moved:
    LDAA #1
    STAA redraw
    JSR star_contact
star_space:
    LDAA phase
    CMPA #2
    BNE star_update_done
    LDAA input_event
    BITA #16
    BEQ star_timer
    LDAA #1
    STAA star_running
    STAA redraw
    LDAA star_shot
    CMPA #255
    BNE star_timer
    LDAA cursor
    SUBA #16
    STAA star_shot
    JSR star_hit
star_timer:
    TST star_running
    BEQ star_update_done
    LDAA phase
    CMPA #2
    BNE star_update_done
    LDAA input_ticks
    SUBA star_clock
    CMPA #6
    BCS star_update_done
    LDAA input_ticks
    STAA star_clock
    JSR star_world
    LDAA #1
    STAA redraw
star_update_done:
    RTS
star_world:
    INC star_steps
    LDAA star_shot
    CMPA #255
    BEQ star_world_bolt
    SUBA #16
    BCC star_move_shot
    LDAA #255
star_move_shot:
    STAA star_shot
    JSR star_hit
star_world_bolt:
    LDAA phase
    CMPA #2
    BEQ star_bolt_live
    RTS
star_bolt_live:
    LDAA star_bolt
    CMPA #255
    BEQ star_march
    ADDA #16
    CMPA #112
    BCS star_move_bolt
    LDAA #255
star_move_bolt:
    STAA star_bolt
    JSR star_contact
    LDAA phase
    CMPA #2
    BEQ star_bolt_survived
    RTS
star_bolt_survived:
    TST star_running
    BEQ star_world_done
star_march:
    INC star_march_clock
    LDAA stage
    LSRA
    LSRA
    TAB
    LDX #star_periods
    ABX
    LDAA star_march_clock
    CMPA 0,X
    BCS star_fire
    CLR star_march_clock
    LDAA star_offset
    ADDA star_direction
    CMPA #7
    BCS star_march_store
    NEG star_direction
    INC star_bounces
    LDAA star_bounces
    CMPA #2
    BCS star_march_refresh
    CLR star_bounces
    INC star_base
    LDAA star_base
    CMPA #5
    BCS star_march_refresh
    LDAA #5
    STAA phase
    RTS
star_march_store:
    STAA star_offset
star_march_refresh:
    JSR star_view
    JSR star_hit
    LDAA phase
    CMPA #2
    BNE star_world_done
star_fire:
    INC star_fire_clock
    LDAA star_fire_clock
    CMPA #8
    BCS star_world_done
    CLR star_fire_clock
    LDAA star_bolt
    CMPA #255
    BNE star_world_done
    JSR star_spawn
    JSR star_contact
star_world_done:
    RTS
star_spawn:
    JSR random
    DECA
star_mod:
    CMPA star_left
    BCS star_rank
    SUBA star_left
    BRA star_mod
star_rank:
    STAA star_pick
    CLR star_index
star_pick_loop:
    LDAB star_index
    LDX #star_enemies
    ABX
    TST 0,X
    BEQ star_pick_next
    TST star_pick
    BEQ star_pick_found
    DEC star_pick
star_pick_next:
    INC star_index
    BRA star_pick_loop
star_pick_found:
    CMPB #10
    BCC star_spawn_cell
    TST 10,X
    BEQ star_spawn_cell
    ADDB #10
star_spawn_cell:
    LDX #star_cells
    ABX
    LDAA 0,X
    ADDA #16
    STAA star_bolt
    RTS
; Upward shots cancel crossing bolts, damage one alien or chip one shield.
star_hit:
    LDAB star_shot
    CMPB #255
    BEQ star_hit_done
    CMPB star_bolt
    BEQ star_cancel
    CLR star_index
star_hit_loop:
    LDAB star_index
    LDX #star_cells
    ABX
    LDAA 0,X
    CMPA star_shot
    BNE star_hit_next
    LDX #star_enemies
    ABX
    TST 0,X
    BEQ star_hit_next
    DEC 0,X
    BNE star_hit_score
    DEC star_left
star_hit_score:
    LDD star_score
    ADDD #10
    STD star_score
    LDAA #255
    STAA star_shot
    JSR star_view
    TST star_left
    BNE star_hit_done
    LDAA #4
    STAA phase
    RTS
star_hit_next:
    INC star_index
    LDAA star_index
    CMPA #20
    BNE star_hit_loop
    LDAA star_shot
    SUBA #80
    CMPA #16
    BCC star_hit_done
    TAB
    LDX #star_shields
    ABX
    TST 0,X
    BEQ star_hit_done
    DEC 0,X
    LDAA #255
    STAA star_shot
    JMP star_view
star_cancel:
    LDAA #255
    STAA star_shot
    STAA star_bolt
star_hit_done:
    RTS
star_contact:
    LDAA star_bolt
    CMPA #255
    BEQ star_contact_done
    CMPA star_shot
    BEQ star_cancel
    CMPA cursor
    BEQ star_die
    SUBA #80
    CMPA #16
    BCC star_contact_done
    TAB
    LDX #star_shields
    ABX
    TST 0,X
    BEQ star_contact_done
    DEC 0,X
    LDAA #255
    STAA star_bolt
    JMP star_view
star_die:
    DEC star_lives
    BEQ star_failed
    JSR input_gate
    JMP star_ready
star_failed:
    LDAA #5
    STAA phase
star_contact_done:
    RTS
star_view:
    LDX #board
    LDAB #112
    CLRA
star_view_clear:
    STAA 0,X
    INX
    DECB
    BNE star_view_clear
    CLR star_view_index
star_view_shields:
    LDAB star_view_index
    LDX #star_shields
    ABX
    LDAA 0,X
    BEQ star_view_shield_next
    ADDA #2
    LDX #board + 80
    ABX
    STAA 0,X
star_view_shield_next:
    INC star_view_index
    LDAA star_view_index
    CMPA #16
    BNE star_view_shields
    LDAA star_base
    ASLA
    ASLA
    ASLA
    ASLA
    ADDA star_offset
    STAA star_view_cell
    CLR star_view_index
star_view_enemies:
    LDAB star_view_index
    LDX #star_cells
    ABX
    LDAA star_view_cell
    STAA 0,X
    LDX #star_enemies
    ABX
    LDAA 0,X
    BEQ star_view_enemy_next
    LDAB star_view_cell
    LDX #board
    ABX
    STAA 0,X
star_view_enemy_next:
    INC star_view_cell
    INC star_view_index
    LDAA star_view_index
    CMPA #10
    BNE star_view_last
    LDAA star_view_cell
    ADDA #6
    STAA star_view_cell
star_view_last:
    LDAA star_view_index
    CMPA #20
    BNE star_view_enemies
    RTS
grid_value:
    CMPB cursor
    BNE star_tile_shot
    LDAA #7
    RTS
star_tile_shot:
    CMPB star_shot
    BNE star_tile_bolt
    LDAA #5
    RTS
star_tile_bolt:
    CMPB star_bolt
    BNE star_tile_board
    LDAA #6
    RTS
star_tile_board:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
star_enemies: .space 20
star_cells: .space 20
star_shields: .space 16
star_left: .space 1
star_lives: .space 1
star_running: .space 1
star_shot: .space 1
star_bolt: .space 1
star_offset: .space 1
star_base: .space 1
star_direction: .space 1
star_bounces: .space 1
star_march_clock: .space 1
star_fire_clock: .space 1
star_score: .space 2
star_steps: .space 1
star_clock: .space 1
star_pointer: .space 2
star_index: .space 1
star_pick: .space 1
star_view_index: .space 1
star_view_cell: .space 1
star_hud_force: .space 1
star_old_left: .space 1
star_old_lives: .space 1
star_old_score: .space 2
star_old_running: .space 1
.section .data, data
star_periods: .byte 8,6,4
star_left_label: .byte 76,69,70,84,0
star_life_label: .byte 76,73,70,69,0
star_score_label: .byte 83,67,79,82,69,0
star_menu_label: .byte 82,69,84,85,82,78,0
star_shoot_label: .byte 83,80,65,67,69,32,70,73,82,69,0
star_start_label: .byte 83,80,65,67,69,32,71,79,32,32,0
