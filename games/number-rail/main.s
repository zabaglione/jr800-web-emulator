; SPDX-License-Identifier: MIT
.global rail_active
.global rail_anim_step
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
    CLR rail_active
    CLR rail_score
    CLR rail_score + 1
    CLR rail_moves
    CLR rail_moves + 1
    LDAA #255
    STAA cursor
    JSR challenge_start
    ; The entire destination stays highlighted, including empty/moving tiles.
    LDAA #255
    STAA challenge_view_cells
    STAA challenge_view_cells + 1
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
    TST rail_active
    BEQ rail_controls
    LDAA input_ticks
    TST resume_pending
    BEQ rail_clock_ready
    STAA rail_clock
    CLR resume_pending
rail_clock_ready:
    SUBA rail_clock
    CMPA #3
    BCC rail_tick
    RTS
rail_tick:
    LDAA input_ticks
    STAA rail_clock
    JMP rail_anim_step
rail_controls:
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
    LDX #rail_merged
    LDAB #16
    CLRA
rail_clear_merged:
    STAA 0,X
    INX
    DECB
    BNE rail_clear_merged
    LDAA #1
    STAA rail_active
    LDAA input_ticks
    STAA rail_clock
    JMP grid_changed
rail_anim_commit:
    CLR rail_active
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
; One edgeward cell per tile per tick. Flags prevent a newly merged tile
; from merging again during this action. The independently computed trial
; is committed only after the visible movement has settled.
rail_anim_step:
    CLR rail_changed_tick
    LDAA rail_direction
    LDAB #16
    MUL
    ADDD #rail_lines
    STD rail_pointer
    CLR rail_line
rail_anim_line:
    LDAA #1
    STAA rail_index
rail_anim_cell:
    LDX rail_pointer
    LDAB rail_index
    ABX
    LDAA 0,X
    STAA rail_cell
    DEX
    LDAA 0,X
    STAA rail_dest
    LDAB rail_cell
    LDX #board
    ABX
    LDAA 0,X
    STAA rail_value
    BEQ rail_anim_next
    LDAB rail_dest
    LDX #board
    ABX
    LDAB 0,X
    BEQ rail_anim_move
    CBA
    BNE rail_anim_next
    CMPA #11
    BCC rail_anim_next
    LDAB rail_cell
    LDX #rail_merged
    ABX
    TST 0,X
    BNE rail_anim_next
    LDAB rail_dest
    LDX #rail_merged
    ABX
    TST 0,X
    BNE rail_anim_next
    INC rail_value
    LDAA #1
    STAA 0,X
    BRA rail_anim_store
rail_anim_move:
    LDAB rail_cell
    LDX #rail_merged
    ABX
    LDAA 0,X
    CLR 0,X
    LDAB rail_dest
    LDX #rail_merged
    ABX
    STAA 0,X
rail_anim_store:
    LDAB rail_cell
    LDX #board
    ABX
    CLR 0,X
    LDAB rail_dest
    LDX #board
    ABX
    LDAA rail_value
    STAA 0,X
    INC rail_changed_tick
rail_anim_next:
    INC rail_index
    LDAA rail_index
    CMPA #4
    BEQ rail_anim_next_line
    JMP rail_anim_cell
rail_anim_next_line:
    LDX rail_pointer
    INX
    INX
    INX
    INX
    STX rail_pointer
    INC rail_line
    LDAA rail_line
    CMPA #4
    BEQ rail_anim_done
    JMP rail_anim_line
rail_anim_done:
    TST rail_changed_tick
    BNE rail_anim_draw
    JMP rail_anim_commit
rail_anim_draw:
    JMP grid_changed
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
    JSR game_bonus
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
    CLR rail_active
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
    CLR paint_index
rail_render_tile:
    LDAB paint_index
    JSR game_tile
    TSTA
    BEQ rail_render_face
    LDAB grid_cell
    CMPB challenge_cells
    BNE rail_render_face
    ORAA #128
rail_render_face:
    LDAB paint_index
    JSR paint_tile
    INC paint_index
    LDAA paint_index
    ANDA #7
    BNE rail_render_next
    JSR input_poll
rail_render_next:
    LDAA paint_index
    CMPA #112
    BNE rail_render_tile
    JSR visual_hud
    ; Outside the board, a pointer locates the fixed destination. On completion
    ; both it and the legend become checks; the number itself stays unobscured.
    LDAA challenge_cells
    ANDA #3
    ASLA
    ASLA
    ASLA
    ASLA
    ASLA
    ADDA #VIEW_X + 12
    STAA paint_x
    LDAA #1
    LDX #rail_hint_down
    LDAB challenge_cells
    CMPB #4
    BCS rail_hint_position
    LDAA #6
    LDX #rail_hint_up
rail_hint_position:
    STAA paint_band
    TST challenge_bonus
    BEQ rail_hint_source
    LDX #rail_hint_check
rail_hint_source:
    STX paint_source
    JSR paint_address
    CLR paint_id
    LDAA #8
    STAA paint_count
    JSR paint_blit
    LDAA #7
    STAA paint_band
    LDAA #70
    STAA paint_x
    JSR paint_address
    LDX #rail_bonus_label
    STX paint_source
    LDAA #20
    STAA paint_count
    JSR paint_blit
    ; Reuse the exact tile artwork for the required number in the legend.
    LDAB stage
    LDX #rail_goals
    ABX
    LDAA 0,X
    LDAB #32
    MUL
    ADDD #tiles + 8
    STD paint_source
    LDAA #97
    STAA paint_x
    JSR paint_address
    LDAA #128
    STAA paint_id
    LDAA #32
    STAA paint_count
    JSR paint_blit
    CLR paint_id
    LDAA #136
    STAA paint_x
    JSR paint_address
    LDX #rail_hint_pending
    TST challenge_bonus
    BEQ rail_legend_source
    LDX #rail_hint_check
rail_legend_source:
    STX paint_source
    LDAA #8
    STAA paint_count
    JMP paint_blit

.section .bss, bss
rail_active: .space 1
rail_clock: .space 1
rail_dest: .space 1
rail_changed_tick: .space 1
rail_merged: .space 16
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
