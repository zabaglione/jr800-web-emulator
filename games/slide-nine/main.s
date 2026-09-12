; SPDX-License-Identifier: MIT
.global undo_valid
.section .text, code
game_render:
    TST hud_ready
    BNE slide_render_begin
    JSR hud_begin
    LDX #framebuffer
    STX unpack_dest
    LDX #slide_panel_art
    JSR puzzle_unpack
    JSR dirty_all
    ; Blank view cells retain the surrounding case instead of erasing it.
    CLR paint_index
slide_panel_cache:
    LDAB paint_index
    LDX #view_cells
    ABX
    LDAA 0,X
    CMPA #255
    BNE slide_panel_next
    LDX #tile_cache
    ABX
    CLR 0,X
slide_panel_next:
    INC paint_index
    LDAA paint_index
    CMPA #112
    BNE slide_panel_cache
slide_render_begin:
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
    JSR slide_animate
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
    BNE slide_present
    LDAA #4
    STAA phase
slide_present:
    JSR render_play_result
    JSR slide_anim_flush
    JSR input_gate
    LDD slide_anim_bytes
    STD dirty_bytes
    PULX
    JMP frame_ready
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

; Translate the two adjacent tile images by eight real LCD pixels per step.
; The final compositor restores the stationary STAMP frame and empty recess.
.global slide_anim_frame
.global slide_anim_step
.global slide_anim_vertical
.section .text, code
slide_animate:
    CLR slide_anim_bytes
    CLR slide_anim_bytes + 1
    CLR slide_anim_step
    CLR slide_anim_reverse
    CLR slide_anim_vertical
    LDAA slide_pending
    SUBA slide_last
    BPL slide_anim_distance
    INC slide_anim_reverse
    NEGA
slide_anim_distance:
    CMPA #3
    BNE slide_anim_horizontal
    INC slide_anim_vertical
slide_anim_horizontal:
    LDAB slide_last
    CMPB slide_pending
    BCS slide_anim_find_origin
    LDAB slide_pending
slide_anim_find_origin:
    ASLB
    LDX #slide_frame_origins
    ABX
    LDAA 0,X
    SUBA #8
    STAA slide_anim_x
    STAA paint_x
    LDAA 1,X
    STAA slide_anim_band
    STAA paint_band
    JSR paint_address
    LDD paint_dest
    STD slide_anim_origin
slide_anim_next:
    LDX slide_anim_origin
    TST slide_anim_vertical
    BEQ slide_anim_horizontal_next
    JMP slide_anim_columns
slide_anim_horizontal_next:
    STX slide_anim_line
    LDAA #2
    STAA slide_anim_rows
slide_anim_row:
    LDAA #8
    STAA slide_anim_pixels
slide_anim_pixel:
    LDX slide_anim_line
    LDAB #63
    TST slide_anim_reverse
    BNE slide_anim_right
    LDAA 0,X
    STAA slide_anim_temp
slide_anim_left_loop:
    LDAA 1,X
    STAA 0,X
    INX
    DECB
    BNE slide_anim_left_loop
    BRA slide_anim_wrap
slide_anim_right:
    ABX
    LDAA 0,X
    STAA slide_anim_temp
slide_anim_right_loop:
    DEX
    LDAA 0,X
    STAA 1,X
    DECB
    BNE slide_anim_right_loop
slide_anim_wrap:
    LDAA slide_anim_temp
    STAA 0,X
    DEC slide_anim_pixels
    BNE slide_anim_pixel
    LDD slide_anim_line
    ADDD #192
    STD slide_anim_line
    DEC slide_anim_rows
    BNE slide_anim_row
    BRA slide_anim_dirty
.section .runtime, code
slide_anim_columns:
    LDAA #32
    STAA slide_anim_pixels
slide_anim_column:
    STX slide_anim_line
    LDAA #4
    STAA slide_anim_bits
slide_anim_bit:
    LDAB #192
    TST slide_anim_reverse
    BNE slide_anim_down
    LDAA 0,X
    LSRA
    ABX
    ABX
    ABX
    ROR 0,X
    LDX slide_anim_line
    ABX
    ABX
    ROR 0,X
    LDX slide_anim_line
    ABX
    ROR 0,X
    LDX slide_anim_line
    ROR 0,X
    BRA slide_anim_bit_next
slide_anim_down:
    ABX
    ABX
    ABX
    LDAA 0,X
    ASLA
    LDX slide_anim_line
    ROL 0,X
    ABX
    ROL 0,X
    ABX
    ROL 0,X
    ABX
    ROL 0,X
slide_anim_bit_next:
    LDX slide_anim_line
    DEC slide_anim_bits
    BNE slide_anim_bit
    INX
    DEC slide_anim_pixels
    BNE slide_anim_column
    JMP slide_anim_dirty
.section .text, code
slide_anim_dirty:
    ; Mark the small rectangle, including partial 32-byte transfer spans.
    LDAA slide_anim_band
    STAA slide_anim_row_band
    LDAA #2
    TST slide_anim_vertical
    BEQ slide_anim_dirty_rows
    ASLA
slide_anim_dirty_rows:
    STAA slide_anim_rows
slide_anim_mark:
    LDAA slide_anim_row_band
    LDAB slide_anim_x
    JSR dirty_mark
    LDAA slide_anim_row_band
    LDAB slide_anim_x
    ADDB #31
    JSR dirty_mark
    TST slide_anim_vertical
    BNE slide_anim_mark_next
    LDAA slide_anim_row_band
    LDAB slide_anim_x
    ADDB #63
    JSR dirty_mark
slide_anim_mark_next:
    INC slide_anim_row_band
    DEC slide_anim_rows
    BNE slide_anim_mark
    JSR slide_anim_flush
slide_anim_frame:
    LDAA input_ticks
    STAA slide_anim_clock
slide_anim_wait:
    JSR input_poll
    LDAA input_ticks
    SUBA slide_anim_clock
    CMPA #6
    BCS slide_anim_wait
    INC slide_anim_step
    LDAA slide_anim_step
    CMPA #3
    BEQ slide_anim_done
    JMP slide_anim_next
slide_anim_done:
    ; The cache must repaint both tile images after their pixel translation.
    CLRB
slide_anim_invalidate:
    LDX #view_cells
    ABX
    LDAA 0,X
    CMPA #255
    BEQ slide_anim_cache_next
    LDX #tile_cache
    ABX
    LDAA #255
    STAA 0,X
slide_anim_cache_next:
    INCB
    CMPB #112
    BNE slide_anim_invalidate
    RTS
slide_anim_flush:
    JSR dirty_begin
slide_anim_transfer:
    JSR dirty_next
    BEQ slide_anim_sum
    JSR input_poll
    BRA slide_anim_transfer
slide_anim_sum:
    LDD dirty_bytes
    ADDD slide_anim_bytes
    STD slide_anim_bytes
    RTS
.section .bss, bss
slide_anim_bytes: .space 2
slide_anim_origin: .space 2
slide_anim_line: .space 2
slide_anim_step: .space 1
slide_anim_reverse: .space 1
slide_anim_vertical: .space 1
slide_anim_rows: .space 1
slide_anim_pixels: .space 1
slide_anim_temp: .space 1
slide_anim_x: .space 1
slide_anim_band: .space 1
slide_anim_row_band: .space 1
slide_anim_clock: .space 1
slide_anim_bits: .space 1
