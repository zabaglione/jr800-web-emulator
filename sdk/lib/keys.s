; SPDX-License-Identifier: MIT
.global keys_reset
.global keys_update
.global keys_keypad
.global keys_latch
.global keys_ack
.section .text, code
; X -> three caller-owned bytes: held, pressed, released. A/B/X clobbered.
keys_reset:
    CLR 0,X
    CLR 1,X
    CLR 2,X
    RTS
; A = active-high mask, X = state. No debounce/repeat or hidden allocation.
keys_update:
    LDAB 0,X
    STAB 2,X
    STAA 0,X
    LDAA 2,X
    COMA
    ANDA 0,X
    STAA 1,X
    LDAA 0,X
    COMA
    ANDA 2,X
    STAA 2,X
    RTS
; Accumulate both edge masks until keys_ack. X still selects the state.
keys_latch:
    LDAB 1,X
    PSHB
    LDAB 2,X
    PSHB
    JSR keys_update
    PULA
    ORAA 2,X
    STAA 2,X
    PULA
    ORAA 1,X
    STAA 1,X
    RTS
keys_ack:
    CLR 1,X
    CLR 2,X
    RTS
; row: active-low keypad 0..7. Matrix limitations still apply.
keys_keypad:
    LDAA $0FFE
    COMA
    JMP keys_latch
