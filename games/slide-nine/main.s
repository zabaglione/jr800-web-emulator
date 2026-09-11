; SPDX-License-Identifier: MIT
.global undo_valid
.section .text, code
game_render:
    JSR paint_board
    JMP visual_hud

game_start:
    JSR grid_reset
    JSR challenge_start
    CLR undo_valid
    LDX #board
    JSR challenge_load
    LDAB stage
    LDX #slide_tokens
    ABX
    LDAA 0,X
    STAA slide_token
    LDX #board
    CLRB
slide_find_blank:
    TST 0,X
    BEQ slide_found_blank
    INX
    INCB
    BRA slide_find_blank
slide_found_blank:
    STAB cursor
    JMP slide_count
game_update:
    JSR grid_move
    CMPB #255
    BEQ slide_idle
    LDAA cursor
    STAA slide_last
    LDAA moves
    STAA slide_saved_moves
    LDAA #1
    STAA undo_valid
    STAB slide_pending
    JSR challenge_step
    LDAB slide_pending
    JSR slide_swap
    LDAB slide_last
    LDX #board
    ABX
    LDAA 0,X
    CMPA slide_token
    BNE slide_not_stamped
    JSR challenge_touch
slide_not_stamped:
    JSR grid_count_move
    JSR slide_count
    TST grid_stat
    BNE slide_idle
    LDAA #4
    STAA phase
slide_idle:
    RTS
game_aux:
    CMPA #1
    BNE slide_restart
    TST undo_valid
    BEQ slide_idle
    JSR challenge_undo
    LDAB slide_last
    JSR slide_swap
    CLR undo_valid
    LDAA slide_saved_moves
    STAA moves
    JMP slide_count
slide_restart:
    JMP game_start
slide_swap:
    STAB slide_target
    LDX #board
    ABX
    LDAA 0,X
    CLR 0,X
    LDX #board
    LDAB cursor
    ABX
    STAA 0,X
    LDAA slide_target
    STAA cursor
    RTS
slide_count:
    CLR grid_stat
    LDX #board
    LDAB #1
slide_count_loop:
    LDAA 0,X
    BEQ slide_count_next
    CBA
    BEQ slide_count_next
    INC grid_stat
slide_count_next:
    INX
    INCB
    CMPB #10
    BNE slide_count_loop
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
.section .bss, bss
slide_source: .space 2
slide_index: .space 1
slide_last: .space 1
slide_target: .space 1
slide_saved_moves: .space 1
undo_valid: .space 1

.global slide_token
.section .bss, bss
slide_token: .space 1
slide_pending: .space 1
.section .text, code
game_bonus:
    RTS
