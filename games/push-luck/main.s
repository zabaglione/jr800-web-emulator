; SPDX-License-Identifier: MIT
.global luck_scores
.global luck_pot
.global luck_face
.global luck_side
.global luck_rolls
.global luck_trail
.global luck_delay
.global luck_message
.global luck_throw
.global luck_bank
.global luck_decide
.section .text, code
game_start:
    CLR luck_scores
    CLR luck_scores + 1
    CLR luck_pot
    CLR luck_face
    CLR luck_side
    CLR luck_rolls
    CLR luck_delay
    CLR luck_message
    LDX #luck_trail
    LDAB #6
luck_clear_trail:
    CLR 0,X
    INX
    DECB
    BNE luck_clear_trail
    RTS
game_update:
    TST luck_side
    BNE luck_cpu_update
    LDAA input_event
    BITA #16
    BEQ luck_idle
    JSR luck_throw
    JMP luck_changed
luck_cpu_update:
    DEC luck_delay
    BNE luck_idle
    LDAA #36
    STAA luck_delay
    JSR luck_decide
    TSTA
    BNE luck_cpu_bank
    JSR luck_throw
    JMP luck_changed
luck_cpu_bank:
    JSR luck_bank
    JMP luck_changed
luck_idle:
    RTS
game_aux:
    TST luck_side
    BNE luck_idle
    CMPA #1
    BNE luck_reset
    JSR luck_bank
    JMP luck_changed
luck_reset:
    JSR game_start
luck_changed:
    LDAA #1
    STAA redraw
    RTS
luck_random:
    JSR random
    CMPA #253
    BCC luck_random
    DECA
luck_mod6:
    CMPA #6
    BCS luck_face_ready
    SUBA #6
    BRA luck_mod6
luck_face_ready:
    INCA
    RTS
luck_throw:
    JSR luck_random
    STAA luck_face
    TST luck_side
    BEQ luck_human_roll
    LDAB luck_rolls
    CMPB #6
    BCC luck_human_roll
    LDX #luck_trail
    ABX
    STAA 0,X
    INC luck_rolls
luck_human_roll:
    LDAA luck_face
    CMPA #1
    BEQ luck_bust
    ADDA luck_pot
    BCC luck_keep_pot
    LDAA #255
luck_keep_pot:
    STAA luck_pot
    LDAA #1
    STAA luck_message
    RTS
luck_bust:
    CLR luck_pot
    LDAA #2
    STAA luck_message
    JMP luck_next
luck_bank:
    TST luck_pot
    BEQ luck_bank_done
    LDAB luck_side
    LDX #luck_scores
    ABX
    LDAA luck_pot
    ADDA 0,X
    BCC luck_store_bank
    LDAA #255
luck_store_bank:
    STAA 0,X
    CLR luck_pot
    LDAA #3
    STAA luck_message
    LDAA 0,X
    CMPA #100
    BCS luck_next
    LDAA #4
    TST luck_side
    BEQ luck_winner
    INCA
luck_winner:
    STAA phase
luck_bank_done:
    RTS
luck_next:
    LDAA luck_side
    EORA #1
    STAA luck_side
    BEQ luck_next_done
    LDAA #36
    STAA luck_delay
    CLR luck_rolls
    LDX #luck_trail
    LDAB #6
luck_erase_cpu:
    CLR 0,X
    INX
    DECB
    BNE luck_erase_cpu
luck_next_done:
    RTS
; A=1 to bank, 0 to continue. No random look-ahead.
luck_decide:
    TST luck_pot
    BEQ luck_continue
    LDAA luck_pot
    ADDA luck_scores + 1
    BCS luck_stop
    CMPA #100
    BCC luck_stop
    LDAA luck_rolls
    CMPA #6
    BCC luck_stop
    LDAB stage
    LDX #luck_limits
    ABX
    LDAA 0,X
    STAA luck_limit
    CMPB #2
    BNE luck_threshold
    LDAA luck_scores
    CMPA #80
    BCS luck_threshold
    LDAA #32
    STAA luck_limit
luck_threshold:
    LDAA luck_pot
    CMPA luck_limit
    BCC luck_stop
luck_continue:
    CLRA
    RTS
luck_stop:
    LDAA #1
    RTS
; Draw the last 24x16 die and the six CPU throws as a compact text history.
game_tile:
    STAB luck_cell
    TBA
    LSRA
    LSRA
    LSRA
    LSRA
    CMPA #1
    BCS luck_tile_blank
    CMPA #3
    BCC luck_tile_blank
    DECA
    LDAB #3
    MUL
    STAB luck_sub
    LDAA luck_cell
    ANDA #15
    CMPA #5
    BCS luck_tile_blank
    CMPA #8
    BCC luck_tile_blank
    SUBA #5
    ADDA luck_sub
    STAA luck_sub
    LDAA luck_face
    LDAB #6
    MUL
    ADDB luck_sub
    INCB
    TBA
    RTS
luck_tile_blank:
    CLRA
    RTS
game_render:
    JSR paint_board
    LDX #luck_turn_human
    TST luck_side
    BEQ luck_turn_text
    LDX #luck_turn_cpu
luck_turn_text:
    CLRA
    LDAB #1
    JSR hud_play_text
    LDX #luck_pot_label
    CLRA
    LDAB #2
    JSR hud_play_text
    CLR paint_x
    LDAA #3
    STAA paint_band
    LDAA luck_pot
    JSR paint_number
    LDAA luck_message
    LDAB #7
    MUL
    ADDD #luck_messages
    XGDX
    CLRA
    LDAB #4
    JSR hud_play_text
    LDX #luck_cpu_label
    CLRA
    LDAB #5
    JSR hud_play_text
    CLR luck_index
luck_trail_text:
    LDAB luck_index
    LDX #luck_trail
    ABX
    LDAA 0,X
    BEQ luck_trail_empty
    ADDA #48
    BRA luck_trail_char
luck_trail_empty:
    LDAA #45
luck_trail_char:
    LDAB luck_index
    ASLB
    LDX #luck_trail_line
    ABX
    STAA 0,X
    INC luck_index
    LDAA luck_index
    CMPA #6
    BNE luck_trail_text
    LDX #luck_trail_line
    CLRA
    LDAB #6
    JSR hud_play_text
    LDX #luck_action_label
    LDAA phase
    CMPA #2
    BNE luck_action_text
    TST luck_side
    BEQ luck_action_text
    LDX #luck_wait_label
luck_action_text:
    CLRA
    LDAB #7
    JSR hud_play_text
    JMP visual_hud
.section .bss, bss
luck_scores: .space 2
luck_pot: .space 1
luck_face: .space 1
luck_side: .space 1
luck_rolls: .space 1
luck_trail: .space 6
luck_delay: .space 1
luck_message: .space 1
luck_limit: .space 1
luck_index: .space 1
luck_cell: .space 1
luck_sub: .space 1
.section .data, data
luck_trail_line: .byte 45,32,45,32,45,32,45,32,45,32,45,32,0
luck_turn_human: .byte 89,79,85,82,32,84,85,82,78,0
luck_turn_cpu: .byte 67,80,85,32,84,85,82,78,32,0
luck_pot_label: .byte 80,79,84,0
luck_cpu_label: .byte 67,80,85,32,82,79,76,76,83,0
luck_action_label: .byte 83,80,65,67,69,58,82,79,76,76,32,32,0
luck_wait_label: .byte 67,80,85,32,84,72,73,78,75,73,78,71,0
luck_you_label: .byte 89,79,85,0
luck_cpu_short: .byte 67,80,85,0
luck_goal_label: .byte 71,79,65,76,0
luck_return_label: .byte 82,69,84,85,82,78,0
luck_messages: .byte 82,69,65,68,89,32,0,82,79,76,76,32,32,0,66,85,83,84,32,32,0,66,65,78,75,32,32,0
