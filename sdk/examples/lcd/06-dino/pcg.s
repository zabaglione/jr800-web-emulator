; SPDX-License-Identifier: MIT
; Software PCG: writable 16x16 patterns, two vertical bytes per column.
; No dedicated PCG hardware, ROM glyphs or external sprite sheet is used.
.global blit
.extern framebuffer
.equ pattern_ptr, $92
.equ band_ptr, $94
.equ pattern_x, $96
.equ pattern_y, $97
.equ pattern_shift, $98
.equ pattern_columns, $99
.equ column_bits, $9A

.section .text, code
; X = pattern, A = x (-15..191, wrapping byte), B = y (0..40).
; Transparent OR blit, clips horizontally, clobbers A/B/X and $92-$9C.
blit:
    STX pattern_ptr
    STAA pattern_x
    STAB pattern_y
    TBA
    ANDA #7
    STAA pattern_shift
    TBA
    LSRA
    LSRA
    LSRA
    LDAB #192
    MUL
    ADDD #framebuffer
    STD band_ptr
    LDAA #16
    STAA pattern_columns
pattern_column:
    LDX pattern_ptr
    LDAA 0,X
    STAA column_bits
    LDAA 1,X
    STAA column_bits + 1
    CLR column_bits + 2
    INX
    INX
    STX pattern_ptr
    LDAA pattern_x
    CMPA #192
    BCC pattern_next
    LDAB pattern_shift
    BEQ pattern_copy
pattern_align:
    ASL column_bits
    ROL column_bits + 1
    ROL column_bits + 2
    DECB
    BNE pattern_align
pattern_copy:
    LDX band_ptr
    LDAB pattern_x
    ABX
    LDAA 0,X
    ORAA column_bits
    STAA 0,X
    LDAB #192
    ABX
    LDAA 0,X
    ORAA column_bits + 1
    STAA 0,X
    ABX
    LDAA 0,X
    ORAA column_bits + 2
    STAA 0,X
pattern_next:
    INC pattern_x
    DEC pattern_columns
    BNE pattern_column
    RTS

.section .data, data
; Original row sketches below are the editable source for each RAM pattern.
.global dino_run_a
dino_run_a:
; ........########
; .......#########
; .......##.######
; .......#########
; .......#####....
; .......#######..
; ......#####.....
; #....########...
; ##..#########...
; ############....
; .###########....
; ..##########....
; ...########.....
; ....##..###.....
; ....##....##....
; ....###.........
    .byte $80, $03
    .byte $00, $07
    .byte $00, $0E
    .byte $00, $1E
    .byte $00, $FF
    .byte $80, $FF
    .byte $C0, $9F
    .byte $FE, $1F
    .byte $FF, $3F
    .byte $FB, $3F
    .byte $FF, $7F
    .byte $BF, $4F
    .byte $AF, $01
    .byte $2F, $00
    .byte $0F, $00
    .byte $0F, $00
.global dino_run_b
dino_run_b:
; ........########
; .......#########
; .......##.######
; .......#########
; .......#####....
; .......#######..
; ......#####.....
; #....########...
; ##..#########...
; ############....
; .###########....
; ..##########....
; ...########.....
; ....###..##.....
; ...##....##.....
; .........###....
    .byte $80, $03
    .byte $00, $07
    .byte $00, $0E
    .byte $00, $5E
    .byte $00, $7F
    .byte $80, $3F
    .byte $C0, $3F
    .byte $FE, $1F
    .byte $FF, $1F
    .byte $FB, $FF
    .byte $FF, $FF
    .byte $BF, $8F
    .byte $AF, $01
    .byte $2F, $00
    .byte $0F, $00
    .byte $0F, $00
.global dino_air
dino_air:
; ........########
; .......#########
; .......##.######
; .......#########
; .......#####....
; .......#######..
; ......#####.....
; #....########...
; ##..#########...
; ############....
; .###########....
; ..##########....
; ...########.....
; ....###.###.....
; .....##..##.....
; .....##..##.....
    .byte $80, $03
    .byte $00, $07
    .byte $00, $0E
    .byte $00, $1E
    .byte $00, $3F
    .byte $80, $FF
    .byte $C0, $FF
    .byte $FE, $1F
    .byte $FF, $3F
    .byte $FB, $FF
    .byte $FF, $FF
    .byte $BF, $0F
    .byte $AF, $01
    .byte $2F, $00
    .byte $0F, $00
    .byte $0F, $00
.global dino_dead
dino_dead:
; ........########
; .......#########
; .......#.#.#####
; .......#########
; .......#####....
; .......#######..
; ......#####.....
; #....########...
; ##..#########...
; ############....
; .###########....
; ..##########....
; ...########.....
; ....##..###.....
; ....##...##.....
; ....###..###....
    .byte $80, $03
    .byte $00, $07
    .byte $00, $0E
    .byte $00, $1E
    .byte $00, $FF
    .byte $80, $FF
    .byte $C0, $9F
    .byte $FE, $1F
    .byte $FB, $3F
    .byte $FF, $FF
    .byte $FB, $FF
    .byte $BF, $8F
    .byte $AF, $01
    .byte $2F, $00
    .byte $0F, $00
    .byte $0F, $00
.global cactus_a
cactus_a:
; ......##........
; ......##........
; ...#..##........
; ..##..##..##....
; ..##..##..##....
; ..##..##..##....
; ..######..##....
; ...#####..##....
; ......######....
; ......#####.....
; ......##........
; ......##........
; ......##........
; ......##........
; ......##........
; .....####.......
    .byte $00, $00
    .byte $00, $00
    .byte $78, $00
    .byte $FC, $00
    .byte $C0, $00
    .byte $C0, $80
    .byte $FF, $FF
    .byte $FF, $FF
    .byte $00, $83
    .byte $00, $03
    .byte $F8, $03
    .byte $F8, $01
    .byte $00, $00
    .byte $00, $00
    .byte $00, $00
    .byte $00, $00
.global cactus_b
cactus_b:
; ....##..........
; ....##..........
; ....##....##....
; .#..##....##....
; .##.##.##.##....
; .##.##.##.##....
; .#####.##.##....
; ..####.#####....
; ....##..####....
; ....##....##....
; ....##....##....
; ....##....##....
; ....##....##....
; ....##....##....
; ....##....##....
; ...####..####...
    .byte $00, $00
    .byte $78, $00
    .byte $F0, $00
    .byte $C0, $80
    .byte $FF, $FF
    .byte $FF, $FF
    .byte $00, $80
    .byte $F0, $00
    .byte $F0, $01
    .byte $80, $81
    .byte $FC, $FF
    .byte $FC, $FF
    .byte $00, $80
    .byte $00, $00
    .byte $00, $00
    .byte $00, $00
.global cloud
cloud:
; ......####......
; .....#....#.....
; ...##......##...
; ..#..........#..
; ##............##
; #..............#
; .##############.
; ................
; ................
; ................
; ................
; ................
; ................
; ................
; ................
; ................
    .byte $30, $00
    .byte $50, $00
    .byte $48, $00
    .byte $44, $00
    .byte $44, $00
    .byte $42, $00
    .byte $41, $00
    .byte $41, $00
    .byte $41, $00
    .byte $41, $00
    .byte $42, $00
    .byte $44, $00
    .byte $44, $00
    .byte $48, $00
    .byte $50, $00
    .byte $30, $00
