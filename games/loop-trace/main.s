; SPDX-License-Identifier: MIT
.global loop_marks
.global loop_paths
.global loop_next
.global loop_undo
.section .text, code
game_start:
    JSR grid_reset
    LDX #loop_marks
    LDAB #72
    CLRA
loop_clear:
    STAA 0,X
    INX
    DECB
    BNE loop_clear
    LDAA #1
    STAA loop_next
    LDAA stage
    LDAB #36
    MUL
    ADDD #loop_levels
    STD loop_pointer
    CLR loop_cell
loop_load:
    LDX loop_pointer
    LDAA 0,X
    INX
    STX loop_pointer
    LDAB loop_cell
    LDX #board
    ABX
    STAA 0,X
    TSTA
    BEQ loop_load_next
    INC grid_stat
    CMPA #5
    BNE loop_load_next
    STAB cursor
loop_load_next:
    INC loop_cell
    LDAA loop_cell
    CMPA #36
    BNE loop_load
    DEC grid_stat
    LDAB cursor
    STAB loop_history
    LDX #loop_marks
    ABX
    LDAA #1
    STAA 0,X
    RTS
game_update:
    LDAA input_event
    BITA #16
    BEQ loop_input_direction
    JMP loop_close
loop_input_direction:
    CLR loop_direction
    BITA #1
    BNE loop_move
    INC loop_direction
    BITA #2
    BNE loop_move
    INC loop_direction
    BITA #4
    BNE loop_move
    INC loop_direction
    BITA #8
    BNE loop_move
    RTS
loop_move:
    JSR grid_move
    CMPB #255
    BEQ loop_idle
    STAB loop_to
    LDX #board
    ABX
    TST 0,X
    BEQ loop_idle
    LDX #loop_marks
    ABX
    TST 0,X
    BEQ loop_unvisited
    TST moves
    BEQ loop_idle
    LDAB moves
    DECB
    LDX #loop_history
    ABX
    LDAA 0,X
    CMPA loop_to
    BNE loop_idle
    JMP loop_undo
loop_unvisited:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BCS loop_accept
    DECA
    CMPA loop_next
    BNE loop_idle
    INC loop_next
loop_accept:
    LDAA cursor
    STAA loop_from
    JSR loop_connect
    LDAA loop_to
    STAA cursor
    INC moves
    DEC grid_stat
    LDAB moves
    LDX #loop_history
    ABX
    STAA 0,X
    LDX #loop_directions
    ABX
    LDAA loop_direction
    STAA 0,X
    LDAB cursor
    LDX #loop_marks
    ABX
    LDAA #1
    STAA 0,X
    JMP grid_changed
loop_idle:
    RTS
loop_connect:
    LDAB loop_direction
    LDX #loop_bits
    ABX
    LDAA 0,X
    STAA loop_mask
    LDX #loop_opposite
    ABX
    LDAA 0,X
    LDAB loop_to
    LDX #loop_paths
    ABX
    ORAA 0,X
    STAA 0,X
    LDAB loop_from
    LDX #loop_paths
    ABX
    LDAA loop_mask
    ORAA 0,X
    STAA 0,X
    RTS
loop_undo:
    TST moves
    BEQ loop_idle
    LDAB cursor
    LDX #loop_marks
    ABX
    CLR 0,X
    LDX #loop_paths
    ABX
    CLR 0,X
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BCS loop_undo_path
    DEC loop_next
loop_undo_path:
    LDAB moves
    LDX #loop_directions
    ABX
    LDAB 0,X
    LDX #loop_bits
    ABX
    LDAA 0,X
    COMA
    STAA loop_mask
    DEC moves
    INC grid_stat
    LDAB moves
    LDX #loop_history
    ABX
    LDAB 0,X
    STAB cursor
    LDX #loop_paths
    ABX
    LDAA 0,X
    ANDA loop_mask
    STAA 0,X
    JMP grid_changed
loop_close:
    TST grid_stat
    BNE loop_close_done
    LDAA loop_next
    CMPA #4
    BNE loop_close_done
    CLR loop_direction
loop_closing_direction:
    LDAA loop_direction
    LDAB #36
    MUL
    ADDB cursor
    LDX #neighbors
    ABX
    LDAA 0,X
    CMPA loop_history
    BEQ loop_closed
    INC loop_direction
    LDAA loop_direction
    CMPA #4
    BNE loop_closing_direction
loop_close_done:
    RTS
loop_closed:
    STAA loop_to
    LDAA cursor
    STAA loop_from
    JSR loop_connect
    LDAA #4
    STAA phase
    JMP grid_changed
game_aux:
    CMPA #1
    BNE loop_restart
    JMP loop_undo
loop_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    BEQ loop_tile_done
    CMPA #5
    BEQ loop_tile_done
    LDX #loop_marks
    ABX
    TST 0,X
    BEQ loop_tile_done
    LDX #loop_paths
    ABX
    LDAA 0,X
    ADDA #6
loop_tile_done:
    RTS
game_render:
    JSR grid_render
    LDX #loop_next_label
    LDAA #84
    CLRB
    JSR paint_text
    LDAA loop_next
    ADDA #64
    CMPA #68
    BNE loop_checkpoint_letter
    LDAA #45
loop_checkpoint_letter:
    STAA loop_letter
    LDX #loop_letter
    LDAA #114
    CLRB
    JSR paint_text
    LDX #loop_blank_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #loop_draw_label
    TST grid_stat
    BNE loop_status
    LDX #loop_close_label
loop_status:
    LDAA #132
    LDAB #5
    JMP paint_text
.section .bss, bss
loop_marks: .space 36
loop_paths: .space 36
loop_history: .space 36
loop_directions: .space 36
loop_next: .space 1
loop_cell: .space 1
loop_pointer: .space 2
loop_direction: .space 1
loop_from: .space 1
loop_to: .space 1
loop_mask: .space 1
loop_letter: .space 2
.section .data, data
loop_bits: .byte 1,2,4,8
loop_opposite: .byte 2,1,8,4
loop_next_label: .byte 78,69,88,84,0
loop_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
loop_draw_label: .byte 68,82,65,87,0
loop_close_label: .byte 67,76,79,83,69,0
