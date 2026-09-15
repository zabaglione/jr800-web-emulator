; SPDX-License-Identifier: MIT
; Short blocking Port 1 tones, serviced during the frame wait.
; This is a monophonic cue player, not an IRQ or a timer-to-speaker assumption.
.global def_audio_init
.global def_audio_request
.global def_audio_pump
.global audio_id
.global audio_serial
.extern sound_tone
.extern sound_port
.extern input_ticks
.section .text, code
def_audio_init:
    CLR audio_id
    CLR audio_priority
    CLR audio_serial
    LDAA #$EF
    STAA sound_port
    STAA $0002
    LDAA #$10
    STAA $0000
    RTS
; A=1 shot,2 armor hit,3 blast,4 split,5 item,6 shield,7 hurt,
;   8 death,9 warning,10 start,11 clear,12 game over,13 enemy shot.
; The priority table keeps an enemy shot from masking an explosion or jingle.
; Preserve caller registers so a cue cannot corrupt an entity pointer.
def_audio_request:
    PSHA
    PSHB
    PSHX
    STAA audio_next
    DECA
    TAB
    LDX #audio_priorities
    ABX
    LDAA 0,X
    CMPA audio_priority
    BCS audio_request_done
    STAA audio_priority
    LDAA audio_next
    STAA audio_id
    INC audio_serial
    DECA
    ASLA
    TAB
    LDX #audio_cues
    ABX
    LDX 0,X
    STX audio_pointer
    JSR audio_note
    ; The first short fragment is audible even in a busy rendering frame.
    JSR audio_fragment
audio_request_done:
    PULX
    PULB
    PULA
    RTS
def_audio_pump:
    TST audio_id
    BEQ audio_done
    LDAA input_ticks
    SUBA audio_start
    CMPA audio_length
    BCS audio_fragment
    LDX audio_pointer
    LDAB #3
    ABX
    STX audio_pointer
    JSR audio_note
audio_fragment:
    TST audio_id
    BEQ audio_done
    LDX audio_period
    CPX #0
    BEQ audio_done
    ; Under 8,500 E cycles for the longest period (512).
    LDD #2
    JMP sound_tone
audio_done:
    RTS
audio_note:
    LDD 0,X
    STD audio_period
    LDAA 2,X
    STAA audio_length
    BEQ audio_end
    LDAA input_ticks
    STAA audio_start
    RTS
audio_end:
    CLR audio_id
    CLR audio_priority
    RTS
.section .bss, bss
audio_id: .space 1
audio_priority: .space 1
audio_next: .space 1
audio_serial: .space 1
audio_pointer: .space 2
audio_period: .space 2
audio_start: .space 1
audio_length: .space 1
.section .data, data
; Each note is delay word + duration in 20,000-E-cycle input ticks.
audio_cues: .word audio_shot,audio_hit,audio_blast,audio_split,audio_item,audio_shield,audio_hurt,audio_death,audio_warning,audio_start_cue,audio_clear,audio_over,audio_enemy
audio_priorities: .byte 1,2,3,4,5,6,7,8,9,10,11,12,2
audio_shot: .word 56
    .byte 2
    .word 100
    .byte 2
    .word 0
    .byte 0
audio_hit: .word 180
    .byte 3
    .word 0
    .byte 0
audio_blast: .word 120
    .byte 3
    .word 280
    .byte 4
    .word 480
    .byte 5
    .word 0
    .byte 0
audio_split: .word 240
    .byte 4
    .word 120
    .byte 4
    .word 400
    .byte 6
    .word 0
    .byte 0
audio_item: .word 172
    .byte 5
    .word 136
    .byte 5
    .word 114
    .byte 7
    .word 0
    .byte 0
audio_shield: .word 90
    .byte 5
    .word 140
    .byte 5
    .word 0
    .byte 0
audio_hurt: .word 100
    .byte 6
    .word 320
    .byte 8
    .word 480
    .byte 10
    .word 0
    .byte 0
audio_death: .word 80
    .byte 6
    .word 140
    .byte 6
    .word 220
    .byte 8
    .word 360
    .byte 10
    .word 512
    .byte 20
    .word 0
    .byte 0
audio_warning: .word 306
    .byte 12
    .word 153
    .byte 12
    .word 0
    .byte 10
    .word 306
    .byte 12
    .word 153
    .byte 12
    .word 0
    .byte 10
    .word 306
    .byte 12
    .word 153
    .byte 20
    .word 0
    .byte 0
audio_start_cue: .word 344
    .byte 8
    .word 273
    .byte 8
    .word 229
    .byte 8
    .word 172
    .byte 16
    .word 0
    .byte 0
audio_clear: .word 229
    .byte 9
    .word 182
    .byte 9
    .word 153
    .byte 9
    .word 114
    .byte 15
    .word 153
    .byte 8
    .word 114
    .byte 25
    .word 0
    .byte 0
audio_over: .word 273
    .byte 15
    .word 344
    .byte 15
    .word 459
    .byte 30
    .word 0
    .byte 0
audio_enemy: .word 86
    .byte 2
    .word 172
    .byte 3
    .word 0
    .byte 0
