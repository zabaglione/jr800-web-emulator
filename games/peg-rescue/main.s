; SPDX-License-Identifier: MIT
.global peg_source
.global peg_middle
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR selection_active
    LDAA #255
    STAA cursor
    LDAA stage
    LDAB #49
    MUL
    ADDD #peg_levels
    STD peg_pointer
    CLR peg_index
peg_load:
    LDX peg_pointer
    LDAA 0,X
    INX
    STX peg_pointer
    LDAB peg_index
    LDX #board
    ABX
    STAA 0,X
    CMPA #1
    BNE peg_load_next
    INC grid_stat
    LDAA cursor
    CMPA #255
    BNE peg_load_next
    STAB cursor
peg_load_next:
    INC peg_index
    LDAA peg_index
    CMPA #49
    BNE peg_load
    RTS
game_update:
    LDAA input_event
    BITA #16
    BNE peg_action
    JSR grid_move
    CMPB #255
    BEQ peg_idle
    LDX #board
    ABX
    LDAA 0,X
    CMPA #255
    BEQ peg_idle
    STAB cursor
    JMP grid_changed
peg_action:
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ peg_select
    TST selection_active
    BEQ peg_idle
    LDX #peg_middle
    ABX
    LDAA 0,X
    CMPA #255
    BEQ peg_idle
    STAA peg_capture
    JSR grid_snapshot
    LDAB peg_source
    LDX #board
    ABX
    CLR 0,X
    LDAB peg_capture
    LDX #board
    ABX
    CLR 0,X
    LDAB cursor
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    CLR selection_active
    INC moves
    DEC grid_stat
    JSR grid_changed
    LDAA grid_stat
    CMPA #1
    BNE peg_idle
    LDAA #4
    STAA phase
peg_idle:
    RTS
peg_select:
    STAB peg_source
    LDAA #1
    STAA selection_active
    JSR peg_options
    JMP grid_changed
peg_options:
    LDX #peg_middle
    LDAB #49
    LDAA #255
peg_clear:
    STAA 0,X
    INX
    DECB
    BNE peg_clear
    LDAA peg_source
    LDAB #8
    MUL
    ADDD #peg_links
    STD peg_pointer
    CLR peg_index
peg_option_loop:
    LDX peg_pointer
    LDAB peg_index
    ABX
    LDAA 0,X
    LDAB 1,X
    CMPA #255
    BEQ peg_option_next
    STAA peg_capture
    STAB peg_target
    LDX #board
    ABX
    TST 0,X
    BNE peg_option_next
    LDAB peg_capture
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE peg_option_next
    LDAB peg_target
    LDX #peg_middle
    ABX
    LDAA peg_capture
    STAA 0,X
peg_option_next:
    INC peg_index
    INC peg_index
    LDAA peg_index
    CMPA #8
    BNE peg_option_loop
    RTS
game_aux:
    CMPA #1
    BNE peg_reset
    CLR selection_active
    JMP grid_restore
peg_reset:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #255
    BNE peg_visible
    LDAA #2
    RTS
peg_visible:
    TST selection_active
    BEQ peg_tile_done
    CMPB peg_source
    BNE peg_hint
    LDAA #3
    RTS
peg_hint:
    TSTA
    BNE peg_tile_done
    LDX #peg_middle
    ABX
    LDAA 0,X
    CMPA #255
    BEQ peg_empty
    LDAA #4
    RTS
peg_empty:
    CLRA
peg_tile_done:
    RTS
game_render:
    JSR paint_board
    JMP visual_hud

.section .bss, bss
peg_source: .space 1
peg_middle: .space 49
peg_pointer: .space 2
peg_index: .space 1
peg_capture: .space 1
peg_target: .space 1
.section .data, data
peg_clear_label: .byte 32,32,32,32,32,32,32,32,32,32,0
peg_pick_label: .byte 80,73,67,75,0
peg_jump_label: .byte 74,85,77,80,0
