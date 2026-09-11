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
    JSR paint_board
    CLR rogue_hud_force
    TST resume_pending
    BEQ rogue_values
    INC rogue_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDX #rogue_hp_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #rogue_gear_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #rogue_pot_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #rogue_left_label
    LDAA #132
    LDAB #7
    JSR paint_text
rogue_values:
rogue_hp_check:
    TST rogue_hud_force
    BNE rogue_hp_value
    LDAA rogue_hp
    CMPA rogue_old_hp
    BEQ rogue_sword_check
rogue_hp_value:
    LDAA rogue_hp
    STAA rogue_old_hp
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA rogue_hp
    JSR paint_number
rogue_sword_check:
    TST rogue_hud_force
    BNE rogue_sword_value
    LDAA rogue_sword
    CMPA rogue_old_sword
    BEQ rogue_armor_check
rogue_sword_value:
    LDAA rogue_sword
    STAA rogue_old_sword
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA rogue_sword
    JSR paint_number
rogue_armor_check:
    TST rogue_hud_force
    BNE rogue_armor_value
    LDAA rogue_armor
    CMPA rogue_old_armor
    BEQ rogue_pot_check
rogue_armor_value:
    LDAA rogue_armor
    STAA rogue_old_armor
    LDAA #162
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA rogue_armor
    JSR paint_number
rogue_pot_check:
    TST rogue_hud_force
    BNE rogue_pot_value
    LDAA rogue_pot
    CMPA rogue_old_pot
    BEQ rogue_left_check
rogue_pot_value:
    LDAA rogue_pot
    STAA rogue_old_pot
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA rogue_pot
    JSR paint_number
rogue_left_check:
    TST rogue_hud_force
    BNE rogue_left_value
    LDAA rogue_left
    CMPA rogue_old_left
    BEQ rogue_floor_check
rogue_left_value:
    LDAA rogue_left
    STAA rogue_old_left
    LDAA #168
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDAA rogue_left
    JSR paint_number
rogue_floor_check:
    TST rogue_hud_force
    BNE rogue_floor_value
    LDAA rogue_floor
    CMPA rogue_old_floor
    BEQ rogue_score_check
rogue_floor_value:
    LDAA rogue_floor
    STAA rogue_old_floor
    LDAA #168
    STAA paint_x
    LDAA #0
    STAA paint_band
    LDAA rogue_floor
    INCA
    JSR paint_number
rogue_score_check:
    TST rogue_hud_force
    BNE rogue_score_value
    LDD rogue_score
    SUBD rogue_old_score
    BEQ rogue_render_done
rogue_score_value:
    LDD rogue_score
    STD rogue_old_score
    LDAA #84
    STAA paint_x
    CLR paint_band
    LDD rogue_score
    JMP paint_number16
rogue_render_done:
    RTS
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
