; SPDX-License-Identifier: MIT
.global board
.global player
.global moves
.global undo_valid
.section .text, code
game_start:
    LDAA stage
    LDAB #113
    MUL
    ADDD #levels
    STD box_source
    LDX box_source
    LDAA 0,X
    STAA player
    INX
    STX box_source
    LDX #board
    STX box_dest
    LDAB #112
box_load:
    LDX box_source
    LDAA 0,X
    INX
    STX box_source
    LDX box_dest
    STAA 0,X
    INX
    STX box_dest
    DECB
    BNE box_load
    CLR moves
    CLR moves + 1
    CLR undo_valid
    CLR show_help
    RTS
game_update:
    LDAA input_event
    BITA #1
    BEQ box_down
    LDAB #$F0
    BRA box_move
box_down:
    BITA #2
    BEQ box_left
    LDAB #16
    BRA box_move
box_left:
    BITA #4
    BEQ box_right
    LDAB #$FF
    BRA box_move
box_right:
    BITA #8
    BNE box_right_move
    RTS
box_right_move:
    LDAB #1
box_move:
    STAB box_delta
    ADDB player
    STAB box_next
    LDX #board
    ABX
    LDAA 0,X
    BITA #1
    BEQ box_not_wall
    RTS
box_not_wall:
    BITA #4
    BEQ box_valid
    ADDB box_delta
    STAB box_beyond
    LDX #board
    ABX
    LDAA 0,X
    ANDA #5
    BEQ box_valid
    RTS
box_valid:
    ; One complete prior logical state, not a second framebuffer.
    LDX #board
    STX box_source
    LDX #undo_board
    STX box_dest
    LDAB #112
box_copy_undo:
    LDX box_source
    LDAA 0,X
    INX
    STX box_source
    LDX box_dest
    STAA 0,X
    INX
    STX box_dest
    DECB
    BNE box_copy_undo
    LDAA player
    STAA undo_player
    LDD moves
    STD undo_moves
    LDAA #1
    STAA undo_valid
    LDAB box_next
    LDX #board
    ABX
    LDAA 0,X
    BITA #4
    BEQ box_walk
    ANDA #$FB
    STAA 0,X
    LDAB box_beyond
    LDX #board
    ABX
    LDAA 0,X
    ORAA #4
    STAA 0,X
box_walk:
    LDAA box_next
    STAA player
    LDD moves
    SUBD #9999
    BCC box_moves_done
    LDD moves
    ADDD #1
    STD moves
box_moves_done:
    LDAA #1
    STAA redraw
    LDX #board
    LDAB #112
box_check:
    LDAA 0,X
    ANDA #6
    CMPA #4
    BEQ box_idle
    INX
    DECB
    BNE box_check
    LDAA #4
    STAA phase
box_idle:
    RTS
game_aux:
    CMPA #1
    BNE box_help
    TST undo_valid
    BEQ box_idle
    LDX #undo_board
    STX box_source
    LDX #board
    STX box_dest
    LDAB #112
box_restore:
    LDX box_source
    LDAA 0,X
    INX
    STX box_source
    LDX box_dest
    STAA 0,X
    INX
    STX box_dest
    DECB
    BNE box_restore
    LDAA undo_player
    STAA player
    LDD undo_moves
    STD moves
    CLR undo_valid
    RTS
box_help:
    LDAA show_help
    EORA #1
    STAA show_help
    RTS
game_tile:
    CMPB player
    BNE box_map_tile
    LDAA #8
    RTS
box_map_tile:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
board: .space 112
undo_board: .space 112
player: .space 1
undo_player: .space 1
moves: .space 2
undo_moves: .space 2
undo_valid: .space 1
box_source: .space 2
box_dest: .space 2
box_delta: .space 1
box_next: .space 1
box_beyond: .space 1
show_help: .space 1
.section .data, data
box_heading: .byte 66,79,88,32,83,72,73,70,84,32,32,32,83,84,65,71,69,0 ; BOX SHIFT   STAGE
box_moves_label: .byte 77,79,86,69,83,0 ; MOVES
box_menu_label: .byte 82,69,84,85,82,78,0 ; RETURN
box_help_label: .byte 66,79,88,32,84,79,32,79,0 ; BOX TO O
