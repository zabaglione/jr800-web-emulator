; SPDX-License-Identifier: MIT
.global dice_holds
.global dice_rolls
.global dice_used
.global dice_saved
.global dice_scores
.global dice_total
.global dice_upper
.global dice_bonus
.global dice_round
.global dice_category
.global dice_evaluate
.global dice_roll
.global dice_commit
.section .text, code
game_start:
    JSR grid_reset
    LDX #dice_used
    LDAB #26
    CLRA
dice_zero_scores:
    STAA 0,X
    INX
    DECB
    BNE dice_zero_scores
    CLR dice_total
    CLR dice_total + 1
    CLR dice_upper
    CLR dice_bonus
    CLR dice_round
    CLR dice_category
    JMP dice_new_round
game_update:
    TST selection_active
    BNE dice_select_score
    LDAA input_event
    BITA #16
    BEQ dice_cursor
    LDAB cursor
    LDX #dice_holds
    ABX
    LDAA 0,X
    EORA #1
    STAA 0,X
    JMP grid_changed
dice_cursor:
    JSR grid_move
    CMPB #255
    BEQ dice_idle
    STAB cursor
    JMP grid_changed
dice_select_score:
    LDAA input_event
    BITA #16
    BNE dice_select_commit
    BITA #5
    BEQ dice_category_next
    TST dice_category
    BEQ dice_category_last
    DEC dice_category
    JMP grid_changed
dice_category_last:
    LDAA #12
    STAA dice_category
    JMP grid_changed
dice_category_next:
    BITA #10
    BEQ dice_idle
    INC dice_category
    LDAA dice_category
    CMPA #13
    BCS dice_category_changed
    CLR dice_category
dice_category_changed:
    JMP grid_changed
dice_select_commit:
    JSR dice_commit
    JMP grid_changed
dice_idle:
    RTS
game_aux:
    CMPA #1
    BNE dice_open_scores
    JSR dice_roll
    JMP grid_changed
dice_open_scores:
    LDAA #1
    STAA selection_active
    JMP grid_changed
dice_new_round:
    LDX #dice_holds
    LDAB #5
    CLRA
dice_unhold:
    STAA 0,X
    INX
    DECB
    BNE dice_unhold
    LDAA #3
    STAA dice_rolls
dice_roll:
    TST dice_rolls
    BEQ dice_roll_done
    CLR dice_index
dice_roll_loop:
    LDAB dice_index
    LDX #dice_holds
    ABX
    TST 0,X
    BNE dice_roll_next
dice_random:
    JSR random
    CMPA #253
    BCC dice_random
    DECA
dice_mod6:
    CMPA #6
    BCS dice_face
    SUBA #6
    BRA dice_mod6
dice_face:
    INCA
    LDAB dice_index
    LDX #board
    ABX
    STAA 0,X
dice_roll_next:
    INC dice_index
    LDAA dice_index
    CMPA #5
    BNE dice_roll_loop
    DEC dice_rolls
    JMP dice_evaluate
dice_roll_done:
    RTS
; Cached category scores are rebuilt only after dice change.
dice_evaluate:
    LDX #dice_counts
    LDAB #7
    CLRA
dice_counts_clear:
    STAA 0,X
    INX
    DECB
    BNE dice_counts_clear
    CLR dice_sum
    CLR dice_index
dice_count_loop:
    LDAB dice_index
    LDX #board
    ABX
    LDAB 0,X
    ADDB dice_sum
    STAB dice_sum
    LDAB 0,X
    LDX #dice_counts
    ABX
    INC 0,X
    INC dice_index
    LDAA dice_index
    CMPA #5
    BNE dice_count_loop
    CLR dice_max
    CLR dice_two
    CLR dice_three
    CLR dice_run
    CLR dice_longest
    LDAA #1
    STAA dice_index
dice_rank_loop:
    LDAB dice_index
    LDX #dice_counts
    ABX
    LDAA 0,X
    CMPA dice_max
    BLS dice_not_max
    STAA dice_max
dice_not_max:
    CMPA #2
    BNE dice_not_two
    INC dice_two
dice_not_two:
    CMPA #3
    BNE dice_not_three
    INC dice_three
dice_not_three:
    TSTA
    BEQ dice_run_zero
    INC dice_run
    LDAB dice_run
    CMPB dice_longest
    BLS dice_run_ready
    STAB dice_longest
    BRA dice_run_ready
dice_run_zero:
    CLR dice_run
dice_run_ready:
    LDAB dice_index
    MUL
    TBA
    LDAB dice_index
    DECB
    LDX #dice_scores
    ABX
    STAA 0,X
    INC dice_index
    LDAA dice_index
    CMPA #7
    BNE dice_rank_loop
    LDX #dice_scores + 6
    LDAB #7
    CLRA
dice_lower_clear:
    STAA 0,X
    INX
    DECB
    BNE dice_lower_clear
    LDAA dice_sum
    STAA dice_scores + 12
    LDAB dice_max
    CMPB #3
    BCS dice_check_house
    STAA dice_scores + 6
    CMPB #4
    BCS dice_check_house
    STAA dice_scores + 7
    CMPB #5
    BNE dice_check_house
    LDAA #50
    STAA dice_scores + 11
dice_check_house:
    TST dice_two
    BEQ dice_check_straight
    TST dice_three
    BEQ dice_check_straight
    LDAA #25
    STAA dice_scores + 8
dice_check_straight:
    LDAA dice_longest
    CMPA #4
    BCS dice_eval_done
    LDAA #30
    STAA dice_scores + 9
    LDAA dice_longest
    CMPA #5
    BCS dice_eval_done
    LDAA #40
    STAA dice_scores + 10
dice_eval_done:
    RTS
dice_commit:
    LDAB dice_category
    LDX #dice_used
    ABX
    TST 0,X
    BNE dice_commit_done
    LDAA #1
    STAA 0,X
    LDX #dice_scores
    ABX
    LDAA 0,X
    STAA dice_points
    LDX #dice_saved
    ABX
    STAA 0,X
    CMPB #6
    BCC dice_add_score
    ADDA dice_upper
    STAA dice_upper
dice_add_score:
    LDD dice_total
    ADDB dice_points
    ADCA #0
    STD dice_total
    TST dice_bonus
    BNE dice_round_end
    LDAA dice_upper
    CMPA #63
    BCS dice_round_end
    LDAA #35
    STAA dice_bonus
    LDD dice_total
    ADDD #35
    STD dice_total
dice_round_end:
    CLR selection_active
    INC dice_round
    LDAA dice_round
    CMPA #13
    BCC dice_finish
    JSR dice_new_round
    JMP paint_clear
dice_finish:
    LDAA stage
    ASLA
    TAB
    LDX #dice_goals
    ABX
    LDD dice_total
    SUBD 0,X
    BCS dice_failed
    LDAA #4
    BRA dice_result
dice_failed:
    LDAA #5
dice_result:
    STAA phase
    JMP paint_clear
dice_commit_done:
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    LDX #dice_holds
    ABX
    TST 0,X
    BEQ dice_sprite_done
    ADDA #6
dice_sprite_done:
    RTS
game_render:
    TST selection_active
    BNE dice_render_scores
    JSR paint_board
    LDX #dice_hold_label
    CLRA
    LDAB #1
    JSR hud_play_text
    LDX #dice_roll_label
    CLRA
    LDAB #4
    JSR hud_play_text
    LDAA #36
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA dice_rolls
    JSR paint_number
    LDX #dice_bonus_label
    CLRA
    LDAB #5
    JSR hud_play_text
    LDAA #36
    STAA paint_x
    LDAA #5
    STAA paint_band
    LDAA dice_bonus
    JSR paint_number
    LDX #dice_menu_label
    CLRA
    LDAB #7
    JSR hud_play_text
    JMP dice_render_hud
dice_render_scores:
    LDX #dice_choose_label
    CLRA
    LDAB #1
    JSR hud_play_text
    LDAA dice_category
    CLRB
dice_page:
    CMPA #5
    BCS dice_page_ready
    SUBA #5
    ADDB #5
    BRA dice_page
dice_page_ready:
    STAB dice_display
    LDAA #2
    STAA dice_row
dice_display_loop:
    LDX #dice_line
    LDAB #16
    LDAA #32
dice_line_clear:
    STAA 0,X
    INX
    DECB
    BNE dice_line_clear
    LDAA dice_display
    CMPA #13
    BCS dice_line_label
    JMP dice_line_paint
dice_line_label:
    LDAB #7
    MUL
    ADDD #dice_labels
    XGDX
    LDD 0,X
    STD dice_line + 2
    LDD 2,X
    STD dice_line + 4
    LDD 4,X
    STD dice_line + 6
    LDAA dice_display
    CMPA dice_category
    BNE dice_line_used
    LDAA #62
    STAA dice_line
dice_line_used:
    LDAB dice_display
    LDX #dice_used
    ABX
    TST 0,X
    BEQ dice_line_preview
    LDAA #88
    STAA dice_line + 9
    LDX #dice_saved
    BRA dice_line_points
dice_line_preview:
    LDX #dice_scores
dice_line_points:
    ABX
    LDAA 0,X
    LDAB #48
dice_line_hundreds:
    CMPA #100
    BCS dice_line_tens_start
    SUBA #100
    INCB
    BRA dice_line_hundreds
dice_line_tens_start:
    STAB dice_line + 12
    LDAB #48
dice_line_tens:
    CMPA #10
    BCS dice_line_ones
    SUBA #10
    INCB
    BRA dice_line_tens
dice_line_ones:
    STAB dice_line + 13
    ADDA #48
    STAA dice_line + 14
dice_line_paint:
    LDX #dice_line
    CLRA
    LDAB dice_row
    JSR hud_play_text
dice_display_next:
    INC dice_display
    INC dice_row
    LDAA dice_row
    CMPA #7
    BEQ dice_display_complete
    JMP dice_display_loop
dice_display_complete:
    LDX #dice_confirm_label
    CLRA
    LDAB #7
    JSR hud_play_text
dice_render_hud:
    JMP visual_hud
.section .bss, bss
dice_holds: .space 5
dice_rolls: .space 1
dice_used: .space 13
dice_saved: .space 13
dice_scores: .space 13
dice_total: .space 2
dice_upper: .space 1
dice_bonus: .space 1
dice_round: .space 1
dice_category: .space 1
dice_counts: .space 7
dice_sum: .space 1
dice_index: .space 1
dice_max: .space 1
dice_two: .space 1
dice_three: .space 1
dice_run: .space 1
dice_longest: .space 1
dice_points: .space 1
dice_display: .space 1
dice_row: .space 1
.section .data, data
dice_hold_label: .byte 83,80,65,67,69,32,84,79,32,72,79,76,68,0
dice_roll_label: .byte 82,79,76,76,83,0
dice_bonus_label: .byte 66,79,78,85,83,0
dice_menu_label: .byte 77,69,78,85,58,82,79,76,76,47,83,67,79,82,69,0
dice_choose_label: .byte 67,72,79,79,83,69,32,65,32,82,79,87,0
dice_confirm_label: .byte 83,80,65,67,69,32,84,79,32,83,67,79,82,69,0
dice_total_label: .byte 84,79,84,65,76,0
dice_goal_label: .byte 71,79,65,76,0
dice_left_label: .byte 76,69,70,84,0
dice_line: .byte 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0
