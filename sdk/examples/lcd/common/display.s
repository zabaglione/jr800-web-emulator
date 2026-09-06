; SPDX-License-Identifier: MIT
; JR-800 provisional LCD mapping: E-184, E-185, E-186; U-011 remains open.
; Scratch $80-$91; stack $5FFF. All routines clobber A, B, X.
.equ src, $80
.equ dest, $82
.equ table, $84
.equ port, $86
.equ remaining, $88
.equ text_ptr, $89
.equ glyph_ptr, $8B
.equ text_dest, $8D
.equ glyph_count, $8F
.equ controllers, $90
.equ page_command, $91
.global init
.global clear
.global present
.global text
.global glyph
.global delay
.global framebuffer
.extern font

.section .text, code
init:
    SEI
    ; The caller sets SP before JSR. Never reset SP inside a subroutine.
    LDX #$0A01
    STX port
    LDAB #8
    STAB controllers
init_controller:
    LDX port
    LDAA #$39
    STAA 0,X
    LDAA #$3E
    STAA 0,X
    LDAA #$3B
    STAA 0,X
    CLRA
    STAA page_command
init_page:
    LDX port
    LDAA page_command
    STAA 0,X
    INC port
    LDX port
    CLRA
    LDAB #50
init_column:
    STAA 0,X
    DECB
    BNE init_column
    DEC port
    LDAA page_command
    ADDA #64
    STAA page_command
    BNE init_page
    ASL port + 1
    DEC controllers
    BNE init_controller
    RTS

clear:
    LDX #framebuffer
    CLRA
clear_byte:
    STAA 0,X
    INX
    CPX #framebuffer + 1536
    BNE clear_byte
    RTS

; Flush 8 bands of 192 vertical bytes through 32 LCD spans.
; Each descriptor: control port (word), direction, XY command, byte count.
present:
    LDX #framebuffer
    STX src
    LDX #spans
    STX table
present_span:
    LDX table
    LDAA 0,X
    STAA port
    LDAA 1,X
    STAA port + 1
    LDAB 4,X
    STAB remaining
    LDAA 2,X
    LDX port
    STAA 0,X
    LDX table
    LDAA 3,X
    LDAB #5
    ABX
    STX table
    LDX port
    STAA 0,X
    INC port
present_byte:
    LDX src
    LDAA 0,X
    INX
    STX src
    LDX port
    STAA 0,X
    DEC remaining
    BNE present_byte
    LDX table
    CPX #spans_end
    BNE present_span
    RTS

; X = zero-terminated ASCII text; D = destination framebuffer address.
; Caller must keep the whole string inside one 192-byte band.
text:
    STX text_ptr
    STD text_dest
text_next:
    LDX text_ptr
    LDAA 0,X
    BEQ text_done
    INX
    STX text_ptr
    LDX text_dest
    JSR glyph
    LDX text_dest
    LDAB #6
    ABX
    STX text_dest
    BRA text_next
text_done:
    RTS

; A = ASCII $20-$5A, X = destination; five columns plus one blank.
glyph:
    STX dest
    SUBA #32
    LDAB #5
    MUL
    ; D keeps the full 16-bit offset, including X/Y/Z beyond byte 255.
    ADDD #font
    STD glyph_ptr
    LDAA #5
    STAA glyph_count
glyph_column:
    LDX glyph_ptr
    LDAA 0,X
    INX
    STX glyph_ptr
    LDX dest
    STAA 0,X
    INX
    STX dest
    DEC glyph_count
    BNE glyph_column
    CLRA
    STAA 0,X
    RTS

; 20,000 * (DEX 1 + BNE 3) cycles, about 65 ms at E-030 nominal rate.
; Frame drawing adds its own cycles; this is not a precision clock.
delay:
    LDX #20000
delay_loop:
    DEX
    BNE delay_loop
    RTS

.section .frame, bss
framebuffer:
    .space 1536

.section .data, data
spans:
    ; Upper half: reverse columns; four hidden columns at each panel edge.
    .word $0A01
    .byte $3A, $2D, 46
    .word $0A02
    .byte $3A, $31, 50
    .word $0A04
    .byte $3A, $31, 50
    .word $0A08
    .byte $3A, $31, 46
    .word $0A01
    .byte $3A, $6D, 46
    .word $0A02
    .byte $3A, $71, 50
    .word $0A04
    .byte $3A, $71, 50
    .word $0A08
    .byte $3A, $71, 46
    .word $0A01
    .byte $3A, $AD, 46
    .word $0A02
    .byte $3A, $B1, 50
    .word $0A04
    .byte $3A, $B1, 50
    .word $0A08
    .byte $3A, $B1, 46
    .word $0A01
    .byte $3A, $ED, 46
    .word $0A02
    .byte $3A, $F1, 50
    .word $0A04
    .byte $3A, $F1, 50
    .word $0A08
    .byte $3A, $F1, 46
    ; Lower half: forward columns.
    .word $0A10
    .byte $3B, $04, 46
    .word $0A20
    .byte $3B, $00, 50
    .word $0A40
    .byte $3B, $00, 50
    .word $0A80
    .byte $3B, $00, 46
    .word $0A10
    .byte $3B, $44, 46
    .word $0A20
    .byte $3B, $40, 50
    .word $0A40
    .byte $3B, $40, 50
    .word $0A80
    .byte $3B, $40, 46
    .word $0A10
    .byte $3B, $84, 46
    .word $0A20
    .byte $3B, $80, 50
    .word $0A40
    .byte $3B, $80, 50
    .word $0A80
    .byte $3B, $80, 46
    .word $0A10
    .byte $3B, $C4, 46
    .word $0A20
    .byte $3B, $C0, 50
    .word $0A40
    .byte $3B, $C0, 50
    .word $0A80
    .byte $3B, $C0, 46
spans_end:
