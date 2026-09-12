; SPDX-License-Identifier: MIT
.global board
.global cursor
.global beams
.global lit_count
.global goal_count
.global rotations
.global source_count
.global source_cells
.section .text, code
game_start:
    JSR challenge_start
    LDX #sources
    JSR challenge_load
    CLR rotations
    CLR undo_valid_mirror
    LDAA #17
    STAA cursor
    JMP trace_light
game_update:
    LDAA input_event
    BITA #16
    BEQ mirror_move
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BEQ mirror_rotate
    CMPA #3
    BNE mirror_idle
mirror_rotate:
    PSHA
    PSHB
    JSR challenge_step
    PULB
    PULA
    EORA #1
    STAA 0,X
    STAB undo_cell
    LDAA #1
    STAA undo_valid_mirror
    INC rotations
    JSR trace_light
    LDAA #1
    STAA redraw
    LDAA lit_count
    CMPA goal_count
    BNE mirror_idle
    LDAA #4
    STAA phase
    RTS
mirror_move:
    BITA #1
    BEQ mirror_down
    LDAB cursor
    CMPB #16
    BCS mirror_idle
    SUBB #16
    BRA mirror_cursor
mirror_down:
    BITA #2
    BEQ mirror_left
    LDAB cursor
    CMPB #96
    BCC mirror_idle
    ADDB #16
    BRA mirror_cursor
mirror_left:
    BITA #4
    BEQ mirror_right
    LDAA cursor
    ANDA #15
    BEQ mirror_idle
    LDAB cursor
    DECB
    BRA mirror_cursor
mirror_right:
    BITA #8
    BEQ mirror_idle
    LDAA cursor
    ANDA #15
    CMPA #15
    BEQ mirror_idle
    LDAB cursor
    INCB
mirror_cursor:
    STAB cursor
    LDAA #1
    STAA redraw
mirror_idle:
    RTS
game_aux:
    CMPA #1
    BNE mirror_reset
    TST undo_valid_mirror
    BEQ mirror_idle
    JSR challenge_undo
    LDAB undo_cell
    LDX #board
    ABX
    LDAA 0,X
    EORA #1
    STAA 0,X
    CLR undo_valid_mirror
    DEC rotations
    JMP trace_light
mirror_reset:
    JMP game_start
; Beam and visited arrays: one byte per cell; 4 direction bits stop cycles.
trace_light:
    LDX #beams
    CLRA
    LDAB #224
mirror_clear_ray:
    STAA 0,X
    INX
    DECB
    BNE mirror_clear_ray
    CLR ray_index
    CLR lit_count
    CLR goal_count
    LDX #board
    LDAB #112
mirror_count_goals:
    LDAA 0,X
    CMPA #4
    BNE mirror_count_next
    INC goal_count
mirror_count_next:
    INX
    DECB
    BNE mirror_count_goals
ray_source_next:
    LDAB ray_index
    CMPB source_count
    BCS ray_has_source
    JMP ray_all_done
ray_has_source:
    LDX #sources + 1
    ABX
    LDAA 0,X
    STAA ray_cell
    CLR ray_dir
    CLR ray_poll
ray_step:
    INC ray_poll
    LDAA ray_poll
    ANDA #15
    BNE ray_polled
    JSR input_poll
ray_polled:
    LDAB ray_dir
    LDX #ray_steps
    ABX
    LDAA 0,X
    ADDA ray_cell
    CMPA #112
    BCC ray_end
    STAA ray_cell
    TAB
    LDX #board
    ABX
    LDAA 0,X
    STAA ray_tile
    CMPA #1
    BEQ ray_end
    LDAB ray_dir
    LDX #ray_bits
    ABX
    LDAA 0,X
    STAA ray_bit
    LDAB ray_cell
    LDX #visited
    ABX
    BITA 0,X
    BNE ray_end
    ORAA 0,X
    STAA 0,X
    LDAA ray_dir
    ANDA #1
    INCA
    LDX #beams
    ABX
    ORAA 0,X
    STAA 0,X
    JSR challenge_touch
    LDAA ray_tile
    CMPA #2
    BEQ ray_slash
    CMPA #3
    BEQ ray_backslash
    CMPA #4
    BEQ ray_end
    CMPA #5
    BCC ray_end
    JMP ray_step
ray_slash:
    LDAA ray_dir
    EORA #3
    STAA ray_dir
    JMP ray_step
ray_backslash:
    LDAA ray_dir
    EORA #1
    STAA ray_dir
    JMP ray_step
ray_end:
    JSR input_poll
    INC ray_index
    JMP ray_source_next
ray_all_done:
    CLRB
mirror_lit_loop:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #4
    BNE mirror_lit_next
    LDX #beams
    ABX
    TST 0,X
    BEQ mirror_lit_next
    INC lit_count
mirror_lit_next:
    INCB
    CMPB #112
    BNE mirror_lit_loop
    RTS
game_tile:
    STAB mirror_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE mirror_tile_receiver
    JSR mirror_wall_tile
    BRA mirror_tile_cursor
mirror_tile_receiver:
    CMPA #4
    BNE mirror_tile_empty
    LDX #beams
    ABX
    TST 0,X
    BEQ mirror_tile_cursor
    LDAA #9
    BRA mirror_tile_cursor
mirror_tile_empty:
    TSTA
    BNE mirror_tile_cursor
    LDX #beams
    ABX
    LDAA 0,X
    BEQ mirror_tile_cursor
    ADDA #9
mirror_tile_cursor:
    LDAB mirror_tile_cell
    CMPB cursor
    BNE mirror_tile_done
    ORAA #128
mirror_tile_done:
    RTS
; Join shielding across wall cells; left/right/up/down are bits 0/1/2/3.
mirror_wall_tile:
    CLR mirror_wall_mask
    LDAB mirror_tile_cell
    ANDB #15
    BEQ mirror_wall_right
    LDAB mirror_tile_cell
    DECB
    JSR mirror_wall_test
    BNE mirror_wall_right
    INC mirror_wall_mask
mirror_wall_right:
    LDAB mirror_tile_cell
    ANDB #15
    CMPB #15
    BEQ mirror_wall_up
    LDAB mirror_tile_cell
    INCB
    JSR mirror_wall_test
    BNE mirror_wall_up
    LDAA mirror_wall_mask
    ORAA #2
    STAA mirror_wall_mask
mirror_wall_up:
    LDAB mirror_tile_cell
    CMPB #16
    BCS mirror_wall_down
    SUBB #16
    JSR mirror_wall_test
    BNE mirror_wall_down
    LDAA mirror_wall_mask
    ORAA #4
    STAA mirror_wall_mask
mirror_wall_down:
    LDAB mirror_tile_cell
    CMPB #96
    BCC mirror_wall_ready
    ADDB #16
    JSR mirror_wall_test
    BNE mirror_wall_ready
    LDAA mirror_wall_mask
    ORAA #8
    STAA mirror_wall_mask
mirror_wall_ready:
    LDAA mirror_wall_mask
    BEQ mirror_wall_single
    ADDA #12
    RTS
mirror_wall_single:
    LDAA #1
    RTS
mirror_wall_test:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    RTS
game_render:
    JSR paint_board
    TST hud_ready
    BNE mirror_render_values
    JSR hud_begin
; Inset the six label bands by one pixel on entry/menu return.
    LDAA #3
    STAA hud_field_6
    LDX #mirror_hud_bands
    STX mirror_hud_pointer
mirror_hud_next:
    LDX mirror_hud_pointer
    LDAA 0,X
    BEQ mirror_render_values
    STAA paint_band
    LDAA 1,X
    STAA paint_x
    LDAA 2,X
    PSHA
    INX
    INX
    INX
    STX mirror_hud_pointer
    JSR paint_address
    PULA
    LDX paint_dest
; Restore the first glyph column without any overlaid rail pixels.
    STAA 2,X
    LDAB #28
    ABX
    LDAB #27
mirror_hud_shift:
    LDAA 0,X
    STAA 1,X
    DEX
    DECB
    BNE mirror_hud_shift
; Column 2 separates the shortened rail ticks from the text at column 3.
    CLR 1,X
    LDAA paint_band
    LDAB paint_x
    JSR dirty_mark
    LDAA paint_band
    LDAB paint_x
    ADDB #31
    JSR dirty_mark
    BRA mirror_hud_next
mirror_render_values:
    JMP visual_hud
.section .bss, bss
sources:
source_count: .space 1
source_cells: .space 3
board: .space 112
beams: .space 112
visited: .space 112
cursor: .space 1
rotations: .space 1
undo_valid_mirror: .space 1
undo_cell: .space 1
mirror_hud_pointer: .space 2
mirror_tile_cell: .space 1
mirror_wall_mask: .space 1
ray_poll: .space 1
ray_index: .space 1
ray_cell: .space 1
ray_dir: .space 1
ray_tile: .space 1
ray_bit: .space 1
lit_count: .space 1
goal_count: .space 1
.section .data, data
ray_steps: .byte 1,16,255,240
ray_bits: .byte 1,2,4,8
; Band, side, first glyph column (L/G/U/P); footer text starts farther inward.
mirror_hud_bands: .byte 1,0,62,1,160,28,4,0,62,4,160,62,7,0,0,7,160,0,0

.section .text, code
game_bonus:
    RTS
