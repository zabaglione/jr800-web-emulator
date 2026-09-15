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
.extern p3_line
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
    CMPA #11
    BHI gate_shots
    LDAA ship_y
    SUBA 1,X
    JSR magnitude
    CMPA #9
    BCS gate_shots
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
    CMPA #10
    BHI gate_done
    TBA
    SUBA 1,X
    JSR magnitude
    CMPA #12
    BCS gate_done
    LDX shot_ptr
    CLR 0,X
    RTS

; Six small shaded polygon silhouettes form fast crossing waves. The large
; enemies still rotate through the 3D renderer; this pool has no matrix work.
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
    CLR scout_row
scout_rows:
    LDX entity_ptr
    LDAA 0,X
    STAA scout_origin
    LDAA 1,X
    SUBA #3
    ADDA scout_row
    STAA def_line + 1
    STAA def_line + 3
    LDAB scout_row
    ASLB
    LDX #scout_facets
    ABX
    LDAA 0,X
    ADDA scout_origin
    STAA def_line
    LDAA 1,X
    ADDA scout_origin
    STAA def_line + 2
    JSR campaign_line
    INC scout_row
    LDAA scout_row
    CMPA #7
    BCS scout_rows
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
    CMPA #8
    BCS laser_done
    CMPA #33
    BCC laser_done
    CMPA #28
    BCC laser_beam
    LDAA def_tick
    BITA #1
    BNE laser_done
    LDAA #16
    STAA def_line
    LDAA #36
    STAA def_line + 2
    BRA laser_height
laser_beam:
    LDAA #16
    STAA def_line
    LDAA enemies + 2
    SUBA #8
    STAA def_line + 2
laser_height:
    LDAA enemies + 5
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JSR campaign_line
    LDAA enemies + 4
    CMPA #28
    BCS laser_done
    INC def_line + 1
    INC def_line + 3
    JMP campaign_line
laser_done:
    RTS

gate_draw:
    TST 0,X
    BEQ laser_done
    STX entity_ptr
    LDAA #8
    STAA wall_top
    LDAA 1,X
    SUBA #12
    STAA wall_bottom
    JSR wall_draw
    LDX entity_ptr
    LDAA 1,X
    ADDA #12
    STAA wall_top
    LDAA #55
    STAA wall_bottom
wall_draw:
    LDX entity_ptr
    LDAA 0,X
    SUBA #4
    STAA def_line
    STAA def_line + 2
    LDAA wall_top
    STAA def_line + 1
    LDAA wall_bottom
    STAA def_line + 3
    LDX #def_line
    JSR campaign_line
    LDAA def_line
    ADDA #8
    STAA def_line + 2
    LDAA wall_top
    STAA def_line + 3
    JSR campaign_line
    LDAA def_line + 2
    STAA def_line
    LDAA wall_bottom
    STAA def_line + 3
    JSR campaign_line
    LDAA def_line
    SUBA #8
    STAA def_line + 2
    LDAA wall_bottom
    STAA def_line + 1
    JSR campaign_line
    LDAA def_line
    SUBA #2
    STAA def_line
    STAA def_line + 2
    LDAA wall_top
    STAA def_line + 1
    LDAA wall_bottom
    STAA def_line + 3
    ; Axis-aligned shadow edge avoids rasterizing a tall diagonal every frame.
    JMP campaign_line
campaign_line:
    LDX #def_line
    JMP p3_line
scout_facets: .byte 3,3,1,3,255,3,253,5,255,2,1,2,3,3
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
scout_row: .space 1
scout_origin: .space 1
wall_top: .space 1
wall_bottom: .space 1
campaign_end:
