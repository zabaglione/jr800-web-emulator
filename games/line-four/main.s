; SPDX-License-Identifier: MIT
.global four_result
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR four_result
    LDAA #3
    STAA cursor
    LDAA #42
    STAA grid_stat
    RTS
game_update:
    LDAA input_event
    BITA #16
    BNE four_play
    BITA #4
    BEQ four_right
    TST cursor
    BEQ four_idle
    DEC cursor
    JMP grid_changed
four_right:
    BITA #8
    BEQ four_idle
    LDAA cursor
    CMPA #6
    BEQ four_idle
    INC cursor
    JMP grid_changed
four_play:
    LDAB cursor
    JSR four_find
    CMPB #255
    BEQ four_idle
    STAB line_last
    JSR grid_snapshot
    LDAB line_last
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    DEC grid_stat
    JSR grid_count_move
    JSR line_score
    LDAA line_longest
    CMPA #4
    BCC four_human_wins
    TST grid_stat
    BEQ four_draw
    JSR four_cpu
    DEC grid_stat
    JSR line_score
    LDAA line_longest
    CMPA #4
    BCC four_cpu_wins
    TST grid_stat
    BEQ four_draw
four_idle:
    RTS
four_human_wins:
    LDAA #1
    STAA four_result
    LDAA #4
    STAA phase
    RTS
four_cpu_wins:
    LDAA #2
    STAA four_result
    LDAA #5
    STAA phase
    RTS
four_draw:
    LDAA #3
    STAA four_result
    LDAA #4
    STAA phase
    RTS
; B column -> B lowest empty cell, $FF if full.
four_find:
    ADDB #35
four_find_loop:
    LDX #board
    ABX
    TST 0,X
    BEQ four_find_done
    SUBB #7
    BPL four_find_loop
    LDAB #255
four_find_done:
    RTS
; Immediate wins, then blocks. Hard adds shape and central-column preference.
four_cpu:
    CLR four_column
    CLR four_best
    CLR four_best_cell
four_candidate:
    LDAB four_column
    JSR four_find
    CMPB #255
    BNE four_try
    JMP four_next_column
four_try:
    STAB four_candidate_cell
    STAB line_last
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    JSR line_score
    LDAA line_longest
    CMPA #4
    BCS four_not_win
    LDAA #255
    STAA four_rank
    JMP four_ranked
four_not_win:
    ASLA
    ASLA
    ASLA
    STAA four_rank
    TST stage
    BNE four_block
    JSR random
    ANDA #31
    INCA
    STAA four_rank
    JMP four_ranked
four_block:
    LDAB four_candidate_cell
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    JSR line_score
    LDAA line_longest
    CMPA #4
    BCS four_shape
    LDAA #224
    STAA four_rank
    BRA four_ranked
four_shape:
    LDAA stage
    CMPA #2
    BNE four_ranked
    LDAA line_longest
    ASLA
    ASLA
    ADDA four_rank
    STAA four_rank
    LDAB four_column
    LDX #four_weights
    ABX
    LDAA 0,X
    ADDA four_rank
    STAA four_rank
    ; Avoid creating a directly playable winning square above this piece.
    LDAB four_candidate_cell
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    SUBB #7
    BMI four_ranked
    STAB line_last
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    JSR line_score
    LDAB line_last
    LDX #board
    ABX
    CLR 0,X
    LDAA line_longest
    CMPA #4
    BCS four_ranked
    LDAA #1
    STAA four_rank
four_ranked:
    LDAB four_candidate_cell
    LDX #board
    ABX
    CLR 0,X
    LDAA four_rank
    CMPA four_best
    BLS four_next_column
    STAA four_best
    STAB four_best_cell
four_next_column:
    JSR input_poll
    INC four_column
    LDAA four_column
    CMPA #7
    BEQ four_chosen
    JMP four_candidate
four_chosen:
    LDAB four_best_cell
    STAB line_last
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    RTS
game_aux:
    CMPA #1
    BNE four_restart
    CLR four_result
    JMP grid_restore
four_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR grid_render
    LDX #four_drop_label
    LDAA four_result
    BEQ four_status
    LDX #four_win_label
    CMPA #1
    BEQ four_status
    LDX #four_lose_label
    CMPA #2
    BEQ four_status
    LDX #four_draw_label
four_status:
    LDAA #138
    LDAB #5
    TST four_result
    BEQ four_status_row
    LDAB #4
four_status_row:
    JMP paint_text
.section .bss, bss
four_result: .space 1
four_column: .space 1
four_candidate_cell: .space 1
four_best: .space 1
four_best_cell: .space 1
four_rank: .space 1
.section .data, data
four_drop_label: .byte 68,82,79,80,32,32,32,0
four_win_label: .byte 89,79,85,32,87,73,78,0
four_lose_label: .byte 67,80,85,32,87,73,78,0
four_draw_label: .byte 68,82,65,87,32,32,32,0
