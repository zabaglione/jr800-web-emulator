; SPDX-License-Identifier: MIT
.global entry
.global frame_ready
.global angle
.global paused
.global frame_counter
.global poll_controls
.extern basic_save
.extern basic_check_break
.extern init
.extern clear
.extern dirty_all
.extern dirty_begin
.extern dirty_next
.extern text
.extern framebuffer
.extern render_fighter
.extern view_valid
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    CLR view_valid
    JSR dirty_all
    CLR paused
    CLR key_previous
    CLR frame_counter
    LDAA #7
    STAA angle
    LDX #title
    LDD #framebuffer + 51
    JSR text
    LDX #caption
    LDD #framebuffer + 1344 + 30
    JSR text
render:
    JSR render_fighter
    JSR dirty_begin
transfer:
    JSR poll_controls
    JSR dirty_next
    BNE transfer
    INC frame_counter
frame_ready:
    JSR poll_controls
    TST paused
    BEQ advance_angle
    ; Only the paused loop waits; active rendering already polls input often.
    LDX #3000
wait:
    DEX
    BNE wait
    BRA frame_ready
advance_angle:
    LDAA angle
    INCA
    ANDA #63
    STAA angle
    BRA render

; Poll between triangles and LCD spans so a short press is not lost during
; rendering. An edge toggles the flag once; the current frame finishes first.
poll_controls:
    JSR basic_check_break
    LDAA $0FFE
    COMA
    ANDA #$20
    STAA key_now
    BEQ key_done
    TST key_previous
    BNE key_done
    LDAA paused
    EORA #1
    STAA paused
key_done:
    LDAA key_now
    STAA key_previous
    RTS
.section .bss, bss
angle: .space 1
paused: .space 1
frame_counter: .space 1
key_previous: .space 1
key_now: .space 1
.section .data, data
title: .byte 80,79,76,89,71,79,78,32,70,73,71,72,84,69,82,0
caption: .byte 53,32,80,65,85,83,69,47,82,85,78,32,66,82,69,65,75,32,69,88,73,84,0
