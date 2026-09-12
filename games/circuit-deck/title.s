; SPDX-License-Identifier: MIT
; Same title pixels, stored with the project's literal/back-reference packets.
; Title and dashboard artwork share a bounded decoder and one framebuffer.
show_title:
    CLR phase
    JSR input_gate
    JSR paint_clear
    LDX #title_art
    JSR deck_backdrop
    JMP fanfare
; X = a packed 192x64 screen. No additional framebuffer or scratch image.
deck_backdrop:
    STX deck_unpack_source
    LDX #framebuffer
    STX deck_unpack_dest
    JSR deck_unpack_packet
    JMP dirty_all
deck_unpack_packet:
    LDX deck_unpack_source
    LDAB 0,X
    BEQ deck_unpack_done
    INX
    STX deck_unpack_source
    TSTB
    BMI deck_unpack_repeat
    STAB deck_unpack_count
deck_unpack_literal:
    LDX deck_unpack_source
    LDAA 0,X
    INX
    STX deck_unpack_source
    LDX deck_unpack_dest
    STAA 0,X
    INX
    STX deck_unpack_dest
    DEC deck_unpack_count
    BNE deck_unpack_literal
    JSR input_poll
    BRA deck_unpack_packet
deck_unpack_repeat:
    SUBB #127
    STAB deck_unpack_count
    LDAA 0,X
    STAA deck_unpack_offset
    INX
    STX deck_unpack_source
    LDD deck_unpack_dest
    SUBB deck_unpack_offset
    SBCA #0
    STD deck_unpack_back
deck_unpack_run:
    LDX deck_unpack_back
    LDAA 0,X
    INX
    STX deck_unpack_back
    LDX deck_unpack_dest
    STAA 0,X
    INX
    STX deck_unpack_dest
    DEC deck_unpack_count
    BNE deck_unpack_run
    JSR input_poll
    BRA deck_unpack_packet
deck_unpack_done:
    RTS
.section .bss, bss
deck_unpack_source: .space 2
deck_unpack_dest: .space 2
deck_unpack_back: .space 2
deck_unpack_count: .space 1
deck_unpack_offset: .space 1
.section .text, code
