; SPDX-License-Identifier: MIT
.global rogue_floor
.global rogue_room
.global rogue_hp
.global rogue_max_hp
.global rogue_sword
.global rogue_armor
.global rogue_pot
.global rogue_relic
.global rogue_left
.global rogue_score
.global rogue_positions
.global rogue_health
.global rogue_distance
.global rogue_trace
.global rogue_enemies
.global rogue_heal
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    CLR rogue_floor
    CLR rogue_score
    CLR rogue_score + 1
    CLR rogue_armor
    LDAA #2
    STAA rogue_sword
    LDAA #1
    STAA rogue_pot
    LDAA stage
    ASLA
    ASLA
    STAA rogue_temp
    LDAA #24
    SUBA rogue_temp
    STAA rogue_hp
    STAA rogue_max_hp
rogue_load_room:
    JSR grid_reset
    CLR rogue_relic
    CLR rogue_actor
    LDAA #3
    STAA rogue_left
    LDAA #15
    STAA cursor
    JSR random
rogue_mod_room:
    CMPA #12
    BCS rogue_room_ready
    SUBA #12
    BRA rogue_mod_room
rogue_room_ready:
    STAA rogue_room
    LDAB #98
    MUL
    ADDD #rogue_rooms
    STD rogue_pointer
    CLR rogue_index
rogue_load:
    LDX rogue_pointer
    LDAA 0,X
    INX
    STX rogue_pointer
    CMPA #7
    BNE rogue_store
    LDAB rogue_actor
    LDX #rogue_positions
    ABX
    LDAA rogue_index
    STAA 0,X
    LDX #rogue_health
    ABX
    LDAA rogue_floor
    ADDA #2
    TST stage
    BEQ rogue_set_enemy_hp
    INCA
rogue_set_enemy_hp:
    STAA 0,X
    INC rogue_actor
    CLRA
rogue_store:
    LDAB rogue_index
    LDX #board
    ABX
    STAA 0,X
    INC rogue_index
    LDAA rogue_index
    CMPA #98
    BNE rogue_load
    LDAA rogue_floor
    CMPA #4
    BNE rogue_load_done
    LDAA #10
    ADDA stage
    STAA rogue_health
rogue_load_done:
    RTS
game_aux:
    CMPA #1
    BNE rogue_restart
    JMP rogue_turn
rogue_restart:
    JMP game_start
game_update:
    CLR resume_pending
    LDAA input_event
    BITA #16
    BEQ rogue_move
    JMP rogue_heal
rogue_move:
    JSR grid_move
    CMPB #255
    BNE rogue_valid_cell
    RTS
rogue_valid_cell:
    STAB rogue_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE rogue_find_enemy
    RTS
rogue_find_enemy:
    JSR rogue_find
    CMPA #255
    BEQ rogue_enter
    TAB
    LDX #rogue_health
    ABX
    LDAA 0,X
    SUBA rogue_sword
    BLS rogue_kill
    STAA 0,X
    JMP rogue_turn
rogue_kill:
    CLR 0,X
    DEC rogue_left
    LDD rogue_score
    ADDD #20
    STD rogue_score
    JMP rogue_turn
rogue_enter:
    LDAB rogue_next
    STAB cursor
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BNE rogue_shield
    CLR 0,X
    LDAA rogue_sword
    CMPA #5
    BCC rogue_enter_turn
    INC rogue_sword
    BRA rogue_enter_turn
rogue_shield:
    CMPA #3
    BNE rogue_medicine
    CLR 0,X
    LDAA rogue_armor
    CMPA #2
    BCC rogue_enter_turn
    INC rogue_armor
    BRA rogue_enter_turn
rogue_medicine:
    CMPA #4
    BNE rogue_gem
    LDAA rogue_pot
    CMPA #5
    BCC rogue_enter_turn
    INC rogue_pot
    CLR 0,X
    BRA rogue_enter_turn
rogue_gem:
    CMPA #5
    BNE rogue_coin
    CLR 0,X
    INC rogue_relic
    LDD rogue_score
    ADDD #100
    STD rogue_score
    BRA rogue_enter_turn
rogue_coin:
    CMPA #9
    BNE rogue_stairs
    CLR 0,X
    LDD rogue_score
    ADDD #10
    STD rogue_score
    BRA rogue_enter_turn
rogue_stairs:
    CMPA #6
    BNE rogue_enter_turn
    TST rogue_relic
    BEQ rogue_enter_turn
    TST rogue_left
    BNE rogue_enter_turn
    LDAA rogue_floor
    CMPA #4
    BEQ rogue_win
    INC rogue_floor
    JSR rogue_load_room
    JSR input_gate
    JSR paint_clear
    LDAA #1
    STAA resume_pending
    STAA redraw
    RTS
rogue_enter_turn:
    JMP rogue_turn
rogue_win:
    LDAA rogue_hp
    LDAB #5
    MUL
    ADDD rogue_score
    STD rogue_score
    LDAA #4
    STAA phase
    LDAA #1
    STAA redraw
    RTS
rogue_heal:
    TST rogue_pot
    BEQ rogue_heal_done
    LDAA rogue_hp
    CMPA rogue_max_hp
    BEQ rogue_heal_done
    ADDA #8
    CMPA rogue_max_hp
    BLS rogue_heal_set
    LDAA rogue_max_hp
rogue_heal_set:
    STAA rogue_hp
    DEC rogue_pot
    BRA rogue_turn
rogue_heal_done:
    RTS
rogue_turn:
    JSR grid_count_move
    JMP rogue_enemies
; B = cell, A = living enemy index or 255.
rogue_find:
    STAB rogue_query
    CLR rogue_scan
rogue_find_loop:
    LDAB rogue_scan
    LDX #rogue_health
    ABX
    TST 0,X
    BEQ rogue_find_next
    LDX #rogue_positions
    ABX
    LDAA 0,X
    CMPA rogue_query
    BEQ rogue_found
rogue_find_next:
    INC rogue_scan
    LDAA rogue_scan
    CMPA #3
    BNE rogue_find_loop
    LDAA #255
    RTS
rogue_found:
    LDAA rogue_scan
    RTS
; A = cell, B = direction; B = checked neighbour or 255.
rogue_neighbor:
    STAA rogue_neighbor_cell
    TBA
    LDAB #98
    MUL
    ADDD #neighbors
    XGDX
    LDAB rogue_neighbor_cell
    ABX
    LDAB 0,X
    RTS
rogue_trace:
    LDX #rogue_distance
    LDAA #255
    LDAB #98
rogue_trace_clear:
    STAA 0,X
    INX
    DECB
    BNE rogue_trace_clear
    CLR rogue_head
    LDAA #1
    STAA rogue_tail
    LDAB cursor
    STAB rogue_queue
    LDX #rogue_distance
    ABX
    CLR 0,X
rogue_trace_pop:
    LDAB rogue_head
    LDX #rogue_queue
    ABX
    LDAB 0,X
    STAB rogue_trace_cell
    LDX #rogue_distance
    ABX
    LDAA 0,X
    INCA
    STAA rogue_depth
    CLR rogue_direction
rogue_trace_neighbor:
    LDAA rogue_trace_cell
    LDAB rogue_direction
    JSR rogue_neighbor
    CMPB #255
    BEQ rogue_trace_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ rogue_trace_next
    LDX #rogue_distance
    ABX
    LDAA 0,X
    CMPA #255
    BNE rogue_trace_next
    LDAA rogue_depth
    STAA 0,X
    STAB rogue_option
    LDAB rogue_tail
    LDX #rogue_queue
    ABX
    LDAA rogue_option
    STAA 0,X
    INC rogue_tail
rogue_trace_next:
    INC rogue_direction
    LDAA rogue_direction
    CMPA #4
    BNE rogue_trace_neighbor
    INC rogue_head
    LDAA rogue_head
    ANDA #7
    BNE rogue_trace_more
    JSR input_poll
rogue_trace_more:
    LDAA rogue_head
    CMPA rogue_tail
    BNE rogue_trace_pop
    RTS
rogue_enemies:
    JSR rogue_trace
    CLR rogue_actor
rogue_enemy:
    LDAB rogue_actor
    LDX #rogue_health
    ABX
    TST 0,X
    BNE rogue_enemy_alive
    JMP rogue_enemy_next
rogue_enemy_alive:
    LDX #rogue_positions
    ABX
    LDAB 0,X
    STAB rogue_origin
    STAB rogue_best
    LDX #rogue_distance
    ABX
    LDAA 0,X
    CMPA #7
    BCS rogue_enemy_awake
    JMP rogue_enemy_next
rogue_enemy_awake:
    CMPA #1
    BEQ rogue_damage
    STAA rogue_best_distance
    CLR rogue_direction
rogue_enemy_option:
    LDAA rogue_origin
    LDAB rogue_direction
    JSR rogue_neighbor
    CMPB #255
    BEQ rogue_enemy_option_next
    STAB rogue_option
    LDX #rogue_distance
    ABX
    LDAA 0,X
    CMPA rogue_best_distance
    BCC rogue_enemy_option_next
    STAA rogue_option_distance
    JSR rogue_find
    CMPA #255
    BNE rogue_enemy_option_next
    LDAA rogue_option
    STAA rogue_best
    LDAA rogue_option_distance
    STAA rogue_best_distance
rogue_enemy_option_next:
    INC rogue_direction
    LDAA rogue_direction
    CMPA #4
    BNE rogue_enemy_option
    LDAB rogue_actor
    LDX #rogue_positions
    ABX
    LDAA rogue_best
    STAA 0,X
    BRA rogue_enemy_next
rogue_damage:
    LDAA rogue_floor
    LSRA
    ADDA #2
    TST rogue_actor
    BNE rogue_damage_armor
    LDAB rogue_floor
    CMPB #4
    BNE rogue_damage_armor
    LDAA #5
rogue_damage_armor:
    SUBA rogue_armor
    BHI rogue_damage_value
    LDAA #1
rogue_damage_value:
    STAA rogue_temp
    LDAA rogue_hp
    CMPA rogue_temp
    BLS rogue_dead
    SUBA rogue_temp
    STAA rogue_hp
rogue_enemy_next:
    INC rogue_actor
    LDAA rogue_actor
    CMPA #3
    BEQ rogue_enemies_done
    JMP rogue_enemy
rogue_enemies_done:
    RTS
rogue_dead:
    CLR rogue_hp
    LDAA #5
    STAA phase
    RTS
grid_value:
    CMPB cursor
    BNE rogue_tile_enemy
    LDAA #11
    RTS
rogue_tile_enemy:
    STAB rogue_tile_cell
    JSR rogue_find
    CMPA #255
    BEQ rogue_tile_board
    TAB
    LDX #rogue_health
    ABX
    LDAA 0,X
    CMPA #4
    BCS rogue_tile_small
    LDAA #9
    RTS
rogue_tile_small:
    CMPA #2
    BCS rogue_tile_weak
    LDAA #8
    RTS
rogue_tile_weak:
    LDAA #7
    RTS
rogue_tile_board:
    LDAB rogue_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    CMPA #9
    BNE rogue_tile_exit
    LDAA #10
    RTS
rogue_tile_exit:
    CMPA #6
    BNE rogue_tile_done
    TST rogue_left
    BNE rogue_tile_done
    TST rogue_relic
    BEQ rogue_tile_done
    LDAA #12
rogue_tile_done:
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    RTS
actor_render_scene:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
rogue_floor: .space 1
rogue_room: .space 1
rogue_hp: .space 1
rogue_max_hp: .space 1
rogue_sword: .space 1
rogue_armor: .space 1
rogue_pot: .space 1
rogue_left: .space 1
rogue_relic: .space 1
rogue_score: .space 2
rogue_positions: .space 3
rogue_health: .space 3
rogue_distance: .space 98
rogue_queue: .space 98
rogue_head: .space 1
rogue_tail: .space 1
rogue_depth: .space 1
rogue_trace_cell: .space 1
rogue_direction: .space 1
rogue_option: .space 1
rogue_origin: .space 1
rogue_best: .space 1
rogue_best_distance: .space 1
rogue_option_distance: .space 1
rogue_neighbor_cell: .space 1
rogue_query: .space 1
rogue_scan: .space 1
rogue_actor: .space 1
rogue_pointer: .space 2
rogue_index: .space 1
rogue_temp: .space 1
rogue_next: .space 1
rogue_tile_cell: .space 1
rogue_hud_force: .space 1
rogue_old_score: .space 2
rogue_old_hp: .space 1
rogue_old_sword: .space 1
rogue_old_armor: .space 1
rogue_old_pot: .space 1
rogue_old_left: .space 1
rogue_old_floor: .space 1
.section .data, data
rogue_hp_label: .byte 72,80,0
rogue_gear_label: .byte 71,69,65,82,0
rogue_pot_label: .byte 84,79,78,73,67,0
rogue_left_label: .byte 70,79,69,83,0

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    CLRB
    LDX #view_cells
actor_intro_find:
    LDAA 0,X
    CMPA cursor
    BEQ actor_intro_found
    INX
    INCB
    CMPB #112
    BNE actor_intro_find
actor_intro_found:
    TBA
    ANDA #15
    ASLA
    ASLA
    ASLA
    ADDA #VIEW_X
    STAA paint_x
    TBA
    LSRA
    LSRA
    LSRA
    LSRA
    INCA
    STAA paint_band
    LDAA paint_x
    STAA actor_intro_x
    LDAA paint_band
    STAA actor_intro_band
    CLR actor_intro_step
    CLR actor_intro_bytes
    CLR actor_intro_bytes + 1
actor_intro_blink:
    LDAA actor_intro_band
    STAA paint_band
    LDAA actor_intro_x
    STAA paint_x
    JSR paint_address
    LDAA #1
    STAA actor_intro_rows
actor_intro_row:
    LDX paint_dest
    LDAB #8
actor_intro_pixels:
    COM 0,X
    INX
    DECB
    BNE actor_intro_pixels
    LDAA paint_band
    LDAB actor_intro_x
    JSR dirty_mark
    LDAA paint_band
    LDAB actor_intro_x
    ADDB #7
    JSR dirty_mark
    LDD paint_dest
    ADDD #192
    STD paint_dest
    INC paint_band
    DEC actor_intro_rows
    BNE actor_intro_row
    JSR dirty_begin
actor_intro_transfer:
    JSR dirty_next
    BEQ actor_intro_sum
    JSR input_poll
    BRA actor_intro_transfer
actor_intro_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
actor_intro_frame:
    TST actor_intro_step
    BNE actor_intro_wait
    LDX #180
    LDD #20
    JSR sound_tone
actor_intro_wait:
    LDAA input_ticks
    STAA actor_intro_clock
actor_intro_delay:
    JSR input_poll
    LDAA input_ticks
    SUBA actor_intro_clock
    CMPA #8
    BCS actor_intro_delay
    INC actor_intro_step
    LDAA actor_intro_step
    CMPA #4
    BEQ actor_intro_done
    JMP actor_intro_blink
actor_intro_done:
    JSR input_gate
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD actor_intro_bytes
    STD dirty_bytes
    ; Finish the complete shell frame for both stage starts and menu resets.
    LDS #$5FFF
    JMP frame_ready
.section .bss, bss
actor_intro_pending: .space 1
actor_intro_x: .space 1
actor_intro_band: .space 1
actor_intro_step: .space 1
actor_intro_clock: .space 1
actor_intro_rows: .space 1
actor_intro_bytes: .space 2
