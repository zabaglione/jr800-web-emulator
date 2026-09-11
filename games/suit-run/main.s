; SPDX-License-Identifier: MIT
.global suit_stock_pos
.global suit_waste
.global suit_chain
.global suit_score
.global suit_available
.global suit_match
.global suit_check
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR suit_stock_pos
    CLR suit_chain
    CLR suit_score
    CLR suit_score + 1
    LDAA #35
    STAA grid_stat
    LDAA #31
    STAA cursor
    LDAA stage
    LDAB #52
    MUL
    ADDD #suit_deals
    STD suit_pointer
    CLR suit_index
suit_load:
    LDX suit_pointer
    LDAA 0,X
    INX
    STX suit_pointer
    LDAB suit_index
    LDX #board
    ABX
    STAA 0,X
    INC suit_index
    LDAA suit_index
    CMPA #35
    BNE suit_load
    LDX suit_pointer
    LDAA 0,X
    STAA suit_waste
    INX
    STX suit_stock
    RTS
game_update:
    LDAA input_event
    BITA #16
    BNE suit_choose
    JSR grid_move
    CMPB #255
    BEQ suit_idle
    STAB cursor
    JMP grid_changed
suit_choose:
    LDAB cursor
    JSR suit_available
    TSTA
    BEQ suit_idle
    STAA suit_rank
    JSR suit_match
    TSTA
    BEQ suit_idle
    JSR suit_snapshot
    LDAA suit_rank
    STAA suit_waste
    LDAB cursor
    LDX #board
    ABX
    CLR 0,X
    DEC grid_stat
    INC suit_chain
    LDD suit_score
    ADDB suit_chain
    ADCA #0
    STD suit_score
    JSR grid_count_move
    JMP suit_check
suit_idle:
    RTS
; A rank, zero if covered/removed.
suit_available:
    LDX #board
    ABX
    LDAA 0,X
    BEQ suit_idle
    CMPB #28
    BCC suit_idle
    TST 7,X
    BEQ suit_idle
    CLRA
    RTS
; A rank -> A 1 if adjacent to waste, wrapping A/K.
suit_match:
    SUBA suit_waste
    CMPA #1
    BEQ suit_yes
    CMPA #255
    BEQ suit_yes
    CMPA #12
    BEQ suit_yes
    CMPA #244
    BEQ suit_yes
    CLRA
    RTS
suit_yes:
    LDAA #1
    RTS
suit_snapshot:
    LDAA suit_stock_pos
    STAA suit_undo_pos
    LDAA suit_waste
    STAA suit_undo_waste
    LDAA suit_chain
    STAA suit_undo_chain
    LDD suit_score
    STD suit_undo_score
    JMP grid_snapshot
game_aux:
    CMPA #1
    BNE suit_undo
    LDAA suit_stock_pos
    CMPA #16
    BCC suit_aux_done
    JSR suit_snapshot
    LDAB suit_stock_pos
    LDX suit_stock
    ABX
    LDAA 0,X
    STAA suit_waste
    INC suit_stock_pos
    CLR suit_chain
    JSR grid_count_move
    JMP suit_check
suit_undo:
    TST undo_valid
    BEQ suit_aux_done
    JSR grid_restore
    LDAA suit_undo_pos
    STAA suit_stock_pos
    LDAA suit_undo_waste
    STAA suit_waste
    LDAA suit_undo_chain
    STAA suit_chain
    LDD suit_undo_score
    STD suit_score
    JMP suit_check
suit_aux_done:
    RTS
suit_check:
    TST grid_stat
    BNE suit_check_stock
    LDAA #4
    STAA phase
    RTS
suit_check_stock:
    LDAA suit_stock_pos
    CMPA #16
    BCS suit_check_done
    CLR suit_index
suit_find:
    LDAB suit_index
    JSR suit_available
    TSTA
    BEQ suit_next
    JSR suit_match
    TSTA
    BNE suit_check_done
suit_next:
    INC suit_index
    LDAA suit_index
    CMPA #35
    BNE suit_find
    LDAA #5
    STAA phase
suit_check_done:
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    STAA suit_display
    JSR suit_available
    TSTA
    BEQ suit_covered
    ADDA #13
    RTS
suit_covered:
    LDAA suit_display
    RTS
game_render:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
suit_stock: .space 2
suit_stock_pos: .space 1
suit_waste: .space 1
suit_chain: .space 1
suit_score: .space 2
suit_pointer: .space 2
suit_index: .space 1
suit_rank: .space 1
suit_display: .space 1
suit_undo_pos: .space 1
suit_undo_waste: .space 1
suit_undo_chain: .space 1
suit_undo_score: .space 2
.section .data, data
suit_rank_labels: .byte 45,65,50,51,52,53,54,55,56,57,84,74,81,75
suit_waste_text: .byte 91,45,93,0
suit_score_label: .byte 83,67,79,82,69,0
suit_left_label: .byte 76,69,70,84,0
suit_waste_label: .byte 87,65,83,84,69,0
suit_deck_label: .byte 68,69,67,75,0
suit_chain_label: .byte 67,72,65,73,78,0
