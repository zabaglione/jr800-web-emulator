; SPDX-License-Identifier: MIT
; Fixed pools and authored encounter patterns, executed by the HD6301.
; Gate: x (0=free), gap center, direction. Scout: x,y,dy,speed.
.global campaign_update
.global campaign_draw
.global campaign_reset
.global campaign_rush
.global campaign_busy
.global gates
.global gate_left
.global swarm
.extern phase
.extern sector
.extern stage
.extern ship_x
.extern ship_y
.extern shots
.extern enemies
.extern def_tick
.extern spawn_left
.extern def_damage
.extern hit_cause
.extern def_add_score
.extern def_drop
.extern def_drop_kind
.extern def_drop_x
.extern def_drop_y
.extern def_audio_request
.extern def_line
.extern facet_register
.extern facet_left
.extern facet_right
.extern facet_top
.extern facet_bottom
.extern framebuffer
.extern facet_mesh
.extern facet_rotor_poses
.extern facet_scout_poses
.extern facet_x
.extern facet_y
.extern input_poll
.extern p3_line
.extern p3_dashed_line
.section .campaign, code
campaign_reset:
    LDX #campaign_state
campaign_zero:
    CLR 0,X
    INX
    CPX #campaign_end
    BNE campaign_zero
    LDAA stage
    CMPA #1
    BNE campaign_done
    LDAA sector
    ADDA #3
    STAA gate_left
campaign_done:
    RTS
campaign_update:
    LDAA stage
    CMPA #1
    BEQ campaign_gates
    CMPA #2
    BEQ campaign_scouts
    RTS
campaign_gates:
    LDX #gates
    JSR gate_move
    LDAA phase
    CMPA #1
    BNE campaign_done
    LDX #gates + 3
    JSR gate_move
    TST gate_delay
    BEQ gate_spawn
    DEC gate_delay
    RTS
gate_spawn:
    TST gate_left
    BEQ campaign_done
    LDX #gates
    TST 0,X
    BEQ gate_slot
    LDX #gates + 3
    TST 0,X
    BNE campaign_done
gate_slot:
    LDAA #186
    STAA 0,X
    LDAA gate_left
    BITA #1
    BEQ gate_high
    LDAA #40
    BRA gate_height
gate_high:
    LDAA #24
gate_height:
    STAA 1,X
    LDAA #1
    STAA 2,X
    DEC gate_left
    LDAA #42
    STAA gate_delay
    RTS
campaign_scouts:
    LDX #swarm
scout_loop:
    STX entity_ptr
    JSR scout_move
    LDAA phase
    CMPA #1
    BNE campaign_done
    LDX entity_ptr
    LDAB #4
    ABX
    CPX #swarm + 24
    BNE scout_loop
    RTS

gate_move:
    TST 0,X
    BEQ gate_done
    LDAA 0,X
    SUBA #2
    STAA 0,X
    CMPA #8
    BHI gate_motion
    CLR 0,X
    RTS
gate_motion:
    LDAA sector
    CMPA #2
    BNE gate_collision
    LDAA def_tick
    ANDA #7
    BNE gate_collision
    LDAA 1,X
    ADDA 2,X
    STAA 1,X
    CMPA #24
    BLS gate_turn
    CMPA #40
    BCS gate_collision
gate_turn:
    NEG 2,X
gate_collision:
    STX entity_ptr
    LDAA ship_x
    SUBA 0,X
    JSR magnitude
    CMPA #13
    BHI gate_shots
    SUBA #3
    BCC gate_player_margin
    CLRA
gate_player_margin:
    LSRA
    ADDA #9
    STAA gate_margin
    LDAA ship_y
    SUBA 1,X
    JSR magnitude
    LDAB sector
    CMPB #2
    BEQ rotor_player
    CMPA gate_margin
    BCS gate_shots
    BRA gate_player_hit
rotor_player:
    CMPA #11
    BHI gate_shots
gate_player_hit:
    LDAA #3
    STAA hit_cause
    JSR def_damage
gate_shots:
    LDX #shots
    JSR gate_stop_shot
    LDX #shots + 4
    JSR gate_stop_shot
gate_done:
    RTS
gate_stop_shot:
    TST 0,X
    BEQ gate_done
    STX shot_ptr
    LDAA 1,X
    LDAB 2,X
    LDX entity_ptr
    SUBA 0,X
    JSR magnitude
    CMPA #13
    BHI gate_done
    SUBA #4
    BCC gate_shot_margin
    CLRA
gate_shot_margin:
    LSRA
    ADDA #12
    STAA gate_margin
    TBA
    SUBA 1,X
    JSR magnitude
    LDAB sector
    CMPB #2
    BEQ rotor_shot
    CMPA gate_margin
    BCS gate_done
    BRA gate_shot_hit
rotor_shot:
    CMPA #11
    BHI gate_done
gate_shot_hit:
    LDX shot_ptr
    CLR 0,X
    RTS

; Six shaded tetrahedra use geometry poses; their faces are rasterized on the CPU.
scout_move:
    TST 0,X
    BEQ scout_done
    LDAA 0,X
    SUBA 3,X
    STAA 0,X
    CMPA #8
    BHI scout_y
    CLR 0,X
    RTS
scout_y:
    LDAA 1,X
    ADDA 2,X
    STAA 1,X
    CMPA #18
    BLS scout_turn
    CMPA #46
    BCS scout_body
scout_turn:
    NEG 2,X
scout_body:
    LDAA ship_x
    SUBA 0,X
    JSR magnitude
    CMPA #9
    BHI scout_shots
    LDAA ship_y
    SUBA 1,X
    JSR magnitude
    CMPA #5
    BHI scout_shots
    LDAA #2
    STAA hit_cause
    JSR def_damage
    LDX entity_ptr
scout_shots:
    LDX #shots
    JSR scout_hit
    LDX #shots + 4
    JSR scout_hit
scout_done:
    RTS
scout_hit:
    TST 0,X
    BEQ scout_done
    STX shot_ptr
    LDAA 1,X
    LDAB 2,X
    LDX entity_ptr
    TST 0,X
    BEQ scout_done
    SUBA 0,X
    JSR magnitude
    CMPA #8
    BHI scout_done
    TBA
    SUBA 1,X
    JSR magnitude
    CMPA #5
    BHI scout_done
    LDAA 0,X
    STAA def_drop_x
    LDAA 1,X
    STAA def_drop_y
    CLR 0,X
    LDX shot_ptr
    LDAA 3,X
    BITA #128
    BNE scout_reward
    CLR 0,X
scout_reward:
    LDAA #1
    JSR def_add_score
    LDAA #3
    JSR def_audio_request
    INC scout_kills
    LDAA scout_kills
    ANDA #3
    BNE scout_done
    LDAA #3
    STAA def_drop_kind
    JMP def_drop

campaign_rush:
    LDAA spawn_left
    STAA campaign_busy
    LDX #swarm
scout_busy:
    LDAA 0,X
    ORAA campaign_busy
    STAA campaign_busy
    LDAB #4
    ABX
    CPX #swarm + 24
    BNE scout_busy
    TST scout_delay
    BEQ scout_spawn
    DEC scout_delay
    RTS
scout_spawn:
    TST spawn_left
    BEQ scout_done
    LDX #swarm
scout_free:
    TST 0,X
    BEQ scout_slot
    LDAB #4
    ABX
    CPX #swarm + 24
    BNE scout_free
    RTS
scout_slot:
    LDAA #184
    STAA 0,X
    LDAA spawn_left
    ANDA #3
    ASLA
    ASLA
    ASLA
    ADDA #20
    STAA 1,X
    LDAA spawn_left
    BITA #1
    BEQ scout_down
    LDAA #255
    BRA scout_dy
scout_down:
    LDAA #1
scout_dy:
    STAA 2,X
    LDAA spawn_left
    ANDA #1
    ADDA #3
    STAA 3,X
    DEC spawn_left
    LDAA #5
    STAA scout_delay
    RTS
magnitude:
    TSTA
    BPL magnitude_done
    NEGA
magnitude_done:
    RTS

campaign_draw:
    LDAA stage
    CMPA #1
    BEQ gates_draw
    CMPA #2
    BEQ scouts_draw
    CMPA #3
    BEQ laser_draw
    RTS
gates_draw:
    LDX #gates
    JSR gate_draw
    LDX #gates + 3
    JMP gate_draw
scouts_draw:
    LDX #swarm
scouts_draw_loop:
    STX entity_ptr
    TST 0,X
    BEQ scouts_draw_next
    LDAA 0,X
    STAA facet_x
    LDAA 1,X
    STAA facet_y
    LDAA def_tick
    ADDA 1,X
    LSRA
    ANDA #7
    ASLA
    TAB
    LDX #facet_scout_poses
    ABX
    LDX 0,X
    JSR facet_mesh
scouts_draw_next:
    LDX entity_ptr
    LDAB #4
    ABX
    CPX #swarm + 24
    BNE scouts_draw_loop
    RTS
laser_draw:
    TST sector
    BEQ laser_done
    LDAA enemies + 4
    CMPA #20
    BCS laser_done
    CMPA #33
    BCC laser_done
    CMPA #28
    BCC laser_beam
    LDAA #16
    STAA def_line
    LDAA enemies + 2
    STAA def_line + 2
    LDAA enemies + 5
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JMP p3_dashed_line
laser_beam:
    LDAA #16
    STAA def_line
    LDAA enemies + 2
    STAA def_line + 2
laser_height:
    LDAA enemies + 5
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JSR campaign_line
    INC def_line + 1
    INC def_line + 3
    JMP campaign_line
laser_done:
    RTS

gate_draw:
    TST 0,X
    BEQ gate_draw_done
    STX entity_ptr
    LDAA sector
    CMPA #2
    BNE crystal_draw
    LDAA 0,X
    STAA facet_x
    LDAA 1,X
    STAA facet_y
    LDAA def_tick
    LSRA
    ANDA #3
    ASLA
    TAB
    LDX #facet_rotor_poses
    ABX
    LDX 0,X
    JMP facet_mesh
gate_draw_done:
    RTS
crystal_draw:
    LDAA 0,X
    CMPA #128
    BCS crystal_solid
    JMP crystal_distant
crystal_solid:
    LDAA 0,X
    SUBA #10
    BCC crystal_left_ready
    CLRA
crystal_left_ready:
    STAA facet_left
    LDAA 0,X
    ADDA #10
    CMPA #192
    BCS crystal_right_ready
    LDAA #191
crystal_right_ready:
    STAA facet_right
    LDAA #8
    STAA facet_top
    LDAA #55
    STAA facet_bottom
    JSR facet_register
    LDAA facet_left
    STAA wall_column
crystal_column:
    LDAA wall_column
    ANDA #3
    BNE crystal_poll_done
    JSR input_poll
crystal_poll_done:
    LDX entity_ptr
    LDAA wall_column
    SUBA 0,X
    JSR magnitude
    LSRA
    STAA wall_indent
    LDAA 1,X
    SUBA #12
    SUBA wall_indent
    STAA wall_ceiling
    LDAA 1,X
    ADDA #12
    ADDA wall_indent
    STAA wall_floor
    LDAA #255
    LDAB wall_column
    CMPB 0,X
    BCC crystal_pattern
    ANDB #1
    BEQ crystal_light
    CLRA
    BRA crystal_pattern
crystal_light:
    LDAA #85
crystal_pattern:
    LDAB wall_column
    CMPB facet_left
    BEQ crystal_border
    CMPB facet_right
    BNE crystal_pattern_ready
crystal_border:
    LDAA #255
crystal_pattern_ready:
    STAA wall_ink
    LDAA #8
    STAA wall_y
    CLRA
    LDAB wall_column
    ADDD #framebuffer + 192
    STD wall_pointer
crystal_band:
    CLRB
    LDAA wall_ceiling
    SUBA wall_y
    BCS crystal_floor_mask
    CMPA #8
    BCS crystal_ceiling_partial
    LDAB #255
    BRA crystal_floor_mask
crystal_ceiling_partial:
    TAB
    LDX #crystal_end_masks
    ABX
    LDAB 0,X
crystal_floor_mask:
    STAB wall_mask
    LDAA wall_floor
    SUBA wall_y
    BLS crystal_floor_full
    CMPA #8
    BCC crystal_mask_ready
    TAB
    LDX #crystal_start_masks
    ABX
    LDAB 0,X
    ORAB wall_mask
    BRA crystal_mask_ready
crystal_floor_full:
    LDAB #255
crystal_mask_ready:
    TSTB
    BEQ crystal_next_band
    STAB wall_mask
    LDAA wall_ink
    ANDA wall_mask
    STAA wall_value
    LDAA wall_mask
    COMA
    LDX wall_pointer
    ANDA 0,X
    ORAA wall_value
    STAA 0,X
crystal_next_band:
    LDD wall_pointer
    ADDD #192
    STD wall_pointer
    LDAA wall_y
    ADDA #8
    STAA wall_y
    CMPA #56
    BCS crystal_band
    LDAA wall_column
    CMPA facet_right
    BEQ crystal_done
    INC wall_column
    JMP crystal_column
crystal_done:
    RTS
.section .text, code
crystal_distant:
    LDAA #192
    SUBA 0,X
    LSRA
    LSRA
    LSRA
    ADDA #2
    STAA wall_indent
    LDAA 0,X
    SUBA wall_indent
    STAA def_line
    LDAA 0,X
    ADDA wall_indent
    CMPA #192
    BCS distant_right
    LDAA #191
distant_right:
    STAA def_line + 2
    LDAA #8
    STAA def_line + 1
    LDAA 1,X
    SUBA #14
    STAA def_line + 3
    JSR distant_box
    LDX entity_ptr
    LDAA 1,X
    ADDA #14
    STAA def_line + 1
    LDAA #55
    STAA def_line + 3
    JMP distant_box
distant_box:
    ; Distant crystal faces stay as thin wire polygons until they approach.
    LDD def_line
    STD distant_save
    LDD def_line + 2
    STD distant_save + 2
    LDAA def_line
    STAA def_line + 2
    JSR campaign_line
    LDD distant_save
    STD def_line
    LDD distant_save + 2
    STD def_line + 2
    LDAA def_line + 2
    STAA def_line
    JSR campaign_line
    LDD distant_save
    STD def_line
    LDD distant_save + 2
    STD def_line + 2
    LDAA def_line + 1
    STAA def_line + 3
    JSR campaign_line
    LDD distant_save
    STD def_line
    LDD distant_save + 2
    STD def_line + 2
    LDAA def_line + 3
    STAA def_line + 1
    JSR campaign_line
    LDD distant_save
    STD def_line
    LDD distant_save + 2
    STD def_line + 2
    RTS
.section .campaign, code
crystal_start_masks: .byte 255,254,252,248,240,224,192,128
crystal_end_masks: .byte 1,3,7,15,31,63,127,255
campaign_line:
    LDX #def_line
    JMP p3_line
.section .bss, bss
campaign_state:
gates: .space 6
gate_left: .space 1
gate_delay: .space 1
swarm: .space 24
scout_delay: .space 1
scout_kills: .space 1
campaign_busy: .space 1
entity_ptr: .space 2
shot_ptr: .space 2
wall_column: .space 1
wall_indent: .space 1
wall_ceiling: .space 1
wall_floor: .space 1
wall_ink: .space 1
wall_y: .space 1
wall_pointer: .space 2
wall_mask: .space 1
wall_value: .space 1
gate_margin: .space 1
distant_save: .space 4
campaign_end:
