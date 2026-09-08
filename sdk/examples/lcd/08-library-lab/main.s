; SPDX-License-Identifier: MIT
.global entry
.global frame_ready
.global key_state
.global position
.extern basic_save
.extern basic_check_break
.extern init
.extern clear
.extern present_begin
.extern present_next
.extern text
.extern framebuffer
.extern keys_ack
.extern keys_reset
.extern keys_keypad
.extern sprite8
.extern sprite_width
.extern sprite_mode
.extern scroll_left
.extern scroll_right
.extern sound_play
.extern sound_port
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    LDX #key_state
    JSR keys_reset
    LDAA #8
    STAA sprite_width
    CLR sprite_mode
    LDAA #92
    STAA position
    LDX #title
    LDD #framebuffer + 192 + 42
    JSR text
    LDX #caption
    LDD #framebuffer + 1152 + 12
    JSR text
    LDX #framebuffer + 768
    LDAB #192
    LDAA #$88
terrain:
    STAA 0,X
    EORA #$44
    INX
    DECB
    BNE terrain
render:
    LDX #key_state
    JSR keys_ack
    LDX #framebuffer + 576
    LDAB #192
    CLRA
erase:
    STAA 0,X
    INX
    DECB
    BNE erase
    LDX #shape
    LDAA position
    LDAB #24
    JSR sprite8
    JSR present_begin
transfer:
    LDX #key_state
    JSR keys_keypad
    JSR present_next
    BNE transfer
frame_ready:
    JSR basic_check_break
    LDAA #20
    STAA poll_count
poll_wait:
    LDX #1000
poll_delay:
    DEX
    BNE poll_delay
    LDX #key_state
    JSR keys_keypad
    DEC poll_count
    BNE poll_wait
    LDAA key_state
    BITA #$10
    BNE left
    BITA #$40
    BNE right
    BRA action
left:
    TST position
    BEQ action
    DEC position
    LDX #framebuffer + 768
    LDAA #1
    JSR scroll_left
    BRA action
right:
    LDAA position
    CMPA #184
    BEQ action
    INC position
    LDX #framebuffer + 768
    LDAA #1
    JSR scroll_right
action:
    LDAA key_state + 1
    ANDA #$A0
    BEQ render
    ; Explicit demo baseline. Applications must own and shadow all Port 1 bits.
    LDAA #$EF
    STAA sound_port
    LDAA #$10
    STAA $0000
    LDX #effect
    LDAA key_state + 1
    BITA #$80
    BEQ play
    LDX #melody
play:
    JSR sound_play
    JMP render
.section .bss, bss
key_state: .space 3
position: .space 1
poll_count: .space 1
.section .data, data
title: .byte 76,73,66,82,65,82,89,32,76,65,66, 0
caption: .byte 52,47,54,32,77,79,86,69,32,53,32,83,69,32,55,32,77,69,76,79,68,89,0
shape: .byte $3C,$7E,$DB,$FF,$FF,$DB,$7E,$3C
effect:
    .word 340,32,170,32,0,0
melody:
    .word 340,110,302,124,269,139,340,110,0,0
