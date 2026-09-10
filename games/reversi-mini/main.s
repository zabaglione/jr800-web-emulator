; SPDX-License-Identifier: MIT
.global rev_legal
.global rev_cpu_disks
.global rev_passed
.global rev_result
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR rev_result
    CLR rev_passed
    CLR rev_apply
    LDX #board
    LDAA #1
    STAA 14,X
    STAA 21,X
    LDAA #2
    STAA 15,X
    STAA 20,X
    LDAA #8
    STAA cursor
    JSR rev_count_disks
    LDAA #1
    JMP rev_find_moves
game_update:
    LDAA input_event
    BITA #16
    BEQ rev_cursor_move
    LDAB cursor
    LDX #rev_legal
    ABX
    TST 0,X
    BNE rev_play
    RTS
rev_cursor_move:
    JSR grid_move
    CMPB #255
    BEQ rev_idle
    STAB cursor
    JMP grid_changed
rev_idle:
    RTS
rev_play:
    JSR grid_snapshot
    CLR rev_passed
    LDAA #1
    STAA rev_who
    LDAB cursor
    JSR rev_place
    JSR grid_count_move
rev_cpu_turn:
    LDAA #2
    JSR rev_find_moves
    TST rev_legal_count
    BNE rev_cpu_plays
    LDAA #1
    JSR rev_find_moves
    TST rev_legal_count
    BEQ rev_finish
    LDAA #2
    STAA rev_passed
    JMP rev_count_disks
rev_cpu_plays:
    LDAA #2
    STAA rev_who
    LDAB rev_best_cell
    JSR rev_place
    LDAA #1
    JSR rev_find_moves
    TST rev_legal_count
    BNE rev_count_disks
    LDAA #1
    STAA rev_passed
    BRA rev_cpu_turn
rev_finish:
    JSR rev_count_disks
    LDAA grid_stat
    CMPA rev_cpu_disks
    BCS rev_lost
    BEQ rev_drawn
    LDAA #1
    STAA rev_result
    LDAA #4
    STAA phase
    RTS
rev_drawn:
    LDAA #3
    STAA rev_result
    LDAA #4
    STAA phase
    RTS
rev_lost:
    LDAA #2
    STAA rev_result
    LDAA #5
    STAA phase
    RTS
rev_count_disks:
    CLR grid_stat
    CLR rev_cpu_disks
    LDX #board
    LDAB #36
rev_count_loop:
    LDAA 0,X
    CMPA #1
    BNE rev_count_cpu
    INC grid_stat
    BRA rev_count_next
rev_count_cpu:
    CMPA #2
    BNE rev_count_next
    INC rev_cpu_disks
rev_count_next:
    INX
    DECB
    BNE rev_count_loop
    RTS
; A player -> all legal moves and a positional CPU choice. No board mutation.
rev_find_moves:
    STAA rev_who
    CLR rev_scan
    CLR rev_legal_count
    CLR rev_best_rank
    CLR rev_apply
rev_scan_loop:
    LDAB rev_scan
    JSR rev_measure
    LDAB rev_scan
    LDX #rev_legal
    ABX
    LDAA rev_total
    STAA 0,X
    BEQ rev_scan_next
    INC rev_legal_count
    ASLA
    ADDA #128
    STAA rev_rank
    LDX #rev_weights
    ABX
    LDAA 0,X
    ADDA rev_rank
    CMPA rev_best_rank
    BLS rev_scan_next
    STAA rev_best_rank
    STAB rev_best_cell
rev_scan_next:
    JSR input_poll
    INC rev_scan
    LDAA rev_scan
    CMPA #36
    BNE rev_scan_loop
    RTS
rev_place:
    LDAA #1
    STAA rev_apply
    JSR rev_measure
    CLR rev_apply
    RTS
; B candidate. Count (or flip) bounded opponent rays bracketed by this player.
rev_measure:
    STAB rev_cell
    CLR rev_total
    LDX #board
    ABX
    TST 0,X
    BEQ rev_measure_empty
    RTS
rev_measure_empty:
    CLR rev_dir
rev_direction:
    LDAA rev_dir
    LDAB #36
    MUL
    ADDD #rev_links
    STD rev_pointer
    LDAA rev_cell
    STAA rev_walk
    CLR rev_length
rev_ray:
    JSR rev_next
    CMPB #255
    BEQ rev_next_direction
    LDX #board
    ABX
    LDAA 0,X
    BEQ rev_next_direction
    CMPA rev_who
    BEQ rev_bracket
    INC rev_length
    BRA rev_ray
rev_bracket:
    LDAA rev_length
    ADDA rev_total
    STAA rev_total
    TST rev_apply
    BEQ rev_next_direction
    TST rev_length
    BEQ rev_next_direction
    LDAA rev_cell
    STAA rev_walk
rev_flip_ray:
    JSR rev_next
    LDX #board
    ABX
    LDAA rev_who
    STAA 0,X
    DEC rev_length
    BNE rev_flip_ray
rev_next_direction:
    INC rev_dir
    LDAA rev_dir
    CMPA #8
    BNE rev_direction
    TST rev_apply
    BEQ rev_measure_done
    TST rev_total
    BEQ rev_measure_done
    LDX #board
    LDAB rev_cell
    ABX
    LDAA rev_who
    STAA 0,X
rev_measure_done:
    RTS
rev_next:
    LDX rev_pointer
    LDAB rev_walk
    ABX
    LDAB 0,X
    STAB rev_walk
    RTS
game_aux:
    CMPA #1
    BNE rev_restart
    JSR grid_restore
    CLR rev_result
    CLR rev_passed
    JSR rev_count_disks
    LDAA #1
    JMP rev_find_moves
rev_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    BNE rev_tile_done
    LDX #rev_legal
    ABX
    TST 0,X
    BEQ rev_tile_done
    LDAA #3
rev_tile_done:
    RTS
game_render:
    JSR grid_render
    LDX #rev_cpu_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDAA #162
    STAA paint_x
    LDAA #5
    STAA paint_band
    LDAA rev_cpu_disks
    JSR paint_number
    LDAA rev_result
    BEQ rev_pass_label
    STAA rev_saved_result
    LDX #rev_blank_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDAA rev_saved_result
    LDX #rev_win_label
    CMPA #1
    BEQ rev_result_label
    LDX #rev_loss_label
    CMPA #2
    BEQ rev_result_label
    LDX #rev_draw_label
rev_result_label:
    LDAA #138
    LDAB #4
    JMP paint_text
rev_pass_label:
    LDAA rev_passed
    BNE rev_has_pass
    LDX #rev_space_label
    BRA rev_show_pass
rev_has_pass:
    LDX #rev_you_pass
    CMPA #1
    BEQ rev_show_pass
    LDX #rev_cpu_pass
rev_show_pass:
    LDAA #132
    LDAB #6
    JMP paint_text
rev_render_done:
    RTS
.section .bss, bss
rev_legal: .space 36
rev_legal_count: .space 1
rev_cpu_disks: .space 1
rev_passed: .space 1
rev_result: .space 1
rev_who: .space 1
rev_scan: .space 1
rev_best_cell: .space 1
rev_best_rank: .space 1
rev_rank: .space 1
rev_apply: .space 1
rev_cell: .space 1
rev_total: .space 1
rev_dir: .space 1
rev_pointer: .space 2
rev_walk: .space 1
rev_length: .space 1
rev_saved_result: .space 1
.section .data, data
rev_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
rev_space_label: .byte 32,83,80,65,67,69,32,32,0
rev_cpu_label: .byte 67,80,85,0
rev_win_label: .byte 89,79,85,32,87,73,78,0
rev_loss_label: .byte 67,80,85,32,87,73,78,0
rev_draw_label: .byte 68,82,65,87,32,32,32,0
rev_you_pass: .byte 89,79,85,32,80,65,83,83,0
rev_cpu_pass: .byte 67,80,85,32,80,65,83,83,0
