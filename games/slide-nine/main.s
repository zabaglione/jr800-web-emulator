; SPDX-License-Identifier: MIT
.global undo_valid
.section .text, code
game_render:
    CLR paint_index
slide_render_tile:
    LDAB paint_index
    JSR game_tile
    ANDA #127
    BEQ slide_render_face
    STAA paint_id
    LDAB grid_subtile
    ANDB #3
    BEQ slide_render_edge
    CMPB #3
    BEQ slide_render_edge
    LDX #board
    LDAB grid_cell
    ABX
    LDAB 0,X
    CMPB slide_token
    BNE slide_render_base
    DECB
    ASLB
    ASLB
    ASLB
    LDX #slide_token_tiles
    ABX
    LDAB grid_subtile
    ABX
    LDAA 0,X
    BRA slide_render_face
slide_render_edge:
    LDAB grid_cell
    CMPB challenge_cells
    BNE slide_render_base
    LDAA #105
    TST challenge_bonus
    BEQ slide_render_border
    LDAA #113
slide_render_border:
    ADDA grid_subtile
    BRA slide_render_face
slide_render_base:
    LDAA paint_id
slide_render_face:
    LDAB paint_index
    JSR paint_tile
    INC paint_index
    LDAA paint_index
    ANDA #7
    BNE slide_render_next
    JSR input_poll
slide_render_next:
    LDAA paint_index
    CMPA #112
    BNE slide_render_tile
    JSR slide_frame_center
    JSR visual_hud
    ; Match the STAMP number to the inverse tile that must be moved.
    LDAA slide_token
    DECA
    LDAB #6
    MUL
    ADDD #slide_stamp_faces
    STD paint_source
    LDAA #54
    STAA paint_x
    LDAA #6
    STAA paint_band
    STAA paint_count
    JSR paint_address
    CLR paint_id
    JMP paint_blit

; Continue the destination's border across the number-bearing middle tiles.
; Preserve the digit rows, including when the stamped tile leaves this cell.
slide_frame_center:
    LDAB challenge_cells
    ASLB
    LDX #slide_frame_origins
    ABX
    LDAA 0,X
    STAA paint_x
    LDAA 1,X
    STAA paint_band
    JSR paint_address
    LDAA #4
    TST challenge_bonus
    BEQ slide_frame_bits
    LDAA #6
slide_frame_bits:
    STAA slide_edge_bits
    LDAA #16
    STAA paint_count
slide_frame_column:
    LDX paint_dest
    LDAA 0,X
    ANDA #$F9
    ORAA slide_edge_bits
    STAA 0,X
    LDAA slide_edge_bits
    ASLA
    ASLA
    ASLA
    ASLA
    STAA slide_edge_high
    LDAA 192,X
    ANDA #$9F
    ORAA slide_edge_high
    STAA 192,X
    INX
    STX paint_dest
    TST challenge_bonus
    BNE slide_frame_next
    LDAA slide_edge_bits
    EORA #6
    STAA slide_edge_bits
slide_frame_next:
    DEC paint_count
    BNE slide_frame_column
    LDAA paint_band
    LDAB paint_x
    JSR dirty_mark
    LDAA paint_band
    LDAB paint_x
    ADDB #15
    JSR dirty_mark
    LDAA paint_band
    INCA
    LDAB paint_x
    JSR dirty_mark
    LDAA paint_band
    INCA
    LDAB paint_x
    ADDB #15
    JMP dirty_mark

game_start:
    JSR grid_reset
    JSR challenge_start
    ; The destination has a complete patterned frame, rather than a tiny mark.
    LDAA #255
    STAA challenge_view_cells
    STAA challenge_view_cells + 1
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
slide_edge_bits: .space 1
slide_edge_high: .space 1
.section .text, code
game_bonus:
    RTS
