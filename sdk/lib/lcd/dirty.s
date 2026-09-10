; SPDX-License-Identifier: MIT
; Caller-owned single framebuffer; eight inclusive horizontal dirty ranges.
; LCD mapping and BUSY protocol are shared with display.s (E-184..186/E-447).
.global dirty_reset
.global dirty_all
.global dirty_mark
.global dirty_begin
.global dirty_next
.global dirty_pending
.global dirty_bytes
.global dirty_min
.global dirty_max
.extern framebuffer
.extern spans
.extern spans_end
.section .text, code
dirty_reset:
    CLR dirty_pending
    LDX #dirty_min
    LDAB #8
dirty_reset_loop:
    LDAA #192
    STAA 0,X
    CLR 8,X
    INX
    DECB
    BNE dirty_reset_loop
    RTS
dirty_all:
    LDAA #1
    STAA dirty_pending
    LDX #dirty_min
    LDAB #8
dirty_all_loop:
    CLR 0,X
    LDAA #191
    STAA 8,X
    INX
    DECB
    BNE dirty_all_loop
    RTS
; A = band 0..7, B = x 0..191. Clobbers A/B/X.
dirty_mark:
    STAA dirty_saved_band
    LDAA #1
    STAA dirty_pending
    LDAA dirty_saved_band
    STAB dirty_x
    TAB
    LDX #dirty_min
    ABX
    LDAA dirty_x
    CMPA 0,X
    BCC dirty_mark_max
    STAA 0,X
dirty_mark_max:
    CMPA 8,X
    BLS dirty_mark_done
    STAA 8,X
dirty_mark_done:
    RTS
dirty_begin:
    LDX #spans
    STX dirty_table
    LDX #framebuffer
    STX dirty_row
    CLR dirty_band
    CLR dirty_column
    CLR dirty_part
    CLR dirty_bytes
    CLR dirty_bytes + 1
    RTS
; Process at most one physical span; Z=1 after the last span.
; Input may be scanned between calls, without sharing this module's scratch.
dirty_next:
    LDX dirty_table
    LDD 0,X
    STD dirty_port
    ADDD #$0100
    STD dirty_data
    LDAA 4,X
    ADDA dirty_column
    STAA dirty_end
    LDAB dirty_band
    LDX #dirty_min
    ABX
    LDAA 0,X
    CMPA dirty_column
    BCC dirty_start_ready
    LDAA dirty_column
dirty_start_ready:
    STAA dirty_start
    LDAA 8,X
    INCA
    CMPA dirty_end
    BLS dirty_end_ready
    LDAA dirty_end
dirty_end_ready:
    SUBA dirty_start
    BLS dirty_advance
    STAA dirty_count
    CLRB
    TAB
    CLRA
    ADDD dirty_bytes
    STD dirty_bytes
    LDAB dirty_start
    LDX dirty_row
    ABX
    STX dirty_source
    LDX dirty_table
    LDAA 2,X
    JSR dirty_control
    LDAA dirty_start
    SUBA dirty_column
    STAA dirty_offset
    LDX dirty_table
    LDAA 3,X
    LDAB 2,X
    CMPB #$3A
    BEQ dirty_reverse
    ADDA dirty_offset
    BRA dirty_command
dirty_reverse:
    SUBA dirty_offset
dirty_command:
    JSR dirty_control
dirty_write:
    LDX dirty_source
    LDAA 0,X
    INX
    STX dirty_source
    LDX dirty_port
dirty_busy:
    TST 0,X
    BMI dirty_busy
    LDX dirty_data
    STAA 0,X
    DEC dirty_count
    BNE dirty_write
dirty_advance:
    LDAA dirty_end
    STAA dirty_column
    INC dirty_part
    LDAA dirty_part
    CMPA #4
    BNE dirty_next_table
    CLR dirty_part
    CLR dirty_column
    LDAB dirty_band
    LDX #dirty_min
    ABX
    LDAA #192
    STAA 0,X
    CLR 8,X
    INC dirty_band
    LDD dirty_row
    ADDD #192
    STD dirty_row
dirty_next_table:
    LDX dirty_table
    LDAB #5
    ABX
    STX dirty_table
    CPX #spans_end
    BNE dirty_not_finished
    CLR dirty_pending
dirty_not_finished:
    RTS
dirty_control:
    LDX dirty_port
dirty_control_busy:
    TST 0,X
    BMI dirty_control_busy
    STAA 0,X
    RTS
.section .bss, bss
dirty_pending: .space 1
dirty_saved_band: .space 1
dirty_min: .space 8
dirty_max: .space 8
dirty_bytes: .space 2
dirty_table: .space 2
dirty_row: .space 2
dirty_port: .space 2
dirty_data: .space 2
dirty_source: .space 2
dirty_band: .space 1
dirty_part: .space 1
dirty_column: .space 1
dirty_start: .space 1
dirty_end: .space 1
dirty_offset: .space 1
dirty_count: .space 1
dirty_x: .space 1
