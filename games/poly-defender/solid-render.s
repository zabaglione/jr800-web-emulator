; SPDX-License-Identifier: MIT
; Game-owned mesh selection and line primitives. No runtime matrix or division.
.global p3_init
.global p3_begin
.global p3_draw
.global p3_line
.global p3_dashed_line
.global p3_x
.global p3_y
.global p3_yaw
.global p3_aim
.global p3_scale
.global p3_mode
.global p3_error
.global p3_clip_top
.global p3_clip_bottom
.global p3_model_tetra
.global p3_model_octa
.global p3_model_prism
.extern facet_mesh
.extern facet_register
.extern facet_reset
.extern facet_x
.extern facet_y
.extern facet_double
.extern facet_wire
.extern facet_left
.extern facet_right
.extern facet_top
.extern facet_bottom
.extern facet_scout_poses
.extern facet_rotor_poses
.extern facet_guardian_poses
.extern facet_aim_poses
.extern framebuffer
.extern input_poll
.section .text, code
p3_init:
    CLR p3_error
    JMP facet_reset
p3_begin:
    ; app_draw clears occupied polygon and line ranges together.
    RTS
p3_draw:
    LDAA p3_x
    STAA facet_x
    LDAA p3_y
    STAA facet_y
    LDAA p3_mode
    STAA facet_wire
    CLR facet_double
    LDAA 0,X
    CMPA #2
    BEQ solid_guardian
    STAA solid_kind
    LDAB p3_scale
    CMPB #48
    BCS solid_small
    INC facet_double
solid_small:
    LDAA p3_yaw
    LSRA
    LDAB solid_kind
    BNE solid_octa
    TST p3_aim
    BNE solid_aim
    ANDA #7
    LDX #facet_scout_poses
    BRA solid_pose
solid_aim:
    CLRA
    LDX #facet_aim_poses
    BRA solid_pose
solid_octa:
    ANDA #3
    LDX #facet_rotor_poses
    BRA solid_pose
solid_guardian:
    LDAA p3_yaw
    LSRA
    ANDA #3
    LDX #facet_guardian_poses
solid_pose:
    ASLA
    TAB
    ABX
    LDX 0,X
    JSR facet_mesh
    CLR facet_double
    CLR facet_wire
    RTS

; The beam's preview uses 4-pixel dashes along its actual firing line.
p3_dashed_line:
    INC line_dashed
    JSR p3_line
    CLR line_dashed
    RTS
; Inclusive integer line, endpoints x=0..191, y=0..63. Viewport clips y.
p3_line:
    LDAA 0,X
    STAA line_x
    STAA facet_left
    STAA facet_right
    LDAA 1,X
    STAA line_y
    STAA facet_top
    STAA facet_bottom
    LDAA 2,X
    CMPA #192
    BCC line_bad
    CMPA facet_left
    BCC line_right
    STAA facet_left
    BRA line_y_bounds
line_right:
    STAA facet_right
    BRA line_y_bounds
line_bad:
    LDAA #1
    STAA p3_error
    RTS
line_y_bounds:
    LDAA 3,X
    CMPA #64
    BCC line_bad
    CMPA facet_top
    BCC line_bottom
    STAA facet_top
    BRA line_clip
line_bottom:
    STAA facet_bottom
line_clip:
    LDAA line_x
    CMPA #192
    BCC line_bad
    LDAA line_y
    CMPA #64
    BCC line_bad
    LDAA facet_top
    CMPA p3_clip_bottom
    BHI line_done
    CMPA p3_clip_top
    BCC line_top_ready
    LDAA p3_clip_top
    STAA facet_top
line_top_ready:
    LDAA facet_bottom
    CMPA p3_clip_top
    BCS line_done
    CMPA p3_clip_bottom
    BLS line_bottom_ready
    LDAA p3_clip_bottom
    STAA facet_bottom
line_bottom_ready:
    LDAA #1
    STAA line_sx
    STAA line_sy
    LDAA 2,X
    SUBA line_x
    BCC line_dx
    NEGA
    NEG line_sx
line_dx:
    STAA line_dx_value
    LDAA 3,X
    SUBA line_y
    BCC line_dy
    NEGA
    NEG line_sy
line_dy:
    STAA line_dy_value
    CMPA line_dx_value
    BCC line_steps
    LDAA line_dx_value
line_steps:
    STAA line_steps_value
    STAA line_count
    CLR line_ex
    CLR line_ey
    JSR facet_register
    TST line_dy_value
    BEQ line_horizontal
    JMP line_loop
line_done:
    RTS
line_horizontal:
    JSR line_address
    LDAB facet_left
    LDX line_pointer
    ABX
    LDAB facet_right
    SUBB facet_left
    INCB
    TST line_dashed
    BNE line_dash_loop
line_horizontal_loop:
    LDAA 0,X
    ORAA line_bit
    STAA 0,X
    INX
    DECB
    BNE line_horizontal_loop
    RTS
line_dash_loop:
    LDAA line_x
    BITA #4
    BNE line_dash_skip
    LDAA 0,X
    ORAA line_bit
    STAA 0,X
line_dash_skip:
    INX
    INC line_x
    DECB
    BNE line_dash_loop
    RTS
line_address:
    LDAA line_y
    ANDA #7
    TAB
    LDX #line_bits
    ABX
    LDAA 0,X
    STAA line_bit
    LDAA line_y
    LSRA
    LSRA
    LSRA
    LDAB #192
    MUL
    ADDD #framebuffer
    STD line_pointer
    RTS
line_loop:
    LDAA line_count
    ANDA #7
    BNE line_polled
    JSR input_poll
line_polled:
    LDAA line_y
    CMPA p3_clip_top
    BCS line_advance
    CMPA p3_clip_bottom
    BHI line_advance
    JSR line_address
    LDX line_pointer
    LDAB line_x
    ABX
    LDAA 0,X
    ORAA line_bit
    STAA 0,X
line_advance:
    TST line_count
    BNE line_continue
    RTS
line_continue:
    DEC line_count
    LDAA line_ex
    ADDA line_dx_value
    BCS line_step_x
    CMPA line_steps_value
    BCS line_keep_x
line_step_x:
    SUBA line_steps_value
    LDAB line_x
    ADDB line_sx
    STAB line_x
line_keep_x:
    STAA line_ex
    LDAA line_ey
    ADDA line_dy_value
    BCS line_step_y
    CMPA line_steps_value
    BCS line_keep_y
line_step_y:
    SUBA line_steps_value
    LDAB line_y
    ADDB line_sy
    STAB line_y
line_keep_y:
    STAA line_ey
    BRA line_loop
.section .data, data
p3_model_tetra: .byte 0
p3_model_octa: .byte 1
p3_model_prism: .byte 2
line_bits: .byte 1,2,4,8,16,32,64,128
.section .bss, bss
p3_x: .space 1
p3_y: .space 1
p3_yaw: .space 1
p3_aim: .space 1
p3_scale: .space 1
p3_mode: .space 1
p3_error: .space 1
p3_clip_top: .space 1
p3_clip_bottom: .space 1
solid_kind: .space 1
line_x: .space 1
line_y: .space 1
line_dx_value: .space 1
line_dy_value: .space 1
line_steps_value: .space 1
line_count: .space 1
line_sx: .space 1
line_sy: .space 1
line_ex: .space 1
line_ey: .space 1
line_bit: .space 1
line_pointer: .space 2
line_dashed: .space 1
