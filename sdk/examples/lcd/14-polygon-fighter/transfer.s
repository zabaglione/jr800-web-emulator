; SPDX-License-Identifier: MIT
; Sample-local dirty transfer. Coordinates/direction use display.s spans.
; Fixed I/O operands keep X on the framebuffer during each physical span.
; Every command and data write still polls BUSY, as in the shared driver.
; Shares display.s scratch $80-$91; input polling does not use these bytes.
.equ dirty_table, $80
.equ dirty_row, $82
.equ dirty_port, $84
.equ dirty_source, $86
.equ dirty_band, $88
.equ dirty_part, $89
.equ dirty_column, $8A
.equ dirty_start, $8B
.equ dirty_end, $8C
.equ dirty_offset, $8D
.equ dirty_count, $8E
.equ dirty_saved_band, $8F
.equ dirty_x, $90
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
    LDAB dirty_band
    ANDB #4
    ADDB dirty_part
    ASLB
    LDX #dirty_writers
    ABX
    LDX 0,X
    JSR 0,X
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

; Four bytes per loop; BUSY is checked before each individual write.
write_0:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_0_tail
write_0_block:
    LDAA 0,X
write_0_busy_0:
    TST $0A01
    BMI write_0_busy_0
    STAA $0B01
    LDAA 1,X
write_0_busy_1:
    TST $0A01
    BMI write_0_busy_1
    STAA $0B01
    LDAA 2,X
write_0_busy_2:
    TST $0A01
    BMI write_0_busy_2
    STAA $0B01
    LDAA 3,X
write_0_busy_3:
    TST $0A01
    BMI write_0_busy_3
    STAA $0B01
    INX
    INX
    INX
    INX
    DECB
    BNE write_0_block
write_0_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_0_done
write_0_byte:
    LDAA 0,X
write_0_busy:
    TST $0A01
    BMI write_0_busy
    STAA $0B01
    INX
    DECB
    BNE write_0_byte
write_0_done:
    RTS

write_1:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_1_tail
write_1_block:
    LDAA 0,X
write_1_busy_0:
    TST $0A02
    BMI write_1_busy_0
    STAA $0B02
    LDAA 1,X
write_1_busy_1:
    TST $0A02
    BMI write_1_busy_1
    STAA $0B02
    LDAA 2,X
write_1_busy_2:
    TST $0A02
    BMI write_1_busy_2
    STAA $0B02
    LDAA 3,X
write_1_busy_3:
    TST $0A02
    BMI write_1_busy_3
    STAA $0B02
    INX
    INX
    INX
    INX
    DECB
    BNE write_1_block
write_1_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_1_done
write_1_byte:
    LDAA 0,X
write_1_busy:
    TST $0A02
    BMI write_1_busy
    STAA $0B02
    INX
    DECB
    BNE write_1_byte
write_1_done:
    RTS

write_2:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_2_tail
write_2_block:
    LDAA 0,X
write_2_busy_0:
    TST $0A04
    BMI write_2_busy_0
    STAA $0B04
    LDAA 1,X
write_2_busy_1:
    TST $0A04
    BMI write_2_busy_1
    STAA $0B04
    LDAA 2,X
write_2_busy_2:
    TST $0A04
    BMI write_2_busy_2
    STAA $0B04
    LDAA 3,X
write_2_busy_3:
    TST $0A04
    BMI write_2_busy_3
    STAA $0B04
    INX
    INX
    INX
    INX
    DECB
    BNE write_2_block
write_2_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_2_done
write_2_byte:
    LDAA 0,X
write_2_busy:
    TST $0A04
    BMI write_2_busy
    STAA $0B04
    INX
    DECB
    BNE write_2_byte
write_2_done:
    RTS

write_3:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_3_tail
write_3_block:
    LDAA 0,X
write_3_busy_0:
    TST $0A08
    BMI write_3_busy_0
    STAA $0B08
    LDAA 1,X
write_3_busy_1:
    TST $0A08
    BMI write_3_busy_1
    STAA $0B08
    LDAA 2,X
write_3_busy_2:
    TST $0A08
    BMI write_3_busy_2
    STAA $0B08
    LDAA 3,X
write_3_busy_3:
    TST $0A08
    BMI write_3_busy_3
    STAA $0B08
    INX
    INX
    INX
    INX
    DECB
    BNE write_3_block
write_3_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_3_done
write_3_byte:
    LDAA 0,X
write_3_busy:
    TST $0A08
    BMI write_3_busy
    STAA $0B08
    INX
    DECB
    BNE write_3_byte
write_3_done:
    RTS

write_4:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_4_tail
write_4_block:
    LDAA 0,X
write_4_busy_0:
    TST $0A10
    BMI write_4_busy_0
    STAA $0B10
    LDAA 1,X
write_4_busy_1:
    TST $0A10
    BMI write_4_busy_1
    STAA $0B10
    LDAA 2,X
write_4_busy_2:
    TST $0A10
    BMI write_4_busy_2
    STAA $0B10
    LDAA 3,X
write_4_busy_3:
    TST $0A10
    BMI write_4_busy_3
    STAA $0B10
    INX
    INX
    INX
    INX
    DECB
    BNE write_4_block
write_4_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_4_done
write_4_byte:
    LDAA 0,X
write_4_busy:
    TST $0A10
    BMI write_4_busy
    STAA $0B10
    INX
    DECB
    BNE write_4_byte
write_4_done:
    RTS

write_5:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_5_tail
write_5_block:
    LDAA 0,X
write_5_busy_0:
    TST $0A20
    BMI write_5_busy_0
    STAA $0B20
    LDAA 1,X
write_5_busy_1:
    TST $0A20
    BMI write_5_busy_1
    STAA $0B20
    LDAA 2,X
write_5_busy_2:
    TST $0A20
    BMI write_5_busy_2
    STAA $0B20
    LDAA 3,X
write_5_busy_3:
    TST $0A20
    BMI write_5_busy_3
    STAA $0B20
    INX
    INX
    INX
    INX
    DECB
    BNE write_5_block
write_5_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_5_done
write_5_byte:
    LDAA 0,X
write_5_busy:
    TST $0A20
    BMI write_5_busy
    STAA $0B20
    INX
    DECB
    BNE write_5_byte
write_5_done:
    RTS

write_6:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_6_tail
write_6_block:
    LDAA 0,X
write_6_busy_0:
    TST $0A40
    BMI write_6_busy_0
    STAA $0B40
    LDAA 1,X
write_6_busy_1:
    TST $0A40
    BMI write_6_busy_1
    STAA $0B40
    LDAA 2,X
write_6_busy_2:
    TST $0A40
    BMI write_6_busy_2
    STAA $0B40
    LDAA 3,X
write_6_busy_3:
    TST $0A40
    BMI write_6_busy_3
    STAA $0B40
    INX
    INX
    INX
    INX
    DECB
    BNE write_6_block
write_6_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_6_done
write_6_byte:
    LDAA 0,X
write_6_busy:
    TST $0A40
    BMI write_6_busy
    STAA $0B40
    INX
    DECB
    BNE write_6_byte
write_6_done:
    RTS

write_7:
    LDX dirty_source
    LDAB dirty_count
    LSRB
    LSRB
    BEQ write_7_tail
write_7_block:
    LDAA 0,X
write_7_busy_0:
    TST $0A80
    BMI write_7_busy_0
    STAA $0B80
    LDAA 1,X
write_7_busy_1:
    TST $0A80
    BMI write_7_busy_1
    STAA $0B80
    LDAA 2,X
write_7_busy_2:
    TST $0A80
    BMI write_7_busy_2
    STAA $0B80
    LDAA 3,X
write_7_busy_3:
    TST $0A80
    BMI write_7_busy_3
    STAA $0B80
    INX
    INX
    INX
    INX
    DECB
    BNE write_7_block
write_7_tail:
    LDAB dirty_count
    ANDB #3
    BEQ write_7_done
write_7_byte:
    LDAA 0,X
write_7_busy:
    TST $0A80
    BMI write_7_busy
    STAA $0B80
    INX
    DECB
    BNE write_7_byte
write_7_done:
    RTS


.section .data, data
dirty_writers:
    .word write_0, write_1, write_2, write_3, write_4, write_5, write_6, write_7
.section .bss, bss
dirty_min: .space 8
dirty_max: .space 8
dirty_pending: .space 1
dirty_bytes: .space 2
