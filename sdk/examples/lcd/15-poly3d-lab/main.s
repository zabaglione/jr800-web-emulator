; SPDX-License-Identifier: MIT
.global app_init
.global app_update
.global app_draw
.global lab_model
.global lab_yaw
.global lab_pitch
.global lab_wire
.global lab_auto
.extern text
.extern framebuffer
.extern ui_footer
.extern ui_number
.extern hud_dirty
.extern input_event
.extern p3_draw
.extern p3_x
.extern p3_y
.extern p3_yaw
.extern p3_pitch
.extern p3_scale
.extern p3_mode
.extern p3_model_tetra
.extern p3_model_octa
.extern p3_model_cube
.section .text, code
app_init:
    CLR lab_model
    CLR lab_yaw
    CLR lab_wire
    LDAA #4
    STAA lab_pitch
    LDAA #1
    STAA lab_auto
    LDX #lab_title
    LDD #framebuffer + 6
    JSR text
    LDX #lab_header
    LDD #framebuffer + 90
    JSR text
    LDX #lab_help
    JMP ui_footer
app_update:
    LDAA input_event
    BITA #32
    BEQ lab_toggle
    INC lab_model
    LDAA lab_model
    CMPA #3
    BCS lab_model_ready
    CLR lab_model
lab_model_ready:
    LDAA #1
    STAA hud_dirty
    STAA lab_auto
lab_toggle:
    LDAA input_event
    BITA #16
    BEQ lab_controls
    LDAA lab_wire
    EORA #1
    STAA lab_wire
lab_controls:
    LDAA input_event
    ANDA #15
    BEQ lab_spin
    CLR lab_auto
    BITA #4
    BEQ lab_right
    LDAB lab_yaw
    SUBB #4
    ANDB #63
    STAB lab_yaw
lab_right:
    BITA #8
    BEQ lab_up
    LDAB lab_yaw
    ADDB #4
    ANDB #63
    STAB lab_yaw
lab_up:
    BITA #1
    BEQ lab_down
    LDAB lab_pitch
    ADDB #4
    ANDB #63
    STAB lab_pitch
lab_down:
    BITA #2
    BEQ lab_update_done
    LDAB lab_pitch
    SUBB #4
    ANDB #63
    STAB lab_pitch
    RTS
lab_spin:
    TST lab_auto
    BEQ lab_update_done
    INC lab_yaw
    LDAA lab_yaw
    ANDA #63
    STAA lab_yaw
lab_update_done:
    RTS
app_draw:
    TST hud_dirty
    BEQ lab_shape
    CLR hud_dirty
    LDAA lab_model
    INCA
    LDX #framebuffer + 132
    JSR ui_number
lab_shape:
    LDAA #96
    STAA p3_x
    STAA p3_scale
    LDAA #34
    STAA p3_y
    LDAA lab_yaw
    STAA p3_yaw
    LDAA lab_pitch
    STAA p3_pitch
    LDAA lab_wire
    STAA p3_mode
    LDX #lab_models
    LDAB lab_model
    ASLB
    ABX
    LDX 0,X
    JMP p3_draw
.section .bss, bss
lab_model: .space 1
lab_yaw: .space 1
lab_pitch: .space 1
lab_wire: .space 1
lab_auto: .space 1
.section .data, data
lab_models: .word p3_model_tetra,p3_model_octa,p3_model_cube
; POLY3D LAB
lab_title: .byte 80,79,76,89,51,68,32,76,65,66,0
; MODEL  01
lab_header: .byte 77,79,68,69,76,32,32,48,49,0
; 4/6 8/2 ROT SPACE WIRE RET NEXT
lab_help: .byte 52,47,54,32,56,47,50,32,82,79,84,32,83,80,65,67,69,32,87,73,82,69,32,82,69,84,32,78,69,88,84,0
