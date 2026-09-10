; SPDX-License-Identifier: MIT
.section .text, code
grid_render:
    JSR paint_board
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #grid_moves_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA moves
    JSR paint_number
    LDX #grid_stat_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA grid_stat
    JSR paint_number
    LDX #grid_action_label
    LDAA #138
    LDAB #6
    JSR paint_text
    LDX #grid_return_label
    LDAA #138
    LDAB #7
    JMP paint_text
