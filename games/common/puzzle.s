; SPDX-License-Identifier: MIT
; Optional campaign code, linked only into authored puzzle programs.
.global challenge_moves
.global challenge_par
.global challenge_bonus
.global challenge_need
.global challenge_best
.global challenge_rating
.global challenge_overflow
.global password_digits
.global password_length
.global password_cursor
.global password_error
.global password_text
.global challenge_encode
.global challenge_validate
.global challenge_award
.global challenge_step
.global challenge_undo
.global challenge_touch
.section .text, code
challenge_init:
    LDX #challenge_best
    LDAB #STAGES
    CLRA
challenge_clear_scores:
    STAA 0,X
    INX
    DECB
    BNE challenge_clear_scores
    CLR challenge_choice
    CLR challenge_restored
    RTS
challenge_start:
    CLR challenge_moves
    CLR challenge_moves + 1
    CLR challenge_overflow
    CLR challenge_bonus
    CLR challenge_undo_bonus
    CLR challenge_rating
    LDAB stage
    LDX #challenge_needs
    ABX
    LDAA 0,X
    STAA challenge_need
    LDAB stage
    ASLB
    LDX #challenge_pars
    ABX
    LDD 0,X
    STD challenge_par
    LDAB stage
    ASLB
    LDX #challenge_targets
    ABX
    LDD 0,X
    STD challenge_cells
    LDAB stage
    ASLB
    LDX #challenge_tiles
    ABX
    LDD 0,X
    STD challenge_view_cells
    RTS
; Every valid action costs one. Undo restores its optional pickups, but costs a turn.
challenge_step:
    LDAA challenge_bonus
    STAA challenge_undo_bonus
challenge_cost:
    LDD challenge_moves
    SUBD #9999
    BCC challenge_cost_capped
    LDD challenge_moves
    ADDD #1
    STD challenge_moves
    RTS
challenge_cost_capped:
    LDAA #1
    STAA challenge_overflow
    RTS
challenge_undo:
    LDAA challenge_undo_bonus
    STAA challenge_bonus
    JSR challenge_invalidate
    JMP challenge_cost
; B = logical cell. Preserve registers so a slide/laser can report intermediate cells.
challenge_touch:
    PSHA
    PSHB
    PSHX
    STAB challenge_touch_cell
    CMPB #255
    BEQ challenge_touch_done
    CMPB challenge_cells
    BNE challenge_touch_second
    LDAA challenge_bonus
    BITA #1
    BNE challenge_touch_second
    ORAA #1
    STAA challenge_bonus
    JSR challenge_invalidate
challenge_touch_second:
    LDAB challenge_touch_cell
    CMPB challenge_cells + 1
    BNE challenge_touch_done
    LDAA challenge_bonus
    BITA #2
    BNE challenge_touch_done
    ORAA #2
    STAA challenge_bonus
    JSR challenge_invalidate
challenge_touch_done:
    PULX
    PULB
    PULA
    RTS
challenge_invalidate:
    LDAB challenge_view_cells
    CMPB #255
    BEQ challenge_invalidate_second
    LDX #tile_cache
    ABX
    LDAA #255
    STAA 0,X
challenge_invalidate_second:
    LDAB challenge_view_cells + 1
    CMPB #255
    BEQ challenge_invalidate_done
    LDX #tile_cache
    ABX
    LDAA #255
    STAA 0,X
challenge_invalidate_done:
    RTS
; Small contrasting diamond in the corner of an optional target's first tile.
challenge_mark_tile:
; @if ice_route
    LDAA ice_reveal
    CMPA #3
    BCS challenge_mark_done
; @endif
    LDAB paint_cell
    CMPB challenge_view_cells
    BNE challenge_mark_second
    LDAA challenge_bonus
    BITA #1
    BEQ challenge_mark_draw
challenge_mark_second:
; @if ice_route
    LDAA ice_reveal
    CMPA #4
    BCS challenge_mark_done
; @endif
    CMPB challenge_view_cells + 1
    BNE challenge_mark_done
    LDAA challenge_bonus
    BITA #2
    BNE challenge_mark_done
challenge_mark_draw:
    LDAA paint_cell
    ANDA #15
    ASLA
    ASLA
    ASLA
; @if ice_route
    ADDA #VIEW_X + 1
; @else
    ADDA #VIEW_X + 5
; @endif
    STAA paint_x
    JSR paint_address
    LDX paint_dest
; @if ice_route
    ; Five-column star on clear ice, distinct from the hollow diamond gems.
    LDAA 0,X
    EORA #$14
    STAA 0,X
    LDAA 1,X
    EORA #$08
    STAA 1,X
    LDAA 2,X
    EORA #$3E
    STAA 2,X
    LDAA 3,X
    EORA #$08
    STAA 3,X
    LDAA 4,X
    EORA #$14
    STAA 4,X
; @else
    LDAA 0,X
    EORA #$40
    STAA 0,X
    LDAA 1,X
    EORA #$E0
    STAA 1,X
    LDAA 2,X
    EORA #$40
    STAA 2,X
; @endif
    LDAA paint_band
    LDAB paint_x
    JSR dirty_mark
    LDAA paint_band
    LDAB paint_x
; @if ice_route
    ADDB #4
; @else
    ADDB #2
; @endif
    JMP dirty_mark
challenge_mark_done:
    RTS
challenge_award:
    JSR game_bonus
    LDAA #1
    STAA challenge_rating
    TST challenge_overflow
    BNE challenge_keep_best
    LDD challenge_moves
    SUBD challenge_par
    BHI challenge_keep_best
    INC challenge_rating
    LDAA challenge_bonus
    ANDA challenge_need
    CMPA challenge_need
    BNE challenge_keep_best
    INC challenge_rating
challenge_keep_best:
    LDAB stage
    LDX #challenge_best
    ABX
    LDAA challenge_rating
    CMPA 0,X
    BLS challenge_awarded
    STAA 0,X
challenge_awarded:
    RTS
; X = destination. Every compressed stage record has a build-verified size.
challenge_load:
    STX unpack_dest
    LDAB stage
    ASLB
    LDX #level_ptrs
    ABX
    LDX 0,X
puzzle_unpack:
    STX unpack_source
unpack_packet:
    LDX unpack_source
    LDAB 0,X
    BEQ unpack_done
    INX
    STX unpack_source
    TSTB
    BMI unpack_repeat
    STAB unpack_count
unpack_literal:
    LDX unpack_source
    LDAA 0,X
    INX
    STX unpack_source
    LDX unpack_dest
    STAA 0,X
    INX
    STX unpack_dest
    DEC unpack_count
    BNE unpack_literal
    JSR input_poll
    BRA unpack_packet
unpack_repeat:
    SUBB #127
    STAB unpack_count
    LDAA 0,X
    STAA unpack_offset
    INX
    STX unpack_source
    LDD unpack_dest
    SUBB unpack_offset
    SBCA #0
    STD unpack_back
unpack_run:
    LDX unpack_back
    LDAA 0,X
    INX
    STX unpack_back
    LDX unpack_dest
    STAA 0,X
    INX
    STX unpack_dest
    DEC unpack_count
    BNE unpack_run
    JSR input_poll
    BRA unpack_packet
unpack_done:
    RTS
challenge_select_show:
    LDAA #1
    STAA phase
    CLR challenge_choice
    JSR input_gate
    JSR paint_clear
challenge_select_draw:
    LDX #challenge_arrow_blank
    LDAA #6
    LDAB #1
    JSR paint_text
    LDX #challenge_arrow_blank
    LDAA #6
    LDAB #3
    JSR paint_text
    LDX #challenge_arrow_blank
    LDAA #6
    LDAB #4
    JSR paint_text
    LDX #game_name
    LDAA #6
    CLRB
    JSR paint_text
    LDX #select_label
    LDAA #18
    LDAB #1
    JSR paint_text
    LDAA #54
    STAA paint_x
    LDAA #1
    STAA paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #challenge_total_label
    LDAA #78
    LDAB #1
    JSR paint_text
    LDX #challenge_best_label
    LDAA #114
    LDAB #1
    JSR paint_text
    LDAB stage
    LDX #challenge_best
    ABX
    LDAB 0,X
    ASLB
    ASLB
    LDX #challenge_star_text
    ABX
    LDAA #150
    LDAB #1
    JSR paint_text
    LDX #challenge_par_label
    LDAA #18
    LDAB #2
    JSR paint_text
    LDAB stage
    ASLB
    LDX #challenge_pars
    ABX
    LDD 0,X
    STD challenge_display_value
    LDAA #42
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDD challenge_display_value
    JSR paint_number16
    JSR challenge_hint
    LDAA #78
    LDAB #2
    JSR paint_text
    LDX #challenge_load_label
    LDAA #18
    LDAB #3
    JSR paint_text
    LDAA #QUICK_CHARS
    JSR challenge_encode
    LDX #password_text
    LDAA #78
    LDAB #3
    JSR paint_text
    LDX #challenge_record_label
    LDAA #18
    LDAB #4
    JSR paint_text
    LDAA #SCORE_CHARS
    JSR challenge_encode
    LDX #password_text
    LDAA #9
    LDAB #5
    JSR paint_text
    LDX #challenge_choose_label
    TST challenge_restored
    BEQ challenge_select_help
    LDX #challenge_restored_label
challenge_select_help:
    LDAA #6
    LDAB #6
    JSR paint_text
    LDX #challenge_select_help_label
    CLRA
    LDAB #7
    JSR paint_text
    LDAB challenge_choice
    LDX #challenge_choice_rows
    ABX
    LDAB 0,X
    LDX #menu_arrow
    LDAA #6
    JMP paint_text
challenge_select_update:
    LDAA input_event
    BITA #32
    BEQ challenge_select_accept
    JSR show_title
    JMP flush
challenge_select_accept:
    BITA #16
    BEQ challenge_select_direction
    LDAA challenge_choice
    BNE challenge_select_password
    JSR begin_game
    JMP flush
challenge_select_password:
    LDAA #QUICK_CHARS
    LDAB challenge_choice
    CMPB #1
    BEQ challenge_password_open
    LDAA #SCORE_CHARS
challenge_password_open:
    JSR challenge_encode
    CLR password_cursor
    CLR password_error
    LDAA #6
    STAA phase
    JSR input_gate
    JSR paint_clear
    JSR challenge_edit_draw
    JMP flush
challenge_select_direction:
    BITA #1
    BEQ challenge_choice_down
    TST challenge_choice
    BEQ challenge_choice_last
    DEC challenge_choice
    BRA challenge_select_changed
challenge_choice_last:
    LDAA #2
    STAA challenge_choice
    BRA challenge_select_changed
challenge_choice_down:
    BITA #2
    BEQ challenge_stage_move
    INC challenge_choice
    LDAA challenge_choice
    CMPA #3
    BCS challenge_select_changed
    CLR challenge_choice
    BRA challenge_select_changed
challenge_stage_move:
    TST challenge_choice
    BNE challenge_select_idle
    BITA #4
    BEQ challenge_stage_next
    TST stage
    BEQ challenge_stage_last
    DEC stage
    BRA challenge_select_changed
challenge_stage_last:
    LDAA #STAGES - 1
    STAA stage
    BRA challenge_select_changed
challenge_stage_next:
    BITA #8
    BEQ challenge_select_idle
    INC stage
    LDAA stage
    CMPA #STAGES
    BCS challenge_select_changed
    CLR stage
challenge_select_changed:
    CLR challenge_restored
    JSR challenge_select_draw
challenge_select_idle:
    JMP flush
; Five characters restore the selected stage; 25 also carry all forty 2-bit bests.
; CRC-8 covers the game tag, stage and each packed score before any state is changed.
challenge_encode:
    STAA password_length
    LDAA #PUZZLE_ID
    STAA password_digits
    LDAA stage
    ANDA #15
    STAA password_digits + 2
    LDAA stage
    LSRA
    LSRA
    LSRA
    LSRA
    STAA password_digits + 1
    LDAA password_length
    CMPA #QUICK_CHARS
    BEQ challenge_encode_crc
    LDX #challenge_best
    STX password_source
    LDX #password_digits + 3
    STX password_dest
    LDAB #STAGES / 2
    STAB password_count
challenge_pack_scores:
    LDX password_source
    LDAA 0,X
    ASLA
    ASLA
    ORAA 1,X
    INX
    INX
    STX password_source
    LDX password_dest
    STAA 0,X
    INX
    STX password_dest
    DEC password_count
    BNE challenge_pack_scores
challenge_encode_crc:
    JSR challenge_crc
    LDAB password_length
    SUBB #2
    LDX #password_digits
    ABX
    LDAA password_crc
    ANDA #15
    STAA 1,X
    LDAA password_crc
    LSRA
    LSRA
    LSRA
    LSRA
    STAA 0,X
    JMP challenge_format
challenge_crc:
    LDAA #$A7
    STAA password_crc
    CLR password_index
    LDAA password_length
    SUBA #2
    STAA password_count
challenge_crc_byte:
    LDAB password_index
    LDX #password_digits
    ABX
    LDAA 0,X
    EORA password_crc
    LDAB #8
challenge_crc_bit:
    LSRA
    BCC challenge_crc_next_bit
    EORA #$8C
challenge_crc_next_bit:
    DECB
    BNE challenge_crc_bit
    STAA password_crc
    INC password_index
    DEC password_count
    BNE challenge_crc_byte
    RTS
challenge_format:
    LDX #password_text
    STX password_dest
    CLR password_index
    CLR password_group
challenge_format_char:
    LDAB password_index
    LDX #password_digits
    ABX
    LDAB 0,X
    LDX #password_alphabet
    ABX
    LDAA 0,X
    LDX password_dest
    STAA 0,X
    INX
    INC password_index
    LDAA password_index
    CMPA password_length
    BEQ challenge_format_done
    INC password_group
    LDAA password_group
    CMPA #5
    BNE challenge_format_more
    LDAA #32
    STAA 0,X
    INX
    CLR password_group
challenge_format_more:
    STX password_dest
    BRA challenge_format_char
challenge_format_done:
    CLR 0,X
    RTS
challenge_validate:
    ; Validate all digits before treating them as table indices or unpacking state.
    LDAA password_length
    CMPA #QUICK_CHARS
    BEQ challenge_validate_digits
    CMPA #SCORE_CHARS
    BEQ challenge_validate_next_1
    JMP challenge_code_bad
challenge_validate_next_1:
challenge_validate_digits:
    LDX #password_digits
    LDAB password_length
challenge_validate_digit:
    LDAA 0,X
    CMPA #16
    BCS challenge_validate_next_2
    JMP challenge_code_bad
challenge_validate_next_2:
    INX
    DECB
    BNE challenge_validate_digit
    LDAA password_digits
    CMPA #PUZZLE_ID
    BEQ challenge_validate_next_3
    JMP challenge_code_bad
challenge_validate_next_3:
    LDAA password_digits + 1
    ASLA
    ASLA
    ASLA
    ASLA
    ORAA password_digits + 2
    CMPA #STAGES
    BCS challenge_validate_next_4
    JMP challenge_code_bad
challenge_validate_next_4:
    STAA password_stage
    JSR challenge_crc
    LDAB password_length
    SUBB #2
    LDX #password_digits
    ABX
    LDAA 0,X
    ASLA
    ASLA
    ASLA
    ASLA
    ORAA 1,X
    CMPA password_crc
    BEQ challenge_validate_next_5
    JMP challenge_code_bad
challenge_validate_next_5:
    LDAA password_stage
    STAA stage
    LDAA password_length
    CMPA #QUICK_CHARS
    BEQ challenge_code_good
    LDX #password_digits + 3
    STX password_source
    LDX #challenge_best
    STX password_dest
    LDAA #STAGES / 2
    STAA password_count
challenge_unpack_scores:
    LDX password_source
    LDAA 0,X
    INX
    STX password_source
    TAB
    ANDA #3
    LDX password_dest
    STAA 1,X
    TBA
    LSRA
    LSRA
    STAA 0,X
    INX
    INX
    STX password_dest
    DEC password_count
    BNE challenge_unpack_scores
challenge_code_good:
    CLR password_error
    LDAA #1
    STAA challenge_restored
    RTS
challenge_code_bad:
    LDAA #1
    STAA password_error
    RTS
challenge_edit_update:
    LDAA input_event
    BITA #32
    BEQ challenge_edit_accept
    JSR show_select
    JMP flush
challenge_edit_accept:
    BITA #16
    BEQ challenge_edit_direction
    LDAA password_cursor
    INCA
    CMPA password_length
    BCS challenge_cursor_next
    JSR challenge_validate
    TST password_error
    BNE challenge_edit_changed
    JSR show_select
    JMP flush
challenge_edit_direction:
    BITA #4
    BEQ challenge_cursor_right
    TST password_cursor
    BEQ challenge_edit_idle
    DEC password_cursor
    BRA challenge_edit_changed
challenge_cursor_right:
    BITA #8
    BEQ challenge_edit_symbol
    LDAA password_cursor
    INCA
    CMPA password_length
    BCC challenge_edit_idle
challenge_cursor_next:
    INC password_cursor
    BRA challenge_edit_changed
challenge_edit_symbol:
    LDAB password_cursor
    LDX #password_digits
    ABX
    BITA #1
    BEQ challenge_symbol_down
    LDAA 0,X
    INCA
    BRA challenge_symbol_store
challenge_symbol_down:
    BITA #2
    BEQ challenge_edit_idle
    LDAA 0,X
    DECA
challenge_symbol_store:
    ANDA #15
    STAA 0,X
    CLR password_error
challenge_edit_changed:
    JSR challenge_edit_draw
challenge_edit_idle:
    JMP flush
challenge_edit_draw:
    LDX #password_quick_label
    LDAA password_length
    CMPA #QUICK_CHARS
    BEQ challenge_edit_heading
    LDX #password_record_label
challenge_edit_heading:
    LDAA #12
    LDAB #1
    JSR paint_text
    JSR challenge_format
    LDX #password_text
    LDAA #9
    LDAB #3
    JSR paint_text
    LDX #password_cursor_blank
    LDAA #9
    LDAB #4
    JSR paint_text
    LDAA password_cursor
    CLRB
challenge_cursor_group:
    CMPA #5
    BCS challenge_cursor_x
    SUBA #5
    INCB
    BRA challenge_cursor_group
challenge_cursor_x:
    ADDB password_cursor
    LDAA #6
    MUL
    ADDB #9
    TBA
    LDX #password_arrow
    LDAB #4
    JSR paint_text
    LDX #password_edit_help
    TST password_error
    BEQ challenge_edit_status
    LDX #password_invalid_label
challenge_edit_status:
    LDAA #9
    LDAB #6
    JSR paint_text
    LDX #password_confirm_help
    LDAA #9
    LDAB #7
    JMP paint_text
challenge_result:
    JSR input_gate
    LDX #challenge_result_edge
    LDAA #24
    LDAB #1
    JSR paint_text
    LDAA #2
    STAA challenge_result_row
challenge_result_blank:
    LDX #challenge_result_empty
    LDAA #24
    LDAB challenge_result_row
    JSR paint_text
    INC challenge_result_row
    LDAA challenge_result_row
    CMPA #6
    BNE challenge_result_blank
    LDX #challenge_result_edge
    LDAA #24
    LDAB #6
    JSR paint_text
    LDX result_label
    LDAA #42
    LDAB #2
    JSR paint_text
    LDAB challenge_rating
    ASLB
    ASLB
    LDX #challenge_star_text
    ABX
    LDAA #120
    LDAB #2
    JSR paint_text
    LDX #challenge_used_label
    LDAA #36
    LDAB #3
    JSR paint_text
    LDAA #66
    STAA paint_x
    LDAA #3
    STAA paint_band
    LDD challenge_moves
    JSR paint_number16
    LDX #challenge_par_label
    LDAA #102
    LDAB #3
    JSR paint_text
    LDAA #126
    STAA paint_x
    LDAA #3
    STAA paint_band
    LDD challenge_par
    JSR paint_number16
    LDX #challenge_bonus_missed
    LDAA challenge_bonus
    ANDA challenge_need
    CMPA challenge_need
    BNE challenge_result_bonus
    LDX #challenge_bonus_complete
challenge_result_bonus:
    LDAA #36
    LDAB #4
    JSR paint_text
    LDX #challenge_next_label
    LDAA #36
    LDAB #5
    JMP paint_text
.section .bss, bss
challenge_moves: .space 2
challenge_par: .space 2
challenge_overflow: .space 1
challenge_bonus: .space 1
challenge_need: .space 1
challenge_undo_bonus: .space 1
challenge_cells: .space 2
challenge_touch_cell: .space 1
challenge_view_cells: .space 2
challenge_rating: .space 1
challenge_best: .space STAGES
challenge_choice: .space 1
challenge_restored: .space 1
challenge_display_value: .space 2
challenge_result_row: .space 1
unpack_source: .space 2
unpack_dest: .space 2
unpack_count: .space 1
unpack_offset: .space 1
unpack_back: .space 2
password_digits: .space SCORE_CHARS
password_text: .space 30
password_length: .space 1
password_cursor: .space 1
password_error: .space 1
password_stage: .space 1
password_source: .space 2
password_dest: .space 2
password_index: .space 1
password_group: .space 1
password_count: .space 1
password_crc: .space 1
.section .data, data
password_alphabet: .byte 50,51,52,54,55,56,57,65,67,68,69,70,72,74,75,77
challenge_star_text: .byte 45,45,45,0,42,45,45,0,42,42,45,0,42,42,42,0
challenge_total_label: .byte 47,48,52,48,0
challenge_best_label: .byte 66,69,83,84,0
challenge_par_label: .byte 80,65,82,0
challenge_used_label: .byte 85,83,69,68,0
challenge_load_label: .byte 76,79,65,68,0
challenge_record_label: .byte 76,79,65,68,32,82,69,67,79,82,68,0
challenge_choose_label: .byte 85,80,47,68,79,87,78,58,32,67,72,79,79,83,69,32,32,32,0
challenge_restored_label: .byte 67,79,68,69,32,82,69,83,84,79,82,69,68,32,32,32,32,32,0
challenge_select_help_label: .byte 76,47,82,32,67,72,65,78,71,69,32,83,80,65,67,69,32,79,75,32,82,69,84,85,82,78,32,66,65,67,75,0
challenge_choice_rows: .byte 1,3,4
challenge_arrow_blank: .byte 32,0
password_cursor_blank: .byte 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0
password_quick_label: .byte 69,78,84,69,82,32,82,69,83,85,77,69,32,67,79,68,69,0
password_record_label: .byte 69,78,84,69,82,32,82,69,67,79,82,68,32,67,79,68,69,0
password_arrow: .byte 94,0
password_edit_help: .byte 85,80,47,68,79,87,78,32,76,69,84,84,69,82,32,32,76,47,82,32,80,79,83,73,84,73,79,78,0
password_invalid_label: .byte 73,78,86,65,76,73,68,32,67,79,68,69,32,45,32,67,72,69,67,75,32,76,69,84,84,69,82,83,0
password_confirm_help: .byte 83,80,65,67,69,32,78,69,88,84,47,76,79,65,68,32,32,82,69,84,85,82,78,32,67,65,78,67,69,76,0
challenge_result_edge: .byte 43,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,43,0
challenge_result_empty: .byte 124,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,124,0
challenge_bonus_missed: .byte 66,79,78,85,83,58,32,73,78,67,79,77,80,76,69,84,69,0
challenge_bonus_complete: .byte 66,79,78,85,83,58,32,67,79,77,80,76,69,84,69,0
challenge_next_label: .byte 83,80,65,67,69,32,78,69,88,84,32,32,82,69,84,32,77,69,78,85,0
