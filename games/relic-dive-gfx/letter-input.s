; SPDX-License-Identifier: MIT
; Reached only when no keypad direction is held. Same logical key codes
; feed the original edge detector, so a keypad/WASD alias cannot double-act.
    LDAA $0FBF
    BITA #$80
    BNE gfx_key_s
    LDAB #1
    JMP poll_sample
gfx_key_s:
    BITA #8
    BNE gfx_key_a
    LDAB #2
    JMP poll_sample
gfx_key_a:
    LDAA $0FEF
    BITA #2
    BNE gfx_key_d
    LDAB #3
    JMP poll_sample
gfx_key_d:
    BITA #16
    BNE gfx_key_confirm
    LDAB #4
    JMP poll_sample
gfx_key_confirm:
