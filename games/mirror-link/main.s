; SPDX-License-Identifier: MIT
.global board
.global cursor
.global beams
.global lit_count
.global goal_count
.global rotations
.section .text, code
game_start:
    LDAA stage
    LDAB #116
    MUL
    ADDD #levels
    STD mirror_source
    LDX #sources
    STX mirror_dest
    LDAB #116
mirror_load:
    LDX mirror_source
    LDAA 0,X
    INX
    STX mirror_source
    LDX mirror_dest
    STAA 0,X
    INX
    STX mirror_dest
    DECB
    BNE mirror_load
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
    LDAA ray_tile
    CMPA #2
    BEQ ray_slash
    CMPA #3
    BEQ ray_backslash
    CMPA #4
    BEQ ray_end
    CMPA #5
    BCC ray_end
    BRA ray_step
ray_slash:
    LDAA ray_dir
    EORA #3
    STAA ray_dir
    BRA ray_step
ray_backslash:
    LDAA ray_dir
    EORA #1
    STAA ray_dir
    BRA ray_step
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
game_render:
    JSR paint_board
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
mirror_source: .space 2
mirror_dest: .space 2
mirror_tile_cell: .space 1
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
mirror_heading: .byte 77,73,82,82,79,82,32,76,73,78,75,0 ; MIRROR LINK
mirror_lit_label: .byte 76,73,84,0 ; LIT
mirror_space_label: .byte 83,80,65,67,69,0 ; SPACE
mirror_turn_label: .byte 82,79,84,65,84,69,0 ; ROTATE
mirror_return_label: .byte 82,69,84,85,82,78,0 ; RETURN
