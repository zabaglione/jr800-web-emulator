; SPDX-License-Identifier: MIT
.global tower_platforms
.global tower_x
.global tower_height
.global tower_velocity
.global tower_grounded
.global tower_camera
.global tower_lives
.global tower_highest
.global tower_checkpoint
.global tower_step
.global tower_die
.global tower_steps
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    CLR tower_environment_ticks
    CLR tower_motion_phase
    CLR tower_steps
    CLR tower_highest
    CLR tower_checkpoint
    LDAA #3
    STAA tower_lives
    LDAA stage
    LDAB #12
    MUL
    ADDD #tower_levels
    STD tower_pointer
    CLR tower_index
tower_load:
    LDX tower_pointer
    LDAA 0,X
    INX
    STX tower_pointer
    LDAB tower_index
    LDX #tower_platforms
    ABX
    STAA 0,X
    LDX #tower_origins
    ABX
    STAA 0,X
    INC tower_index
    LDAA tower_index
    CMPA #12
    BNE tower_load
    LDAA stage
    LDAB #12
    MUL
    ADDD #tower_kinds
    STD tower_kind_source
    LDAA stage
tower_enemy_floor_mod:
    CMPA #3
    BCS tower_enemy_floor_ready
    SUBA #3
    BRA tower_enemy_floor_mod
tower_enemy_floor_ready:
    ADDA #5
    ASLA
    ASLA
    ASLA
    ASLA
    ADDA #10
    STAA tower_enemy_height
    LDAA #14
    SUBA stage
    CMPA #6
    BCC tower_crumble_delay
    LDAA #6
tower_crumble_delay:
    STAA tower_crumble_limit
    JMP tower_restore
game_aux:
    CMPA #2
    BNE tower_aux_checkpoint
    JMP game_start
tower_aux_checkpoint:
    JMP tower_die
tower_restore:
    CLRB
tower_restore_platforms:
    LDX #tower_origins
    ABX
    LDAA 0,X
    LDX #tower_platforms
    ABX
    STAA 0,X
    LDX #tower_crumble
    ABX
    CLR 0,X
    INCB
    CMPB #12
    BNE tower_restore_platforms
    LDAA #32
    STAA tower_enemy_x
    LDAA #1
    STAA tower_enemy_dir
    LDAB tower_checkpoint
    LDX #tower_platforms
    ABX
    LDAA 0,X
    ASLA
    ASLA
    ASLA
    ADDA #10
    STAA tower_x
    TBA
    ASLA
    ASLA
    ASLA
    ASLA
    STAA tower_height
    SUBA #16
    BCC tower_restore_camera
    CLRA
tower_restore_camera:
    STAA tower_camera
    CLR tower_velocity
    LDAA #1
    STAA tower_grounded
    LDAA input_ticks
    STAA tower_clock
    JMP tower_invalidate
tower_die:
    DEC tower_lives
    BEQ tower_fail
    JMP tower_restore
tower_fail:
    LDAA #5
    STAA phase
    RTS
game_update:
    TST resume_pending
    BEQ tower_input
    CLR resume_pending
    LDAA input_ticks
    STAA tower_clock
tower_input:
    LDAA input_event
    BITA #16
    BEQ tower_timer
    TST tower_grounded
    BEQ tower_timer
    CLR tower_grounded
    LDAA #7
    STAA tower_velocity
tower_timer:
    LDAA input_ticks
    SUBA tower_clock
    CMPA #4
    BCC tower_due
    RTS
tower_due:
    LDAA input_ticks
    STAA tower_clock
tower_change:
    JSR tower_erase
    JSR tower_erase_enemy
    JSR tower_environment
    JSR tower_step
    JSR tower_enemy_contact
    LDAA #1
    STAA redraw
    RTS
; Integer ballistic step, with top-only platform contacts on descent.
tower_step:
    INC tower_steps
    LDAA input_held
    BITA #4
    BEQ tower_right
    LDAA tower_x
    SUBA #2
    BCC tower_move
    CLRA
    BRA tower_move
tower_right:
    BITA #8
    BEQ tower_vertical
    LDAA tower_x
    ADDA #2
    CMPA #125
    BCS tower_move
    LDAA #124
tower_move:
    STAA tower_x
tower_vertical:
    LDAA tower_height
    STAA tower_previous
    ADDA tower_velocity
    STAA tower_candidate
    TST tower_velocity
    BPL tower_rising
    LDAA tower_candidate
    CMPA tower_previous
    BLS tower_descend
    JMP tower_die
tower_rising:
    BNE tower_airborne
tower_descend:
    ; At most two platform heights can be crossed; scanning twelve is bounded.
    LDAA #11
    STAA tower_index
    LDAA #176
    STAA tower_level_height
tower_collision_loop:
    LDAA tower_level_height
    CMPA tower_previous
    BHI tower_collision_next
    CMPA tower_candidate
    BCS tower_collision_next
    LDAB tower_index
    LDX #tower_platforms
    ABX
    LDAA 0,X
    CMPA #255
    BEQ tower_collision_next
    ASLA
    ASLA
    ASLA
    STAA tower_left
    LDAA tower_x
    ADDA #3
    CMPA tower_left
    BCS tower_collision_next
    LDAA tower_left
    ADDA #24
    CMPA tower_x
    BLS tower_collision_next
    JMP tower_land
tower_collision_next:
    LDAA tower_level_height
    SUBA #16
    STAA tower_level_height
    DEC tower_index
    BPL tower_collision_loop
tower_airborne:
    CLR tower_grounded
    LDAA tower_candidate
    STAA tower_height
    CMPA tower_camera
    BCC tower_above_bottom
    JMP tower_die
tower_above_bottom:
    DEC tower_velocity
    LDAA tower_height
    SUBA tower_camera
    CMPA #41
    BCS tower_step_done
    LDAA tower_height
    SUBA #32
    ANDA #240
    STAA tower_camera
    JMP tower_invalidate
tower_land:
    LDAA tower_level_height
    STAA tower_height
    CLR tower_velocity
    LDAA #1
    STAA tower_grounded
    LDAA tower_index
    CMPA tower_highest
    BLS tower_step_done
    STAA tower_highest
    ANDA #252
    STAA tower_checkpoint
    LDAA tower_highest
    CMPA #11
    BNE tower_step_done
    LDAA #4
    STAA phase
tower_step_done:
    RTS
tower_invalidate:
    LDX #tile_cache
    LDAB #112
    LDAA #255
tower_invalidate_loop:
    STAA 0,X
    INX
    DECB
    BNE tower_invalidate_loop
    RTS
tower_erase:
    LDAA #4
    STAA scene_w
    LDAA #6
    STAA scene_h
    LDAA tower_height
    SUBA tower_camera
    STAA tower_draw_delta
    LDAB #58
    SUBB tower_draw_delta
    LDAA tower_x
    JMP scene_rect
game_tile:
    TBA
    ANDA #15
    STAA tower_tile_col
    LSRB
    LSRB
    LSRB
    LSRB
    STAB tower_tile_row
    LDAA #6
    SUBA tower_tile_row
    ASLA
    ASLA
    ASLA
    ADDA tower_camera
    BITA #15
    BNE tower_tile_wall
    LSRA
    LSRA
    LSRA
    LSRA
    CMPA #12
    BCC tower_tile_wall
    STAA tower_tile_level
    TAB
    LDX #tower_platforms
    ABX
    LDAA 0,X
    CMPA #255
    BEQ tower_tile_wall
    LDAA tower_tile_col
    SUBA 0,X
    CMPA #3
    BCC tower_tile_wall
    LDAA tower_tile_level
    ANDA #3
    BEQ tower_tile_checkpoint
    LDAB tower_tile_level
    LDX tower_kind_source
    ABX
    LDAA 0,X
    BEQ tower_tile_plain
    ADDA #3
    CMPA #5
    BNE tower_tile_kind_done
    LDX #tower_crumble
    ABX
    LDAB 0,X
    CMPB #4
    BCS tower_tile_kind_done
    INCA
tower_tile_kind_done:
    RTS
tower_tile_plain:
    LDAA #1
    RTS
tower_tile_checkpoint:
    LDAA #2
    RTS
tower_tile_wall:
    LDAA tower_tile_col
    BEQ tower_tile_brick
    CMPA #15
    BEQ tower_tile_brick
    CLRA
    RTS
tower_tile_brick:
    LDAA #3
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    RTS
actor_render_scene:
    JSR paint_board
    JSR tower_draw_enemy
    LDAA tower_height
    SUBA tower_camera
    STAA tower_draw_delta
    LDAA #58
    SUBA tower_draw_delta
    STAA tower_draw_y
    CLR tower_draw_index
tower_draw_hero:
    LDAB tower_draw_index
    LDX #tower_sprite_x
    ABX
    LDAA 0,X
    ADDA tower_x
    STAA tower_draw_x
    LDX #tower_sprite_y
    ABX
    LDAB 0,X
    ADDB tower_draw_y
    LDAA tower_draw_x
    JSR scene_pixel
    INC tower_draw_index
    LDAA tower_draw_index
    CMPA #14
    BNE tower_draw_hero
    JMP visual_hud
.section .bss, bss
tower_steps: .space 1
tower_platforms: .space 12
tower_x: .space 1
tower_height: .space 1
tower_velocity: .space 1
tower_grounded: .space 1
tower_camera: .space 1
tower_lives: .space 1
tower_highest: .space 1
tower_checkpoint: .space 1
tower_clock: .space 1
tower_pointer: .space 2
tower_index: .space 1
tower_previous: .space 1
tower_candidate: .space 1
tower_level_height: .space 1
tower_left: .space 1
tower_tile_col: .space 1
tower_tile_row: .space 1
tower_tile_level: .space 1
tower_draw_delta: .space 1
tower_draw_y: .space 1
tower_draw_x: .space 1
tower_draw_index: .space 1
tower_hud_force: .space 1
tower_old_highest: .space 1
tower_old_lives: .space 1
tower_old_checkpoint: .space 1
.section .data, data
tower_jump_label: .byte 83,80,65,67,69,32,74,85,77,80,0
tower_floor_label: .byte 70,76,79,79,82,0
tower_life_label: .byte 76,73,70,69,0
tower_save_label: .byte 83,65,86,69,0
tower_menu_label: .byte 82,69,84,85,82,78,0

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    LDAA tower_x
    ADDA #VIEW_X
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA paint_x
    STAA actor_intro_x
    LDAA paint_band
    STAA actor_intro_band
    CLR actor_intro_step
    CLR actor_intro_bytes
    CLR actor_intro_bytes + 1
actor_intro_blink:
    LDAA actor_intro_band
    STAA paint_band
    LDAA actor_intro_x
    STAA paint_x
    JSR paint_address
    LDAA #2
    STAA actor_intro_rows
actor_intro_row:
    LDX paint_dest
    LDAB #8
actor_intro_pixels:
    COM 0,X
    INX
    DECB
    BNE actor_intro_pixels
    LDAA paint_band
    LDAB actor_intro_x
    JSR dirty_mark
    LDAA paint_band
    LDAB actor_intro_x
    ADDB #7
    JSR dirty_mark
    LDD paint_dest
    ADDD #192
    STD paint_dest
    INC paint_band
    DEC actor_intro_rows
    BNE actor_intro_row
    JSR dirty_begin
actor_intro_transfer:
    JSR dirty_next
    BEQ actor_intro_sum
    JSR input_poll
    BRA actor_intro_transfer
actor_intro_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
actor_intro_frame:
    TST actor_intro_step
    BNE actor_intro_wait
    LDX #180
    LDD #20
    JSR sound_tone
actor_intro_wait:
    LDAA input_ticks
    STAA actor_intro_clock
actor_intro_delay:
    JSR input_poll
    LDAA input_ticks
    SUBA actor_intro_clock
    CMPA #8
    BCS actor_intro_delay
    INC actor_intro_step
    LDAA actor_intro_step
    CMPA #4
    BEQ actor_intro_done
    JMP actor_intro_blink
actor_intro_done:
    JSR input_gate
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD actor_intro_bytes
    STD dirty_bytes
    ; Finish the complete shell frame for both stage starts and menu resets.
    LDS #$5FFF
    JMP frame_ready
.section .bss, bss
actor_intro_pending: .space 1
actor_intro_x: .space 1
actor_intro_band: .space 1
actor_intro_step: .space 1
actor_intro_clock: .space 1
actor_intro_rows: .space 1
actor_intro_bytes: .space 2

; Moving platforms and the drone advance on the same CPU-cycle clock as jumps.
; Checkpoint floors remain permanent, making every recovery position stable.
.global tower_origins
.global tower_crumble
.global tower_environment_ticks
.global tower_motion_phase
.global tower_enemy_x
.global tower_enemy_dir
.global tower_enemy_height
.global tower_crumble_limit
.section .text, code
tower_environment:
    INC tower_environment_ticks
    LDAA tower_environment_ticks
    CMPA #12
    BCS tower_environment_begin
    CLR tower_environment_ticks
    INC tower_motion_phase
    LDAA tower_motion_phase
    ANDA #3
    STAA tower_motion_phase
tower_environment_begin:
    CLR tower_env_index
tower_platform_tick:
    LDAB tower_env_index
    LDX tower_kind_source
    ABX
    LDAA 0,X
    CMPA #1
    BEQ tower_platform_move
    CMPA #2
    BNE tower_platform_next
    TST tower_grounded
    BEQ tower_platform_next
    TBA
    ASLA
    ASLA
    ASLA
    ASLA
    CMPA tower_height
    BNE tower_platform_next
    LDX #tower_crumble
    ABX
    INC 0,X
    LDAA 0,X
    CMPA tower_crumble_limit
    BCS tower_platform_dirty
    LDX #tower_platforms
    ABX
    LDAA #255
    STAA 0,X
    BRA tower_platform_dirty
tower_platform_move:
    TST tower_environment_ticks
    BNE tower_platform_next
    LDAB tower_motion_phase
    LDX #tower_motion_offsets
    ABX
    LDAA 0,X
    LDAB tower_env_index
    LDX #tower_origins
    ABX
    ADDA 0,X
    STAA tower_env_position
    LDX #tower_platforms
    ABX
    SUBA 0,X
    ASLA
    ASLA
    ASLA
    STAA tower_env_delta
    LDAA tower_env_position
    STAA 0,X
    TST tower_grounded
    BEQ tower_platform_dirty
    TBA
    ASLA
    ASLA
    ASLA
    ASLA
    CMPA tower_height
    BNE tower_platform_dirty
    LDAA tower_x
    ADDA tower_env_delta
    STAA tower_x
tower_platform_dirty:
    JSR tower_invalidate
tower_platform_next:
    INC tower_env_index
    LDAA tower_env_index
    CMPA #12
    BEQ tower_enemy_tick
    JMP tower_platform_tick
tower_enemy_tick:
    LDAA tower_enemy_x
    ADDA tower_enemy_dir
    STAA tower_enemy_x
    CMPA #32
    BEQ tower_enemy_reverse
    CMPA #96
    BNE tower_environment_done
tower_enemy_reverse:
    NEG tower_enemy_dir
tower_environment_done:
    RTS
tower_enemy_contact:
    LDAA phase
    CMPA #2
    BNE tower_environment_done
    LDAA tower_height
    ADDA #5
    CMPA tower_enemy_height
    BCS tower_environment_done
    LDAA tower_enemy_height
    ADDA #4
    CMPA tower_height
    BLS tower_environment_done
    LDAA tower_x
    ADDA #3
    CMPA tower_enemy_x
    BCS tower_environment_done
    LDAA tower_enemy_x
    ADDA #5
    CMPA tower_x
    BLS tower_environment_done
    JMP tower_die
tower_enemy_position:
    LDAA tower_enemy_height
    SUBA tower_camera
    STAA tower_draw_delta
    LDAA #60
    SUBA tower_draw_delta
    STAA tower_enemy_y
    RTS
tower_erase_enemy:
    JSR tower_enemy_position
    LDAA #5
    STAA scene_w
    LDAA #4
    STAA scene_h
    LDAA tower_enemy_x
    LDAB tower_enemy_y
    JMP scene_rect
tower_draw_enemy:
    JSR tower_enemy_position
    CLR tower_enemy_dot
tower_enemy_pixel:
    LDAB tower_enemy_dot
    LDX #tower_enemy_dx
    ABX
    LDAA 0,X
    ADDA tower_enemy_x
    STAA tower_draw_x
    LDX #tower_enemy_dy
    ABX
    LDAB 0,X
    ADDB tower_enemy_y
    LDAA tower_draw_x
    JSR scene_pixel
    INC tower_enemy_dot
    LDAA tower_enemy_dot
    CMPA #14
    BNE tower_enemy_pixel
    RTS
.section .data, data
tower_motion_offsets: .byte 0,1,0,255
tower_enemy_dx: .byte 1,2,3,0,1,3,4,0,2,4,0,1,3,4
tower_enemy_dy: .byte 0,0,0,1,1,1,1,2,2,2,3,3,3,3
.section .bss, bss
tower_origins: .space 12
tower_crumble: .space 12
tower_kind_source: .space 2
tower_crumble_limit: .space 1
tower_environment_ticks: .space 1
tower_motion_phase: .space 1
tower_env_index: .space 1
tower_env_position: .space 1
tower_env_delta: .space 1
tower_enemy_height: .space 1
tower_enemy_x: .space 1
tower_enemy_dir: .space 1
tower_enemy_y: .space 1
tower_enemy_dot: .space 1
