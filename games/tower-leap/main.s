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
    INC tower_index
    LDAA tower_index
    CMPA #12
    BNE tower_load
    JMP tower_restore
game_aux:
    CMPA #2
    BEQ game_start
    JMP tower_die
tower_restore:
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
    TST tower_grounded
    BEQ tower_change
    LDAA input_held
    ANDA #12
    BNE tower_change
    RTS
tower_change:
    JSR tower_erase
    JSR tower_step
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
    LDAA tower_tile_col
    SUBA 0,X
    CMPA #3
    BCC tower_tile_wall
    LDAA tower_tile_level
    ANDA #3
    BEQ tower_tile_checkpoint
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
    JSR paint_board
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
    CLR tower_hud_force
    TST resume_pending
    BEQ tower_hud_values
    INC tower_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #tower_jump_label
    LDAA #78
    CLRB
    JSR paint_text
    LDX #tower_floor_label
    LDAA #132
    LDAB #1
    JSR paint_text
    LDX #tower_life_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #tower_save_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #tower_menu_label
    LDAA #132
    LDAB #7
    JSR paint_text
tower_hud_values:
    TST tower_hud_force
    BNE tower_floor_value
    LDAA tower_highest
    CMPA tower_old_highest
    BEQ tower_life_check
tower_floor_value:
    LDAA tower_highest
    STAA tower_old_highest
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA tower_highest
    INCA
    JSR paint_number
tower_life_check:
    TST tower_hud_force
    BNE tower_life_value
    LDAA tower_lives
    CMPA tower_old_lives
    BEQ tower_save_check
tower_life_value:
    LDAA tower_lives
    STAA tower_old_lives
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA tower_lives
    JSR paint_number
tower_save_check:
    TST tower_hud_force
    BNE tower_save_value
    LDAA tower_checkpoint
    CMPA tower_old_checkpoint
    BEQ tower_render_done
tower_save_value:
    LDAA tower_checkpoint
    STAA tower_old_checkpoint
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA tower_checkpoint
    INCA
    JMP paint_number
tower_render_done:
    RTS
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
