; SPDX-License-Identifier: MIT
; software Port 1 bit 4 tone. No ROM routine or timer IRQ dependency.
.global sound_play
.global sound_tone
.global sound_rest
.global sound_port
.global sound_period
.global sound_count
.section .text, code
; Caller owns DDR and supplies the full Port 1 output shadow in sound_port.
; X = delay iterations (1..65535), D = full wave periods (0 = no-op).
; Blocking; mask interrupts externally for predictable pitch. A/B/X/CC clobbered.
; Delay loop = 4*N cycles. See README for measured complete half-periods.
sound_tone:
    STD sound_count
    CPX #0
    BEQ sound_return
    STX sound_period
    LDD sound_count
    BEQ sound_return
    LDAA sound_port
    ANDA #$EF
    STAA sound_port
sound_cycle:
    LDAA sound_port
    ORAA #$10
    STAA $0002
    LDX sound_period
sound_high:
    DEX
    BNE sound_high
    LDAA sound_port
    STAA $0002
    LDX sound_period
sound_low:
    DEX
    BNE sound_low
    LDD sound_count
    SUBD #1
    STD sound_count
    BNE sound_cycle
sound_return:
    RTS
; X = delay iterations; zero is a no-op. Does not write ports.
sound_rest:
    CPX #0
    BEQ sound_return
sound_rest_loop:
    DEX
    BNE sound_rest_loop
    RTS
; X -> records: delay word, period-count word; count zero ends.
; A zero delay selects a rest of count*4 cycles plus call overhead.
sound_play:
    STX sound_sequence
sound_play_next:
    LDX sound_sequence
    LDD 0,X
    STD sound_period
    LDD 2,X
    BEQ sound_return
    STD sound_count
    LDAB #4
    ABX
    STX sound_sequence
    LDX sound_period
    CPX #0
    BEQ sound_play_rest
    LDD sound_count
    JSR sound_tone
    BRA sound_play_next
sound_play_rest:
    LDX sound_count
    JSR sound_rest
    BRA sound_play_next
.section .bss, bss
sound_port: .space 1
sound_period: .space 2
sound_count: .space 2
sound_sequence: .space 2
