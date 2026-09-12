; SPDX-License-Identifier: MIT
.global undo_valid
.section .text, code
game_render:
    JSR paint_board
    JMP visual_hud

game_start:
    CLR lamp_active
    JSR grid_reset
    JSR challenge_start
    LDAA challenge_bonus
    STAA lamp_view_bonus
    ; Targets use complete lamp sprites, so suppress the shared corner marks.
    LDAA #255
    STAA challenge_view_cells
    STAA challenge_view_cells + 1
    CLR undo_valid
    LDX #board
    JSR challenge_load
    JMP lamp_count
game_update:
    TST lamp_active
    BEQ lamp_input
    JMP lamp_animation_update
lamp_input:
    LDAA input_event
    BITA #16
    BEQ lamp_move
    LDAA cursor
    STAA lamp_last
    LDAA moves
    STAA lamp_saved_moves
    LDAA #1
    STAA undo_valid
    JSR lamp_animation_begin
    JSR challenge_step
    LDAB cursor
    JSR challenge_touch
    JSR lamp_toggle
    JSR grid_count_move
    JSR lamp_animation_start
    JMP lamp_animation_update
lamp_move:
    JSR grid_move
    CMPB #255
    BEQ lamp_idle
    STAB cursor
    JMP grid_changed
lamp_idle:
    RTS
game_aux:
    CMPA #1
    BNE lamp_restart
    TST undo_valid
    BEQ lamp_idle
    TST lamp_active
    BNE lamp_cancel_undo
    LDAA cursor
    STAA lamp_saved
    LDAA lamp_last
    STAA cursor
    JSR lamp_animation_begin
    JSR challenge_undo
    JSR lamp_toggle
    LDAA lamp_saved
    STAA lamp_anim_cursor
    CLR undo_valid
    LDAA lamp_saved_moves
    STAA moves
    JMP lamp_animation_start
; Undo from a paused flip cancels the whole pending move and restores its start.
lamp_cancel_undo:
    CLR lamp_active
    LDAA lamp_anim_cursor
    STAA cursor
    JSR challenge_undo
    LDAA cursor
    STAA lamp_saved
    LDAA lamp_last
    STAA cursor
    JSR lamp_toggle
    LDAA lamp_saved
    STAA cursor
    CLR undo_valid
    LDAA lamp_saved_moves
    STAA moves
    LDAA challenge_bonus
    STAA lamp_view_bonus
    RTS
lamp_restart:
    JMP game_start
lamp_toggle:
    LDAB cursor
    JSR lamp_flip
    LDX #neighbors
    STX lamp_source
    LDAA #4
    STAA lamp_index
lamp_near:
    LDX lamp_source
    LDAB cursor
    ABX
    LDAB 0,X
    CMPB #255
    BEQ lamp_skip
    JSR lamp_flip
lamp_skip:
    LDD lamp_source
    ADDD #25
    STD lamp_source
    DEC lamp_index
    BNE lamp_near
lamp_count:
    CLR grid_stat
    LDX #board
    TST lamp_active
    BEQ lamp_count_source
    LDX #lamp_display
lamp_count_source:
    LDAB #25
lamp_count_loop:
    TST 0,X
    BEQ lamp_count_next
    INC grid_stat
lamp_count_next:
    INX
    DECB
    BNE lamp_count_loop
    RTS
lamp_flip:
    LDX #board
    ABX
    LDAA 0,X
    EORA #1
    STAA 0,X
    RTS
grid_value:
    TST lamp_active
    BEQ lamp_board_value
    CMPB lamp_anim_cell
    BNE lamp_display_value
    LDAA lamp_sprite
    RTS
lamp_display_value:
    LDX #lamp_display
    BRA lamp_visible_value
lamp_board_value:
    LDX #board
lamp_visible_value:
    LDAA lamp_view_bonus
    STAA lamp_face_bonus
; X = board or presentation board, B = cell, lamp_face_bonus = visible goals.
lamp_value_at:
    ABX
    LDAA 0,X
    CMPB challenge_cells
    BEQ lamp_first_target
    CMPB challenge_cells + 1
    BNE lamp_value_done
    LDAB lamp_face_bonus
    BITB #2
    BEQ lamp_target_value
    RTS
lamp_first_target:
    LDAB lamp_face_bonus
    BITB #1
    BNE lamp_value_done
lamp_target_value:
    ADDA #2
lamp_value_done:
    RTS
.section .bss, bss
lamp_source: .space 2
lamp_index: .space 1
lamp_last: .space 1
lamp_saved: .space 1
lamp_saved_moves: .space 1
undo_valid: .space 1

.section .text, code
game_bonus:
    RTS
