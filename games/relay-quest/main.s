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
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
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
