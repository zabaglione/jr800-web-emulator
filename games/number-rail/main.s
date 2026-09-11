; SPDX-License-Identifier: MIT
.global rail_score
.global rail_moves
.global rail_delta
.global rail_row
.global rail_merge
.global rail_direction
.global rail_slide
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR rail_score
    CLR rail_score + 1
    CLR rail_moves
    CLR rail_moves + 1
    LDAA #255
    STAA cursor
    JSR challenge_start
    LDX #board
    JSR challenge_load
    LDAB #7
rail_expand:
    LDX #board
    ABX
    LDAA 0,X
    PSHA
    ASLB
    INCB
    LDX #board
    ABX
    ANDA #15
    STAA 0,X
    PULA
    LSRA
    LSRA
    LSRA
    LSRA
    DEX
    STAA 0,X
    LSRB
    DECB
    BPL rail_expand
    LDAB stage
    LDX #rail_seeds
    ABX
    LDAA 0,X
    STAA seed
    JMP rail_status
game_update:
    LDAA input_event
    CLR rail_direction
    BITA #1
    BNE rail_slide
    INC rail_direction
    BITA #2
    BNE rail_slide
    INC rail_direction
    BITA #4
    BNE rail_slide
    INC rail_direction
    BITA #8
    BNE rail_slide
    RTS
rail_slide:
    CLR rail_delta
    CLR rail_delta + 1
    LDAA rail_direction
    LDAB #16
    MUL
    ADDD #rail_lines
    STD rail_pointer
    CLR rail_line
rail_read_line:
    CLR rail_index
rail_read_cell:
    LDX rail_pointer
    LDAB rail_index
    ABX
    LDAB 0,X
    LDX #board
    ABX
    LDAA 0,X
    LDAB rail_index
    LDX #rail_row
    ABX
    STAA 0,X
    INC rail_index
    LDAA rail_index
    CMPA #4
    BNE rail_read_cell
    JSR rail_merge
    CLR rail_index
rail_write_cell:
    LDX rail_pointer
    LDAB rail_index
    ABX
    LDAA 0,X
    STAA rail_cell
    LDX #rail_row
    ABX
    LDAA 0,X
    LDAB rail_cell
    LDX #rail_trial
    ABX
    STAA 0,X
    INC rail_index
    LDAA rail_index
    CMPA #4
    BNE rail_write_cell
    LDX rail_pointer
    INX
    INX
    INX
    INX
    STX rail_pointer
    INC rail_line
    LDAA rail_line
    CMPA #4
    BNE rail_read_line
    LDAB #0
rail_compare:
    LDX #board
    ABX
    LDAA 0,X
    LDX #rail_trial
    ABX
    CMPA 0,X
    BNE rail_changed
    INCB
    CMPB #16
    BNE rail_compare
    RTS
rail_changed:
    JSR grid_snapshot
    LDD rail_score
    STD rail_undo_score
    ADDD rail_delta
    BCC rail_score_ok
    LDD #$FFFF
rail_score_ok:
    STD rail_score
    LDD rail_moves
    STD rail_undo_moves
    SUBD #9999
    BCC rail_moves_capped
    LDD rail_moves
    ADDD #1
    STD rail_moves
rail_moves_capped:
    LDAA seed
    STAA rail_undo_seed
    LDAB #0
rail_commit:
    LDX #rail_trial
    ABX
    LDAA 0,X
    LDX #board
    ABX
    STAA 0,X
    INCB
    CMPB #16
    BNE rail_commit
    JSR rail_spawn
    JSR grid_count_move
    JMP rail_status
; Compress four powers, merge each original tile at most once, accumulate points.
rail_merge:
    CLR rail_index
    CLR rail_count
rail_pack:
    LDAB rail_index
    LDX #rail_row
    ABX
    LDAA 0,X
    BEQ rail_pack_next
    LDAB rail_count
    LDX #rail_compact
    ABX
    STAA 0,X
    INC rail_count
rail_pack_next:
    INC rail_index
    LDAA rail_index
    CMPA #4
    BNE rail_pack
    LDX #rail_row
    CLR 0,X
    CLR 1,X
    CLR 2,X
    CLR 3,X
    CLR rail_index
    CLR rail_output
rail_merge_next:
    LDAB rail_index
    CMPB rail_count
    BEQ rail_merge_done
    LDX #rail_compact
    ABX
    LDAA 0,X
    STAA rail_value
    INCB
    CMPB rail_count
    BEQ rail_single
    CMPA #11
    BCC rail_single
    CMPA 1,X
    BNE rail_single
    INC rail_index
    INC rail_value
    LDAB rail_value
    ASLB
    LDX #rail_values
    ABX
    LDD 0,X
    ADDD rail_delta
    STD rail_delta
rail_single:
    LDAB rail_output
    LDX #rail_row
    ABX
    LDAA rail_value
    STAA 0,X
    INC rail_index
    INC rail_output
    BRA rail_merge_next
rail_merge_done:
    RTS
rail_spawn:
    JSR random
    ANDA #15
    TAB
    LDX #board
    ABX
    TST 0,X
    BNE rail_spawn
    STAB rail_cell
    JSR random
    ANDA #15
    BNE rail_spawn_two
    LDAA #2
    BRA rail_spawn_value
rail_spawn_two:
    LDAA #1
rail_spawn_value:
    LDAB rail_cell
    LDX #board
    ABX
    STAA 0,X
    RTS
rail_status:
    CLR grid_stat
    CLR rail_empty
    LDAB #0
rail_highest:
    LDX #board
    ABX
    LDAA 0,X
    BNE rail_peak
    INC rail_empty
rail_peak:
    CMPA grid_stat
    BLS rail_highest_next
    STAA grid_stat
rail_highest_next:
    INCB
    CMPB #16
    BNE rail_highest
    LDAB stage
    LDX #rail_goals
    ABX
    LDAA grid_stat
    CMPA 0,X
    BCS rail_check_space
    LDAA #4
    STAA phase
    RTS
rail_check_space:
    TST rail_empty
    BNE rail_available
    CLR rail_cell
rail_pair:
    LDAB rail_cell
    LDX #board
    ABX
    LDAA 0,X
    STAA rail_value
    LDX #neighbors + 16
    ABX
    LDAB 0,X
    CMPB #255
    BEQ rail_pair_right
    LDX #board
    ABX
    LDAA 0,X
    CMPA rail_value
    BEQ rail_available
rail_pair_right:
    LDAB rail_cell
    LDX #neighbors + 48
    ABX
    LDAB 0,X
    CMPB #255
    BEQ rail_pair_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA rail_value
    BEQ rail_available
rail_pair_next:
    INC rail_cell
    LDAA rail_cell
    CMPA #16
    BNE rail_pair
    LDAA #5
    STAA phase
rail_available:
    RTS
game_aux:
    CMPA #1
    BNE rail_restart
    TST undo_valid
    BEQ rail_available
    JSR grid_restore
    LDD rail_undo_score
    STD rail_score
    LDD rail_undo_moves
    STD rail_moves
    LDAA rail_undo_seed
    STAA seed
    JMP rail_status
rail_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_bonus:
    CLR challenge_bonus
    LDAB stage
    LDX #rail_goals
    ABX
    LDAA 0,X
    LDAB challenge_cells
    LDX #board
    ABX
    CMPA 0,X
    BHI rail_bonus_done
    INC challenge_bonus
rail_bonus_done:
    RTS
game_render:
    JSR paint_board
    JMP visual_hud

.section .bss, bss
rail_score: .space 2
rail_moves: .space 2
rail_delta: .space 2
rail_undo_score: .space 2
rail_undo_moves: .space 2
rail_undo_seed: .space 1
rail_direction: .space 1
rail_pointer: .space 2
rail_trial: .space 16
rail_row: .space 4
rail_compact: .space 4
rail_count: .space 1
rail_index: .space 1
rail_output: .space 1
rail_line: .space 1
rail_cell: .space 1
rail_value: .space 1
rail_empty: .space 1
rail_display: .space 2
rail_digit: .space 2
.section .data, data
rail_score_label: .byte 83,0
rail_goal_label: .byte 71,79,65,76,32,0
