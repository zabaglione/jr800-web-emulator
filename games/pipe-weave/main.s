; SPDX-License-Identifier: MIT
.global pipe_wet
.global pipe_leaks
.global pipe_trace
.global pipe_connections
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    LDAA stage
    LDAB #36
    MUL
    ADDD #pipe_levels
    STD pipe_pointer
    CLR pipe_cell
pipe_load:
    LDX pipe_pointer
    LDAA 0,X
    INX
    STX pipe_pointer
    LDAB pipe_cell
    LDX #board
    ABX
    STAA 0,X
    INC pipe_cell
    LDAA pipe_cell
    CMPA #36
    BNE pipe_load
    JMP pipe_trace
game_update:
    LDAA input_event
    BITA #16
    BNE pipe_turn
    JSR grid_move
    CMPB #255
    BEQ pipe_idle
    STAB cursor
    JMP grid_changed
pipe_turn:
    LDAB cursor
    LDX #board
    ABX
    LDAB 0,X
    CMPB #15
    BEQ pipe_idle
    JSR grid_snapshot
    LDAB cursor
    LDX #board
    ABX
    LDAB 0,X
    LDX #pipe_rotation
    ABX
    LDAA 0,X
    LDAB cursor
    LDX #board
    ABX
    STAA 0,X
    JSR grid_count_move
    JMP pipe_trace
pipe_idle:
    RTS
pipe_trace:
    CLR pipe_leaks
    CLR pipe_cell
    LDX #pipe_connections
    STX pipe_pointer
pipe_link_cell:
    CLR pipe_dir
pipe_link_dir:
    LDAB pipe_dir
    LDX #pipe_bits
    ABX
    LDAA 0,X
    STAA pipe_mask
    LDX #pipe_opposite
    ABX
    LDAA 0,X
    STAA pipe_other
    LDAB pipe_cell
    LDX #board
    ABX
    LDAA 0,X
    BITA pipe_mask
    BEQ pipe_disconnected
    LDAA pipe_dir
    LDAB #36
    MUL
    ADDB pipe_cell
    LDX #neighbors
    ABX
    LDAB 0,X
    CMPB #255
    BEQ pipe_leak
    STAB pipe_next
    LDX #board
    ABX
    LDAA 0,X
    BITA pipe_other
    BEQ pipe_leak
    LDAA pipe_next
    BRA pipe_store_link
pipe_leak:
    INC pipe_leaks
pipe_disconnected:
    LDAA #255
pipe_store_link:
    LDX pipe_pointer
    STAA 0,X
    INX
    STX pipe_pointer
    INC pipe_dir
    LDAA pipe_dir
    CMPA #4
    BNE pipe_link_dir
    JSR input_poll
    INC pipe_cell
    LDAA pipe_cell
    CMPA #36
    BNE pipe_link_cell
    LDX #pipe_wet
    LDAB #36
    CLRA
pipe_dry_all:
    STAA 0,X
    INX
    DECB
    BNE pipe_dry_all
    CLR pipe_head
    CLR pipe_queue
    LDAA #1
    STAA pipe_tail
    STAA pipe_wet
    LDAA #35
    STAA grid_stat
pipe_visit:
    LDAB pipe_head
    LDX #pipe_queue
    ABX
    LDAA 0,X
    LDAB #4
    MUL
    ADDD #pipe_connections
    STD pipe_pointer
    CLR pipe_dir
pipe_visit_link:
    LDX pipe_pointer
    LDAB pipe_dir
    ABX
    LDAB 0,X
    CMPB #255
    BEQ pipe_visit_next
    STAB pipe_next
    LDX #pipe_wet
    ABX
    TST 0,X
    BNE pipe_visit_next
    LDAA #1
    STAA 0,X
    DEC grid_stat
    LDAB pipe_tail
    LDX #pipe_queue
    ABX
    LDAA pipe_next
    STAA 0,X
    INC pipe_tail
pipe_visit_next:
    INC pipe_dir
    LDAA pipe_dir
    CMPA #4
    BNE pipe_visit_link
    JSR input_poll
    INC pipe_head
    LDAA pipe_head
    CMPA pipe_tail
    BNE pipe_visit
    TST grid_stat
    BNE pipe_idle_return
    TST pipe_leaks
    BNE pipe_idle_return
    LDAA #4
    STAA phase
pipe_idle_return:
    RTS
game_aux:
    CMPA #1
    BNE pipe_restart
    JSR grid_restore
    JMP pipe_trace
pipe_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    TSTB
    BEQ pipe_source_tile
    CMPB #35
    BEQ pipe_exit_tile
    LDX #pipe_wet
    ABX
    TST 0,X
    BEQ pipe_tile_done
    ADDA #16
pipe_tile_done:
    RTS
pipe_source_tile:
    TAB
    LDX #pipe_ports
    ABX
    LDAA 0,X
    ADDA #32
    RTS
pipe_exit_tile:
    TAB
    LDX #pipe_ports
    ABX
    LDAA 0,X
    ADDA #40
    RTS
game_render:
    JSR paint_board
    JMP visual_hud

.section .bss, bss
pipe_wet: .space 36
pipe_connections: .space 144
pipe_queue: .space 36
pipe_head: .space 1
pipe_tail: .space 1
pipe_leaks: .space 1
pipe_cell: .space 1
pipe_dir: .space 1
pipe_pointer: .space 2
pipe_mask: .space 1
pipe_other: .space 1
pipe_next: .space 1
.section .data, data
pipe_bits: .byte 1,2,4,8
pipe_opposite: .byte 2,1,8,4
pipe_leak_label: .byte 76,69,65,75,0
pipe_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
pipe_turn_label: .byte 84,85,82,78,0
