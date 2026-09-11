; SPDX-License-Identifier: MIT
.global beat_notes
.global beat_interval
.global beat_window
.global beat_total
.global beat_index
.global beat_time
.global beat_due
.global beat_score
.global beat_life
.global beat_combo
.global beat_active
.global beat_previous
.global beat_pressed
.global beat_judgement
.global beat_mute
.global beat_rules
.global beat_edges
.section .text, code
game_start:
    LDAA stage
    LDAB #67
    MUL
    ADDD #beat_charts
    XGDX
    LDAA 0,X
    STAA beat_interval
    LDAA 1,X
    STAA beat_window
    LDAA 2,X
    STAA beat_total
    INX
    INX
    INX
    STX beat_pointer
    CLR beat_loop
beat_load:
    LDX beat_pointer
    LDAA 0,X
    INX
    STX beat_pointer
    LDAB beat_loop
    LDX #beat_notes
    ABX
    STAA 0,X
    INC beat_loop
    LDAB beat_loop
    CMPB #64
    BNE beat_load
    CLR beat_time
    CLR beat_time + 1
    CLR beat_score
    CLR beat_score + 1
    CLR beat_index
    CLR beat_combo
    CLR beat_active
    CLR beat_previous
    CLR beat_pressed
    CLR beat_judgement
    CLR beat_mute
    CLR beat_draw_count
    LDD #64
    STD beat_due
    STD beat_sound_due
    LDAA #6
    STAA beat_life
    LDAA #1
    STAA beat_hud
    RTS
game_aux:
    CMPA #2
    BEQ game_start
    LDAA beat_mute
    EORA #1
    STAA beat_mute
    LDAA #1
    STAA beat_hud
    RTS
game_update:
    TST resume_pending
    BEQ beat_resumed
    CLR resume_pending
    LDAA input_ticks
    STAA beat_clock
    LDAA input_held
    ANDA #15
    STAA beat_previous
beat_resumed:
    TST beat_active
    BNE beat_timing
    LDAA input_event
    BITA #16
    BEQ beat_update_done
    LDAA #1
    STAA beat_active
    STAA beat_hud
    LDAA input_ticks
    STAA beat_clock
    LDAA input_held
    ANDA #15
    STAA beat_previous
    BRA beat_repaint
beat_timing:
    LDAA input_ticks
    SUBA beat_clock
    TAB
    LDAA input_ticks
    STAA beat_clock
    CLRA
    ADDD beat_time
    STD beat_time
    JSR beat_edges
    JSR beat_rules
    TST beat_active
    BEQ beat_check_redraw
    LDD beat_time
    SUBD beat_sound_due
    BCS beat_check_redraw
    LDD beat_sound_due
    ADDB beat_interval
    ADCA #0
    STD beat_sound_due
    TST beat_mute
    BNE beat_check_redraw
    JSR chirp
beat_check_redraw:
    TST beat_hud
    BNE beat_repaint
    LDD beat_time
    LSRA
    RORB
    SUBD beat_draw_time
    BEQ beat_update_done
beat_repaint:
    JSR beat_erase
    LDAA #1
    STAA redraw
beat_update_done:
    RTS
; SDK has already merged keypad/WASD aliases into four logical held bits.
beat_edges:
    LDAA beat_previous
    COMA
    ANDA input_held
    ANDA #15
    STAA beat_pressed
    LDAA input_held
    ANDA #15
    STAA beat_previous
    RTS
; Expiry and key judgement have no sound or rendering side effects.
beat_rules:
    CLR beat_expired
beat_expire_loop:
    LDD beat_time
    SUBD beat_due
    BCS beat_key_judge
    TSTA
    BNE beat_expire
    CMPB beat_window
    BLS beat_key_judge
beat_expire:
    INC beat_expired
    JSR beat_damage
    JSR beat_advance
    TST beat_active
    BNE beat_expire_loop
    RTS
beat_key_judge:
    TST beat_expired
    BEQ beat_branch_1
    JMP beat_rules_done
beat_branch_1:
    TST beat_pressed
    BNE beat_branch_2
    JMP beat_rules_done
beat_branch_2:
    LDD beat_time
    SUBD beat_due
    BCC beat_absolute
    STD beat_difference
    LDD #0
    SUBD beat_difference
beat_absolute:
    TSTA
    BNE beat_damage
    CMPB beat_window
    BHI beat_damage
    STAB beat_accuracy
    LDAB beat_index
    LDX #beat_notes
    ABX
    LDAB 0,X
    LDX #beat_masks
    ABX
    LDAA 0,X
    CMPA beat_pressed
    BNE beat_damage
    LDAA #2
    STAA beat_judgement
    LDD #60
    STAA beat_points
    STAB beat_points + 1
    LDAA beat_accuracy
    CMPA #2
    BHI beat_award
    LDAA #1
    STAA beat_judgement
    LDD #100
    STD beat_points
beat_award:
    LDD beat_score
    ADDD beat_points
    STD beat_score
    INC beat_combo
    LDAA #1
    STAA beat_hud
    JMP beat_advance
beat_damage:
    CLR beat_combo
    LDAA #3
    STAA beat_judgement
    LDAA #1
    STAA beat_hud
    DEC beat_life
    BEQ beat_branch_3
    JMP beat_rules_done
beat_branch_3:
    CLR beat_active
    LDAA #5
    STAA phase
beat_rules_done:
    RTS
beat_advance:
    INC beat_index
    LDD beat_due
    ADDB beat_interval
    ADCA #0
    STD beat_due
    TST beat_active
    BEQ beat_advance_done
    LDAA beat_index
    CMPA beat_total
    BCS beat_advance_done
    CLR beat_active
    LDAA #4
    STAA phase
beat_advance_done:
    RTS
beat_erase:
    CLR beat_loop
    LDAA #8
    STAA scene_w
    STAA scene_h
beat_erase_next:
    LDAA beat_loop
    CMPA beat_draw_count
    BCC beat_erase_done
    TAB
    LDX #beat_old_x
    ABX
    LDAA 0,X
    STAA beat_draw_x
    LDAB beat_loop
    LDX #beat_old_y
    ABX
    LDAB 0,X
    LDAA beat_draw_x
    JSR scene_rect
    INC beat_loop
    BRA beat_erase_next
beat_erase_done:
    RTS
game_tile:
    TBA
    ANDA #15
    CMPA #2
    BEQ beat_vertical
    BITB #16
    BNE beat_blank
    LDAA #3
    RTS
beat_vertical:
    BITB #16
    BNE beat_vertical_only
    LDAA #4
    RTS
beat_vertical_only:
    LDAA #2
    RTS
beat_blank:
    CLRA
    RTS
game_render:
    JSR paint_board
    LDD beat_time
    LSRA
    RORB
    STD beat_draw_time
    CLR beat_draw_count
    LDAA beat_index
    STAA beat_draw_index
    LDD beat_due
    STD beat_draw_due
beat_note_next:
    LDAA beat_draw_count
    CMPA #5
    BCS beat_branch_4
    JMP beat_notes_done
beat_branch_4:
    LDAA beat_draw_index
    CMPA beat_total
    BCS beat_branch_5
    JMP beat_notes_done
beat_branch_5:
    LDD beat_draw_due
    SUBD beat_time
    BCS beat_note_late
    TSTA
    BEQ beat_branch_6
    JMP beat_notes_done
beat_branch_6:
    CMPB #104
    BLS beat_note_in_range
    JMP beat_notes_done
beat_note_in_range:
    TBA
    ADDA #13
    BRA beat_note_position
beat_note_late:
    LDD beat_time
    SUBD beat_draw_due
    STAB beat_offset
    LDAA #13
    SUBA beat_offset
beat_note_position:
    STAA beat_draw_x
    LDAB beat_draw_count
    LDX #beat_old_x
    ABX
    STAA 0,X
    LDAB beat_draw_index
    LDX #beat_notes
    ABX
    LDAA 0,X
    STAA beat_lane
    LDAB #16
    MUL
    ADDB #8
    STAB beat_note_top
    LDAB beat_draw_count
    LDX #beat_old_y
    ABX
    LDAA beat_note_top
    STAA 0,X
    LDAA beat_lane
    LDAB #8
    MUL
    ADDD #beat_arrows
    STD beat_arrow_pointer
    LDAA beat_lane
    ASLA
    INCA
    STAA beat_note_band
    LDAB #192
    MUL
    ADDD #framebuffer
    ADDB beat_draw_x
    ADCA #0
    STD beat_destination
    LDAA #8
    STAA beat_columns
; Arrows stay aligned to an LCD band, so each column is one byte operation.
beat_arrow_column:
    LDX beat_arrow_pointer
    LDAA 0,X
    INX
    STX beat_arrow_pointer
    LDX beat_destination
    ORAA 0,X
    CMPA 0,X
    BEQ beat_arrow_unchanged
    STAA 0,X
    LDAA beat_note_band
    LDAB beat_draw_x
    JSR dirty_mark
beat_arrow_unchanged:
    LDX beat_destination
    INX
    STX beat_destination
    JSR input_poll
    INC beat_draw_x
    DEC beat_columns
    BNE beat_arrow_column
    INC beat_draw_count
    INC beat_draw_index
    LDD beat_draw_due
    ADDB beat_interval
    ADCA #0
    STD beat_draw_due
    JMP beat_note_next
beat_notes_done:
    TST resume_pending
    BNE beat_hud_all
    TST beat_hud
    BNE beat_hud_values
    RTS
beat_hud_all:
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #beat_score_label
    LDAA #132
    LDAB #1
    JSR paint_text
    LDX #beat_life_label
    LDAA #132
    LDAB #3
    JSR paint_text
    LDX #beat_combo_label
    LDAA #132
    LDAB #5
    JSR paint_text
beat_hud_values:
    CLR beat_hud
    LDX #beat_unmuted_label
    TST beat_mute
    BEQ beat_mute_text
    LDX #beat_muted_label
beat_mute_text:
    LDAA #60
    CLRB
    JSR paint_text
    TST beat_active
    BNE beat_progress_text
    TST beat_index
    BNE beat_progress_text
    LDX #beat_start_label
    LDAA #78
    CLRB
    JSR paint_text
    BRA beat_score_text
beat_progress_text:
    LDAA #78
    STAA paint_x
    CLR paint_band
    LDAA beat_index
    JSR paint_number
    LDX #beat_slash_label
    LDAA #96
    CLRB
    JSR paint_text
    LDAA #102
    STAA paint_x
    LDAA beat_total
    JSR paint_number
beat_score_text:
    LDAA #132
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDD beat_score
    JSR paint_number16
    LDAA #132
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA beat_life
    JSR paint_number
    LDAA #132
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA beat_combo
    JSR paint_number
    LDAB beat_judgement
    ASLB
    LDX #beat_judge_labels
    ABX
    LDX 0,X
    LDAA #132
    LDAB #7
    JMP paint_text
.section .bss, bss
beat_notes: .space 64
beat_interval: .space 1
beat_window: .space 1
beat_total: .space 1
beat_index: .space 1
beat_time: .space 2
beat_due: .space 2
beat_score: .space 2
beat_life: .space 1
beat_combo: .space 1
beat_active: .space 1
beat_previous: .space 1
beat_pressed: .space 1
beat_judgement: .space 1
beat_mute: .space 1
beat_clock: .space 1
beat_sound_due: .space 2
beat_pointer: .space 2
beat_loop: .space 1
beat_hud: .space 1
beat_expired: .space 1
beat_difference: .space 2
beat_accuracy: .space 1
beat_points: .space 2
beat_draw_time: .space 2
beat_draw_due: .space 2
beat_draw_index: .space 1
beat_draw_count: .space 1
beat_old_x: .space 5
beat_old_y: .space 5
beat_draw_x: .space 1
beat_lane: .space 1
beat_offset: .space 1
beat_note_top: .space 1
beat_arrow_pointer: .space 2
beat_columns: .space 1
beat_note_band: .space 1
beat_destination: .space 2
.section .data, data
beat_masks: .byte 4,2,1,8
beat_score_label: .byte 83,67,79,82,69,0
beat_life_label: .byte 76,73,70,69,0
beat_combo_label: .byte 67,79,77,66,79,0
beat_start_label: .byte 83,80,65,67,69,32,32,0
beat_slash_label: .byte 47,0
beat_muted_label: .byte 77,0
beat_unmuted_label: .byte 32,0
beat_judge_labels: .word beat_wait_label,beat_perfect_label,beat_good_label,beat_miss_label
beat_wait_label: .byte 82,69,65,68,89,32,32,0
beat_perfect_label: .byte 80,69,82,70,69,67,84,0
beat_good_label: .byte 71,79,79,68,32,32,32,0
beat_miss_label: .byte 77,73,83,83,32,32,32,0
