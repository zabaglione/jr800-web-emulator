; SPDX-License-Identifier: MIT
; One reversible board action. Invalid moves leave the previous snapshot intact.
.global undo_valid
.section .text, code
grid_snapshot:
    LDX #board
    STX snapshot_source
    LDX #snapshot_board
    STX snapshot_dest
    JSR snapshot_copy
    LDAA cursor
    STAA snapshot_cursor
    LDAA moves
    STAA snapshot_moves
    LDAA grid_stat
    STAA snapshot_stat
    LDAA #1
    STAA undo_valid
    RTS
grid_restore:
    TST undo_valid
    BEQ snapshot_done
    LDX #snapshot_board
    STX snapshot_source
    LDX #board
    STX snapshot_dest
    JSR snapshot_copy
    LDAA snapshot_cursor
    STAA cursor
    LDAA snapshot_moves
    STAA moves
    LDAA snapshot_stat
    STAA grid_stat
    CLR undo_valid
snapshot_done:
    RTS
snapshot_copy:
    LDAB #112
snapshot_loop:
    LDX snapshot_source
    LDAA 0,X
    INX
    STX snapshot_source
    LDX snapshot_dest
    STAA 0,X
    INX
    STX snapshot_dest
    DECB
    BNE snapshot_loop
    RTS
.section .bss, bss
undo_valid: .space 1
snapshot_cursor: .space 1
snapshot_moves: .space 1
snapshot_stat: .space 1
snapshot_board: .space 112
snapshot_source: .space 2
snapshot_dest: .space 2
