; SPDX-License-Identifier: MIT
; Test program: actual CPU calls, with no host writes to the drawing state.
.global entry
.global frame_ready
.extern p3_error
.global probe_case
.global probe_group
.extern basic_save
.extern init
.extern clear
.extern input_init
.extern input_poll
.extern dirty_all
.extern dirty_begin
.extern dirty_next
.extern facet_reset
.extern facet_begin
.extern facet_mesh
.extern facet_x
.extern facet_y
.extern facet_double
.extern facet_wire
.extern facet_guardian_poses
.extern p3_line
.extern p3_clip_top
.extern p3_clip_bottom
.extern facet_ship_poses
.extern facet_scout_poses
.extern facet_rotor_poses
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    JSR dirty_all
    JSR input_init
    JSR facet_reset
    CLR p3_error
    LDAA #8
    STAA p3_clip_top
    LDAA #55
    STAA p3_clip_bottom
    CLR probe_case
    CLR probe_group
probe_draw:
    JSR facet_begin
    LDAA probe_group
    CMPA #12
    BCS probe_dispatch
    LDAA probe_case
    CMPA #1
    BEQ probe_overlap
    JMP probe_present
probe_overlap:
    LDAA #93
    STAA facet_x
    LDAA #33
    STAA facet_y
    LDX #facet_ship_poses
    LDX 0,X
    JSR facet_mesh
    LDAA #90
    STAA facet_x
    LDX #facet_rotor_poses
    LDX 0,X
    JSR facet_mesh
    BRA probe_present
probe_dispatch:
    CMPA #11
    BNE probe_not_line
    LDAA probe_case
    ASLA
    ASLA
    TAB
    LDX #probe_lines
    ABX
    JSR p3_line
    JMP probe_present
probe_not_line:
    CMPA #10
    BNE probe_model
    LDAB #1
    STAB facet_wire
probe_model:
    TAB
    LDX #probe_xs
    ABX
    LDAA 0,X
    STAA facet_x
    LDAA 11,X
    STAA facet_y
    LDAA probe_group
    CMPA #10
    BEQ probe_small
    CMPA #5
    BCS probe_small
    LDAA #1
    STAA facet_double
    LDX #facet_rotor_poses
    LDAA probe_group
    CMPA #8
    BCS probe_double_pose
    LDX #facet_scout_poses
probe_double_pose:
    LDAA probe_case
    BRA probe_pose
probe_small:
    LDAA probe_case
    LDX #facet_ship_poses
    CMPA #3
    BCS probe_pose
    SUBA #3
    LDX #facet_scout_poses
    CMPA #8
    BCS probe_pose
    SUBA #8
    LDX #facet_rotor_poses
    CMPA #4
    BCS probe_pose
    SUBA #4
    LDX #facet_guardian_poses
probe_pose:
    ASLA
    TAB
    ABX
    LDX 0,X
    JSR facet_mesh
probe_present:
    JSR dirty_begin
probe_transfer:
    JSR input_poll
    JSR dirty_next
    BNE probe_transfer
frame_ready:
    INC probe_case
    LDAB probe_group
    LDX #probe_limits
    ABX
    LDAB 0,X
    STAB probe_limit
    LDAA probe_case
    CMPA probe_limit
    BCC probe_next_group
    JMP probe_draw
probe_next_group:
    CLR probe_case
    INC probe_group
    JMP probe_draw
probe_xs: .byte 16,93,187,16,110,214,152,64,152,152,93
    .byte 24,33,40,18,44,32,32,32,16,48,33
probe_limits: .byte 19,19,19,19,19,4,4,4,8,8,19,16,3
probe_lines:
    .byte 0,8,191,8,191,55,0,55,40,0,40,63,100,63,100,0
    .byte 0,0,191,63,191,63,0,0,0,63,191,0,191,0,0,63
    .byte 18,20,24,24,24,24,18,20,70,15,73,46,73,46,70,15
    .byte 5,2,189,2,5,60,189,60,92,32,92,32,20,8,22,9
.section .bss, bss
probe_case: .space 1
probe_group: .space 1
probe_limit: .space 1
