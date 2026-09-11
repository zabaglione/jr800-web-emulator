; SPDX-License-Identifier: MIT
.global quest_hp
.global quest_keys
.global quest_pot
.global quest_left
.global quest_score
.global quest_aim
.global quest_counter
.global quest_attack
.global quest_heal
.section .text, code
game_start:
    JSR grid_reset
    LDAA #15
    STAA cursor
    LDAA #3
    STAA quest_aim
    LDAA #9
    STAA quest_hp
    LDAA #2
    STAA quest_left
    CLR quest_keys
    CLR quest_pot
    CLR quest_score
    CLR quest_score + 1
    LDAA stage
    LDAB #98
    MUL
    ADDD #quest_levels
    STD quest_pointer
    CLR quest_index
quest_load:
    LDX quest_pointer
    LDAA 0,X
    INX
    STX quest_pointer
    LDAB quest_index
    LDX #board
    ABX
    STAA 0,X
    INC quest_index
    LDAA quest_index
    CMPA #98
    BNE quest_load
    RTS
game_aux:
    CMPA #1
    BEQ quest_heal
    JMP game_start
quest_heal:
    TST quest_pot
    BEQ quest_no_heal
    LDAA quest_hp
    CMPA #9
    BEQ quest_no_heal
    ADDA #3
    CMPA #9
    BLS quest_heal_value
    LDAA #9
quest_heal_value:
    STAA quest_hp
    DEC quest_pot
    JMP quest_counter
quest_no_heal:
    RTS
game_update:
    CLR resume_pending
    LDAA input_event
    BITA #1
    BEQ quest_down
    CLR quest_aim
    BRA quest_move
quest_down:
    BITA #2
    BEQ quest_left_input
    LDAA #1
    STAA quest_aim
    BRA quest_move
quest_left_input:
    BITA #4
    BEQ quest_right_input
    LDAA #2
    STAA quest_aim
    BRA quest_move
quest_right_input:
    BITA #8
    BEQ quest_action
    LDAA #3
    STAA quest_aim
quest_move:
    LDAA #1
    STAA redraw
    JSR grid_move
    CMPB #255
    BNE quest_neighbor_ok
    RTS
quest_neighbor_ok:
    STAB quest_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ quest_done
    CMPA #7
    BCC quest_done
    CMPA #3
    BNE quest_enter
    TST quest_keys
    BEQ quest_done
    DEC quest_keys
    CLR 0,X
quest_enter:
    STAB cursor
    CMPA #2
    BNE quest_tonic
    INC quest_keys
    CLR 0,X
quest_tonic:
    CMPA #4
    BNE quest_relay
    INC quest_pot
    CLR 0,X
quest_relay:
    CMPA #5
    BNE quest_exit
    CLR 0,X
    DEC quest_left
    LDD quest_score
    ADDD #100
    STD quest_score
    BRA quest_turn
quest_exit:
    CMPA #6
    BNE quest_turn
    TST quest_left
    BNE quest_turn
    LDAA quest_hp
    LDAB #10
    MUL
    ADDD quest_score
    STD quest_score
    LDAA #4
    STAA phase
    RTS
quest_turn:
    JMP quest_counter
quest_action:
    BITA #16
    BEQ quest_done
    JMP quest_attack
quest_done:
    RTS
quest_attack:
    LDAB quest_aim
    LDX #quest_delta
    ABX
    LDAA 0,X
    ADDA cursor
    TAB
    CMPB #98
    BCC quest_done
    LDX #board
    ABX
    LDAA 0,X
    CMPA #7
    BCS quest_done
    CMPA #9
    BEQ quest_wound
    CLR 0,X
    LDD quest_score
    ADDD #25
    STD quest_score
    BRA quest_counter
quest_wound:
    LDAA #7
    STAA 0,X
quest_counter:
    JSR grid_count_move
    CLR quest_index
quest_guard:
    LDAB quest_index
    LDX #quest_delta
    ABX
    LDAA 0,X
    ADDA cursor
    TAB
    CMPB #98
    BCC quest_next_guard
    LDX #board
    ABX
    LDAA 0,X
    CMPA #7
    BCS quest_next_guard
    DEC quest_hp
    BNE quest_next_guard
    LDAA #5
    STAA phase
    RTS
quest_next_guard:
    INC quest_index
    LDAA quest_index
    CMPA #4
    BNE quest_guard
    RTS
grid_value:
    CMPB cursor
    BNE quest_map_tile
    LDAA quest_aim
    ADDA #10
    RTS
quest_map_tile:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    CLR quest_hud_force
    TST resume_pending
    BEQ quest_values
    INC quest_hud_force
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
    LDX #quest_hp_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #quest_keys_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #quest_pot_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #quest_left_label
    LDAA #132
    LDAB #7
    JSR paint_text
quest_values:
    TST quest_hud_force
    BNE quest_hp_value
    LDAA quest_hp
    CMPA quest_old_hp
    BEQ quest_keys_check
quest_hp_value:
    LDAA quest_hp
    STAA quest_old_hp
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA quest_hp
    JSR paint_number
quest_keys_check:
    TST quest_hud_force
    BNE quest_keys_value
    LDAA quest_keys
    CMPA quest_old_keys
    BEQ quest_pot_check
quest_keys_value:
    LDAA quest_keys
    STAA quest_old_keys
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA quest_keys
    JSR paint_number
quest_pot_check:
    TST quest_hud_force
    BNE quest_pot_value
    LDAA quest_pot
    CMPA quest_old_pot
    BEQ quest_left_check
quest_pot_value:
    LDAA quest_pot
    STAA quest_old_pot
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA quest_pot
    JSR paint_number
quest_left_check:
    TST quest_hud_force
    BNE quest_left_value
    LDAA quest_left
    CMPA quest_old_left
    BEQ quest_score_check
quest_left_value:
    LDAA quest_left
    STAA quest_old_left
    LDAA #168
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDAA quest_left
    JSR paint_number
quest_score_check:
    TST quest_hud_force
    BNE quest_score_value
    LDD quest_score
    SUBD quest_old_score
    BEQ quest_render_done
quest_score_value:
    LDD quest_score
    STD quest_old_score
    LDAA #90
    STAA paint_x
    CLR paint_band
    LDD quest_score
    JMP paint_number16
quest_render_done:
    RTS
.section .bss, bss
quest_hp: .space 1
quest_keys: .space 1
quest_pot: .space 1
quest_left: .space 1
quest_score: .space 2
quest_aim: .space 1
quest_pointer: .space 2
quest_index: .space 1
quest_next: .space 1
quest_hud_force: .space 1
quest_old_hp: .space 1
quest_old_keys: .space 1
quest_old_pot: .space 1
quest_old_left: .space 1
quest_old_score: .space 2
.section .data, data
quest_delta: .byte 242,14,255,1
quest_hp_label: .byte 72,80,0
quest_keys_label: .byte 75,69,89,83,0
quest_pot_label: .byte 84,79,78,73,67,0
quest_left_label: .byte 76,69,70,84,0
