; SPDX-License-Identifier: MIT
.global five_result
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR five_result
    LDAA #48
    STAA cursor
    LDAA #98
    STAA grid_stat
    RTS
game_update:
    JSR cursor_blink_tick
    LDAA input_event
    BITA #16
    BNE five_play
    JSR grid_move
    CMPB #255
    BEQ five_idle
    STAB cursor
    JMP grid_changed
five_play:
    LDAB cursor
    LDX #board
    ABX
    TST 0,X
    BNE five_idle
    JSR grid_snapshot
    LDAB cursor
    STAB line_last
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    DEC grid_stat
    JSR grid_count_move
    JSR line_score
    LDAA line_longest
    CMPA #5
    BCC five_win
    TST grid_stat
    BEQ five_draw
    JSR five_cpu
    DEC grid_stat
    JSR line_score
    LDAA line_longest
    CMPA #5
    BCC five_lose
    TST grid_stat
    BEQ five_draw
five_idle:
    RTS
five_win:
    LDAA #1
    STAA five_result
    LDAA #4
    STAA phase
    RTS
five_lose:
    LDAA #2
    STAA five_result
    LDAA #5
    STAA phase
    RTS
five_draw:
    LDAA #3
    STAA five_result
    LDAA #4
    STAA phase
    RTS
five_cpu:
    CLR five_scan
    CLR five_best
five_candidate:
    LDAB five_scan
    LDX #board
    ABX
    TST 0,X
    BEQ five_try
    JMP five_next
five_try:
    STAB line_last
    LDAA #2
    STAA 0,X
    JSR line_score
    LDAA line_longest
    CMPA #5
    BCS five_not_win
    LDAA #255
    STAA five_rank
    BRA five_ranked
five_not_win:
    LDAB #3
    MUL
    ADDB line_open
    LDX #five_attack
    ABX
    LDAA 0,X
    STAA five_rank
    LDAB five_scan
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    JSR line_score
    LDAA line_longest
    CMPA #5
    BCS five_shape
    LDAA #240
    STAA five_rank
    BRA five_ranked
five_shape:
    LDAB #3
    MUL
    ADDB line_open
    LDX #five_defense
    ABX
    LDAA 0,X
    CMPA #128
    BCC five_priority
    LDAB five_rank
    CMPB #128
    BCC five_weight
    ADDA five_rank
    STAA five_rank
    BRA five_weight
five_priority:
    CMPA five_rank
    BLS five_weight
    STAA five_rank
five_weight:
    LDAB five_scan
    LDX #five_weights
    ABX
    LDAA 0,X
    ADDA five_rank
    STAA five_rank
five_ranked:
    LDAB five_scan
    LDX #board
    ABX
    CLR 0,X
    LDAA five_rank
    CMPA five_best
    BLS five_next
    STAA five_best
    STAB five_best_cell
five_next:
    JSR input_poll
    INC five_scan
    LDAA five_scan
    CMPA #98
    BEQ five_cpu_place
    JMP five_candidate
five_cpu_place:
    LDAB five_best_cell
    STAB line_last
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    RTS
game_aux:
    CMPA #1
    BNE five_restart
    CLR five_result
    JMP grid_restore
five_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR cursor_blink_prepare
    JSR cursor_blink_board
    JMP visual_hud

.section .bss, bss
five_result: .space 1
five_scan: .space 1
five_best: .space 1
five_best_cell: .space 1
five_rank: .space 1
.section .data, data
five_win_label: .byte 89,79,85,32,87,73,78,0
five_lose_label: .byte 67,80,85,32,87,73,78,0
five_draw_label: .byte 68,82,65,87,32,32,32,0
five_attack: .byte 0,0,0,8,8,8,16,16,16,24,24,140,32,225,230
five_defense: .byte 0,0,0,6,6,6,12,12,12,18,18,130,24,150,220

; The JR-800 input timer drives focus blinking; input immediately restores it.
.global cursor_blink_mask
.global cursor_blink_clock
.section .text, code
cursor_blink_prepare:
    TST hud_ready
    BEQ cursor_blink_show
    RTS
cursor_blink_tick:
    TST input_event
    BNE cursor_blink_show
    LDAA input_ticks
    SUBA cursor_blink_clock
    CMPA #25
    BCS cursor_blink_idle
    LDAA cursor_blink_mask
    EORA #128
    BRA cursor_blink_store
cursor_blink_show:
    LDAA #128
cursor_blink_store:
    CMPA cursor_blink_mask
    BEQ cursor_blink_time
    STAA cursor_blink_mask
    LDAA #1
    STAA redraw
cursor_blink_time:
    LDAA input_ticks
    STAA cursor_blink_clock
cursor_blink_idle:
    RTS
cursor_blink_board:
    LDAA cursor
    PSHA
    TST cursor_blink_mask
    BNE cursor_blink_paint
    LDAA #255
    STAA cursor
cursor_blink_paint:
    JSR paint_board
    PULA
    STAA cursor
    RTS
.section .bss, bss
cursor_blink_mask: .space 1
cursor_blink_clock: .space 1
