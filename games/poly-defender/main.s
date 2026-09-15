; SPDX-License-Identifier: MIT
; All combat, item collection, waves, and timing execute on the JR-800.
; Enemy: kind,hp,x,y,age,target_y,vy,scale,loot,flash (10 bytes).
; Kind: 0 absent, 1 gunner, 2 diver, 3 splitter, 4 fragment, 5 boss.
; Player shot: active,x,y,flags (bit 7 piercing; bits 0/1 already hit).
; Enemy bolt: life,xQ4(16),yQ4(16),vxQ4(16),vyQ4(8 signed).
.global app_init
.global app_update
.global app_draw
.global phase
.global stage
.global sector
.global charge
.global message_ticks
.global score_hi
.extern campaign_update
.extern campaign_draw
.extern campaign_reset
.extern campaign_rush
.extern campaign_busy
.extern gates
.extern gate_left
.extern swarm
.extern music_tick
.global def_damage
.global def_add_score
.global def_drop
.global def_drop_kind
.global def_drop_x
.global def_drop_y
.global def_line
.global def_enemy_ptr
.global def_new_generation
.global score
.global hull
.global weapon
.global shield
.global ship_x
.global ship_y
.global pause_ticks
.global hit_cause
.global screen_inverted
.global fx_age
.global fire_enabled
.global invulnerable
.global enemies
.global shots
.global bolts
.global item
.global spawn_left
.global def_tick
.global boss_mode
.extern def_audio_init
.extern def_audio_pump
.extern def_audio_request
.extern audio_id
.extern dirty_all
.extern scene_clear
.extern text
.extern glyph
.extern framebuffer
.extern ui_footer
.extern ui_number
.extern hud_dirty
.extern dirty_min
.extern dirty_max
.extern dirty_pending
.extern input_event
.extern input_held
.extern input_ticks
.extern input_poll
.extern p3_draw
.extern p3_line
.extern p3_x
.extern p3_y
.extern p3_yaw
.extern p3_pitch
.extern p3_scale
.extern p3_mode
.extern p3_model_tetra
.extern p3_model_octa
.extern p3_model_prism
.section .text, code
app_init:
    JSR def_audio_init
    CLR screen_inverted
    CLR phase
    LDAA input_ticks
    STAA def_clock
    CLR def_tick
    LDX #def_title
    LDD #framebuffer + 57
    JSR text
    LDX #def_intro
    LDD #framebuffer + 192 + 15
    JSR text
    LDX #def_rules
    LDD #framebuffer + 384 + 24
    JSR text
    LDX #def_start
    JMP ui_footer
app_update:
    ; Presentation cues hold the already displayed frame, with input/BREAK polls
    ; between two-wave fragments. Gameplay SE never enters this hold loop.
    LDAA phase
    CMPA #2
    BCS def_update_ready
def_presentation_sound:
    TST audio_id
    BEQ def_update_ready
    JSR input_poll
    JSR def_audio_pump
    BRA def_presentation_sound
def_update_ready:
    ; Always restore the logical framebuffer before the renderer clears spans.
    TST screen_inverted
    BEQ def_wait
    JSR def_invert
    CLR screen_inverted
def_wait:
    JSR input_poll
    LDAA input_ticks
    SUBA def_clock
    CMPA #6
    BCC def_wait_done
    CMPA #5
    BCC def_wait
    JSR def_audio_pump
    BRA def_wait
def_wait_done:
    LDAA input_ticks
    STAA def_clock
    LDAA phase
    CMPA #1
    BEQ def_play
    CMPA #4
    BCC def_presentation
    INC def_tick
    LDAA input_event
    ANDA #48
    BNE def_branch_1
    JMP def_idle
def_branch_1:
    JMP def_new
def_idle:
    RTS
def_presentation:
    JMP def_pause_update
def_play:
    INC def_tick
    TST fx_age
    BEQ def_tick_shield
    DEC fx_age
def_tick_shield:
    TST invulnerable
    BEQ def_controls_update
    DEC invulnerable
def_controls_update:
    TST message_ticks
    BEQ def_charge_tick
    DEC message_ticks
    BNE def_charge_tick
    JSR def_footer
def_charge_tick:
    TST fire_enabled
    BNE def_controls_input
    LDAA charge
    CMPA #18
    BCC def_controls_input
    INC charge
    LDAA charge
    CMPA #18
    BNE def_controls_input
    JSR def_footer
def_controls_input:
    LDAA input_event
    BITA #16
    BEQ def_move
    LDAA fire_enabled
    EORA #1
    STAA fire_enabled
    BEQ def_charge_begin
    LDAA charge
    CMPA #18
    BCS def_charge_begin
    LDAA #1
    STAA burst_pending
    CLR def_cooldown
def_charge_begin:
    CLR charge
    JSR def_footer
def_move:
    LDAA input_held
    ORAA input_event
    BITA #1
    BEQ def_move_down
    LDAB ship_y
    CMPB #16
    BLS def_move_down
    SUBB #2
    STAB ship_y
def_move_down:
    BITA #2
    BEQ def_move_left
    LDAB ship_y
    CMPB #48
    BCC def_move_left
    ADDB #2
    STAB ship_y
def_move_left:
    BITA #4
    BEQ def_move_right
    LDAB ship_x
    CMPB #16
    BLS def_move_right
    SUBB #2
    STAB ship_x
def_move_right:
    BITA #8
    BEQ def_update_shots
    LDAB ship_x
    CMPB #112
    BCC def_update_shots
    ADDB #2
    STAB ship_x
def_update_shots:
    CLRA
    LDAB ship_x
    CMPB #72
    BCS def_bonus_zone
    INCA
def_bonus_zone:
    CMPA front_zone
    BEQ def_bonus_ready
    STAA front_zone
    JSR def_footer
def_bonus_ready:
    JSR music_tick
    JSR campaign_update
    LDAA phase
    CMPA #1
    BEQ def_update_projectiles
    RTS
def_update_projectiles:
    LDX #shots
    JSR def_shot_move
    LDX #shots + 4
    JSR def_shot_move
    JSR def_fire
    LDX #enemies
    LDAA #1
    STAA def_enemy_bit
    JSR def_enemy_update
    LDAA phase
    CMPA #1
    BEQ def_branch_2
    JMP def_idle
def_branch_2:
    LDX #enemies + 10
    LDAA #2
    STAA def_enemy_bit
    JSR def_enemy_update
    LDAA phase
    CMPA #1
    BEQ def_far_1
    JMP def_idle
def_far_1:
    LDX #bolts
    JSR def_bolt_move
    LDAA phase
    CMPA #1
    BEQ def_far_2
    JMP def_idle
def_far_2:
    LDX #bolts + 8
    JSR def_bolt_move
    LDAA phase
    CMPA #1
    BEQ def_far_15
    JMP def_idle
def_far_15:
    JSR def_item_update
    JMP def_waves

def_shot_move:
    TST 0,X
    BEQ def_shot_done
    LDAA 1,X
    ADDA #8
    STAA 1,X
    CMPA #186
    BCS def_shot_done
    CLR 0,X
def_shot_done:
    RTS

def_fire:
    TST def_cooldown
    BEQ def_fire_ready
    DEC def_cooldown
def_fire_ready:
    TST fire_enabled
    BEQ def_shot_done
    TST def_cooldown
    BNE def_shot_done
    LDAA weapon
    CMPA #1
    BNE def_find_shot
    LDAA shots
    ORAA shots + 4
    BNE def_shot_done
def_find_shot:
    LDX #shots
    TST 0,X
    BEQ def_fire_slot
    LDX #shots + 4
    TST 0,X
    BNE def_shot_done
def_fire_slot:
    LDAA #1
    STAA 0,X
    LDAA ship_x
    ADDA #8
    STAA 1,X
    LDAA ship_y
    STAA 2,X
    CLR 3,X
    TST burst_pending
    BEQ def_regular_shot
    CLR burst_pending
    LDAA #192
    STAA 3,X
    LDAA #14
    JSR def_audio_request
    BRA def_fire_delay
def_regular_shot:
    LDAA weapon
    CMPA #3
    BNE def_fire_delay
    LDAA #128
    STAA 3,X
def_fire_delay:
    LDAA #5
    STAA def_cooldown
    LDAA #1
    JMP def_audio_request

; X is saved across routines that use the renderer or other records.
def_enemy_update:
    TST 0,X
    BNE def_enemy_live
    RTS
def_enemy_live:
    STX def_enemy_ptr
    LDAA 4,X
    BPL def_branch_3
    JMP def_enter
def_branch_3:
    INC 4,X
    TST 9,X
    BEQ def_enemy_kind
    DEC 9,X
def_enemy_kind:
    LDAA 0,X
    CMPA #5
    BNE def_not_boss
    JSR def_boss_update
    JMP def_enemy_collisions
def_not_boss:
    CMPA #3
    BNE def_not_parent
    LDAA 4,X
    CMPA #12
    BNE lock_far_1
    JMP def_lock_enemy
lock_far_1:
    CMPA #24
    BNE def_branch_4
    JMP def_shoot_enemy
def_branch_4:
    CMPA #48
    BCC def_branch_5
    JMP def_enemy_collisions
def_branch_5:
    CLR 4,X
    JMP def_enemy_collisions
def_not_parent:
    CMPA #4
    BNE def_regular
    ; Two fragments peel into separate lanes, then travel left.
    LDAA 3,X
    ADDA 6,X
    STAA 3,X
    CMPA #18
    BLS def_fragment_turn
    CMPA #46
    BCS def_fragment_x
def_fragment_turn:
    NEG 6,X
def_fragment_x:
    DEC 2,X
    JMP def_enemy_collisions
def_regular:
    LDAB 0,X
    CMPB #6
    BCS def_regular_old
    LDAA 2,X
    SUBA #2
    CMPB #7
    BNE def_weave_x
    SUBA #2
def_weave_x:
    STAA 2,X
    LDAA 3,X
    ADDA 6,X
    STAA 3,X
    CMPA #18
    BLS def_weave_turn
    CMPA #46
    BCS def_weave_done
def_weave_turn:
    NEG 6,X
def_weave_done:
    JMP def_enemy_collisions
def_regular_old:
    LDAA 4,X
    CMPA #8
    BNE lock_far_2
    JMP def_lock_enemy
lock_far_2:
    CMPA #20
    BNE def_branch_6
    JMP def_shoot_enemy
def_branch_6:
    LDAB 0,X
    CMPB #2
    BEQ def_diver
    CMPA #40
    BNE lock_far_3
    JMP def_lock_enemy
lock_far_3:
    CMPA #52
    BNE def_branch_7
    JMP def_shoot_enemy
def_branch_7:
    CMPA #64
    BCC def_branch_8
    JMP def_enemy_collisions
def_branch_8:
    CLR 4,X
    JMP def_enemy_collisions
def_diver:
    CMPA #24
    BCC def_branch_9
    JMP def_enemy_collisions
def_branch_9:
    LDAA 2,X
    SUBA #3
    STAA 2,X
    ; A dive commits to the warned lane; it does not track the player.
    JSR def_move_to_aim
    JMP def_enemy_collisions
def_lock_enemy:
    LDAA ship_x
    STAA 6,X
    LDAA ship_y
    STAA 5,X
    JMP def_enemy_collisions
def_shoot_enemy:
    JSR def_launch_bolt
    JMP def_enemy_collisions
def_enter:
    ; Approach from a tiny distant point at the right edge, growing as it enters.
    LDAA 2,X
    SUBA #3
    STAA 2,X
    CMPA #153
    BHI def_enter_done
    LDAA #152
    STAA 2,X
    CLR 4,X
def_enter_done:
    RTS

def_enemy_collisions:
    LDX def_enemy_ptr
    TST 0,X
    BEQ def_enemy_return
    LDAA 2,X
    SUBA ship_x
    BPL def_body_dx
    NEGA
def_body_dx:
    CMPA #13
    BHI def_enemy_escape_check
    LDAA 3,X
    SUBA ship_y
    BPL def_body_distance
    NEGA
def_body_distance:
    CMPA #8
    BHI def_enemy_escape_check
    LDAA #2
    STAA hit_cause
    JSR def_damage
    LDAA phase
    CMPA #1
    BNE def_enemy_return
    LDX def_enemy_ptr
def_enemy_escape_check:
    LDAA 0,X
    CMPA #5
    BEQ def_enemy_shots
    LDAA 2,X
    CMPA #12
    BHI def_enemy_shots
    CLR 0,X
    RTS

def_enemy_shots:
    LDAA 7,X
    LSRA
    LSRA
    LSRA
    LSRA
    ADDA #3
    STAA def_radius
    LDX #shots
    JSR def_shot_hit
    LDX def_enemy_ptr
    TST 0,X
    BEQ def_enemy_return
    LDX #shots + 4
    JSR def_shot_hit
def_enemy_return:
    RTS

; Move vertically toward the position captured during the visible warning.
def_move_to_aim:
    LDAA 3,X
    CMPA 5,X
    BEQ def_aim_done
    BCS def_aim_right
    SUBA 5,X
    CMPA #2
    BLS def_aim_snap
    LDAA 3,X
    SUBA #2
    STAA 3,X
    RTS
def_aim_right:
    LDAA 5,X
    SUBA 3,X
    CMPA #2
    BLS def_aim_snap
    LDAA 3,X
    ADDA #2
    STAA 3,X
    RTS
def_aim_snap:
    LDAA 5,X
    STAA 3,X
def_aim_done:
    RTS

; Boss: warning/fire, committed dive, retreat and vulnerable recovery.
def_boss_update:
    JSR def_boss_drop
    LDX def_enemy_ptr
    LDAA 4,X
    LSRA
    LSRA
    LSRA
    LSRA
    CMPA #3
    BLS def_boss_mode_bounded
    LDAA #3
def_boss_mode_bounded:
    CMPA boss_mode
    BEQ def_boss_pattern
    STAA boss_mode
    LDAA #1
    STAA hud_dirty
def_boss_pattern:
    TST sector
    BEQ def_boss_no_laser
    LDAA 4,X
    CMPA #28
    BCS def_boss_no_laser
    CMPA #33
    BCC def_boss_no_laser
    LDAA #15
    JSR def_audio_request
    LDAA ship_x
    ADDA #8
    CMPA 2,X
    BCC def_boss_no_laser
    LDAA ship_y
    SUBA 5,X
    BPL def_laser_dy
    NEGA
def_laser_dy:
    CMPA #5
    BHI def_boss_no_laser
    LDAA #4
    STAA hit_cause
    JSR def_damage
    LDX def_enemy_ptr
def_boss_no_laser:
    LDAA 4,X
    CMPA #8
    BEQ def_boss_lock
    CMPA #16
    BEQ def_boss_fire
    CMPA #20
    BNE def_boss_second
    LDAB 1,X
    CMPB #9
    BLS def_boss_fire
def_boss_second:
    CMPA #28
    BEQ def_boss_fire
    CMPA #34
    BEQ def_boss_lock
    CMPA #40
    BCS def_boss_done
    LDAB sector
    CMPB #1
    BNE def_boss_ram
    CMPA #64
    BCC def_boss_recover
    LDAB 3,X
    CMPA #52
    BCC def_boss_sweep_down
    DECB
    BRA def_boss_sweep_store
def_boss_sweep_down:
    INCB
def_boss_sweep_store:
    STAB 3,X
    RTS
def_boss_ram:
    CMPA #48
    BCC def_boss_retreat
    LDAB 2,X
    SUBB #15
    STAB 2,X
    JMP def_move_to_aim
def_boss_retreat:
    CMPA #64
    BCC def_boss_recover
    LDAB 2,X
    CMPB #152
    BCC def_boss_done
    ADDB #8
    CMPB #152
    BLS def_boss_return_x
    LDAB #152
def_boss_return_x:
    STAB 2,X
    RTS
def_boss_recover:
    LDAB 1,X
    CMPB #9
    BHI def_boss_slow
    CMPA #72
    BCS def_boss_done
    BRA def_boss_reset
def_boss_slow:
    CMPA #80
    BCS def_boss_done
def_boss_reset:
    CLR 4,X
    RTS
def_boss_lock:
    LDAA ship_x
    STAA 6,X
    LDAA ship_y
    STAA 5,X
def_boss_done:
    RTS
def_boss_fire:
    JMP def_launch_bolt

def_boss_drop:
    TST item
    BNE def_boss_drop_done
    LDAA 1,X
    CMPA #20
    BHI def_boss_drop_done
    LDAA boss_drop_mask
    BITA #1
    BNE def_boss_drop_shield
    ORAA #1
    STAA boss_drop_mask
    LDAA #1
    BRA def_boss_drop_position
def_boss_drop_shield:
    LDAA 1,X
    CMPA #10
    BHI def_boss_drop_done
    LDAA boss_drop_mask
    BITA #2
    BNE def_boss_drop_done
    ORAA #2
    STAA boss_drop_mask
    LDAA #2
def_boss_drop_position:
    STAA def_drop_kind
    LDAA 2,X
    STAA def_drop_x
    LDAA 3,X
    STAA def_drop_y
    JMP def_drop
def_boss_drop_done:
    RTS

; Shot is tested once per enemy generation, including piercing and the boss.
def_shot_hit:
    TST 0,X
    BNE def_branch_10
    JMP def_hit_return
def_branch_10:
    LDAA 3,X
    BITA def_enemy_bit
    BEQ def_branch_11
    JMP def_hit_return
def_branch_11:
    STX def_shot_ptr
    LDAA 1,X
    LDX def_enemy_ptr
    SUBA 2,X
    BPL def_hit_dx
    NEGA
def_hit_dx:
    CMPA def_radius
    BLS def_branch_12
    JMP def_hit_return
def_branch_12:
    LDX def_shot_ptr
    LDAA 2,X
    LDX def_enemy_ptr
    SUBA 3,X
    BPL def_hit_dy
    NEGA
def_hit_dy:
    CMPA def_radius
    BLS def_branch_13
    JMP def_hit_return
def_branch_13:
    LDAB #1
    LDX def_shot_ptr
    LDAA 3,X
    ORAA def_enemy_bit
    STAA 3,X
    BITA #128
    BEQ def_consume_shot
    INCB
    BITA #64
    BEQ def_hit_damage
    LDAB #5
    BRA def_hit_damage
def_consume_shot:
    CLR 0,X
def_hit_damage:
    LDX def_enemy_ptr
    LDAA 0,X
    CMPA #5
    BNE def_hit_hp
    LDAA 4,X
    CMPA #48
    BCC def_core_open
    LDAA sector
    CMPA #2
    BNE def_hit_hp
    CMPB #5
    BCC def_hit_hp
    LDAA #2
    JMP def_audio_request
def_core_open:
    INCB
    ; Recovery rewards attacking rather than moving out to collect an item.
def_hit_hp:
    STAB def_damage_amount
    LDAA #2
    STAA 9,X
    LDAA #1
    STAA hud_dirty
    LDAA 1,X
    SUBA def_damage_amount
    BLS def_killed
    STAA 1,X
    LDAA #2
    JMP def_audio_request
def_hit_return:
    RTS
def_killed:
    JSR def_explosion
    LDAA #3
    JSR def_audio_request
    CLR 1,X
    LDAA 0,X
    CMPA #3
    BEQ def_split
    CMPA #5
    BEQ def_boss_killed
    LDAB 8,X
    CMPA #4
    BNE def_kill_drop_position
    INC fragment_kills
    LDAA fragment_kills
    CMPA #2
    BNE def_kill_drop_position
    LDAB #1
def_kill_drop_position:
    LDAA 2,X
    STAA def_drop_x
    LDAA 3,X
    STAA def_drop_y
    CLR 0,X
    STAB def_drop_kind
    LDAA #1
    JSR def_add_score
    JSR def_drop
    RTS
def_boss_killed:
    CLR 0,X
    LDAA #20
    JSR def_add_score
    LDAA #8
    STAA phase
    LDAA #22
    STAA pause_ticks
    STAA fx_age
    LDAA #8
    JSR def_audio_request
    LDX #def_boss_down
    JMP ui_footer

def_split:
    LDAA #4
    JSR def_audio_request
    ; The parent has exclusive use of both slots. No chain splitting.
    LDAA #2
    JSR def_add_score
    CLR fragment_kills
    LDX #enemies
    JSR def_clear_enemy
    LDX #enemies + 10
    JSR def_clear_enemy
    LDAA #4
    STAA enemies
    STAA enemies + 10
    LDAA #1
    STAA enemies + 1
    STAA enemies + 11
    ; The final defeated fragment earns W, regardless of which one dies first.
    LDAA def_drop_x
    STAA enemies + 2
    STAA enemies + 12
    LDAA #22
    STAA enemies + 3
    LDAA #42
    STAA enemies + 13
    LDAA #40
    STAA enemies + 7
    STAA enemies + 17
    LDAA #$FF
    STAA enemies + 6
    LDAA #1
    STAA enemies + 16
    ; Old shots must not damage the fragments in the parent-death frame.
    CLR shots
    CLR shots + 4
    RTS

; Pool exhaustion skips a shot; no existing projectile is overwritten.
; Aim at a captured lane over 32 updates (about 3.2 seconds at normal speed).
; Signed Q4 velocities remain fixed after launch. No homing and no overwrites.
def_launch_bolt:
    LDX #bolts
    TST 0,X
    BEQ def_bolt_slot
    LDX #bolts + 8
    TST 0,X
    BEQ def_far_16
    JMP def_boss_done
def_far_16:
def_bolt_slot:
    STX def_bolt_ptr
    LDX def_enemy_ptr
    LDAA 2,X
    SUBA #8
    STAA def_bolt_origin_x
    LDAA 3,X
    STAA def_bolt_origin_y
    LDAA 5,X
    SUBA def_bolt_origin_y
    ASRA
    STAA def_tmp
    CLRA
    LDAB def_bolt_origin_x
    STD def_velocity
    CLRA
    LDAB 6,X
    SUBD def_velocity
    ASRA
    RORB
    LDX def_bolt_ptr
    STD 5,X
    LDAA def_tmp
    STAA 7,X
    LDAA #38
    STAA 0,X
    CLRA
    LDAB def_bolt_origin_x
    ASLD
    ASLD
    ASLD
    ASLD
    STD 1,X
    CLRA
    LDAB def_bolt_origin_y
    ASLD
    ASLD
    ASLD
    ASLD
    STD 3,X
    LDAA #13
    JMP def_audio_request

def_bolt_move:
    TST 0,X
    BEQ def_bolt_done
    DEC 0,X
    BEQ def_bolt_done
    LDD 1,X
    ADDD 5,X
    STD 1,X
    CMPA #12
    BCC def_bolt_remove
    LDAB 7,X
    CLRA
    TSTB
    BPL def_bolt_dy_positive
    DECA
def_bolt_dy_positive:
    ADDD 3,X
    STD 3,X
    SUBD #864
    BCC def_bolt_remove
    LDD 3,X
    SUBD #144
    BCS def_bolt_remove
    LDD 1,X
    LSRD
    LSRD
    LSRD
    LSRD
    SUBB ship_x
    BPL def_bolt_dx
    NEGB
def_bolt_dx:
    CMPB #5
    BHI def_bolt_done
    LDD 3,X
    LSRD
    LSRD
    LSRD
    LSRD
    SUBB ship_y
    BPL def_bolt_distance
    NEGB
def_bolt_distance:
    CMPB #4
    BHI def_bolt_done
    ; Keep the offending projectile visible throughout the hit pause.
    LDAA #1
    STAA hit_cause
    JMP def_damage
def_bolt_remove:
    CLR 0,X
def_bolt_done:
    RTS

def_damage:
    TST invulnerable
    BNE def_damage_done
    LDAA #12
    STAA invulnerable
    LDAA #1
    STAA hud_dirty
    TST shield
    BEQ def_damage_hull
    CLR shield
    LDAA #20
    STAA message_ticks
    LDX #def_shield_broken
    JSR ui_footer
    LDAA #6
    JMP def_audio_request
def_damage_hull:
    DEC hull
    LDAA weapon
    CMPA #1
    BLS def_hurt_pause
    DEC weapon
def_hurt_pause:
    LDAA #6
    STAA phase
    LDAA #8
    STAA pause_ticks
    LDAA #7
    TST hull
    BNE def_hurt_cue
    LDAA #7
    STAA phase
    LDAA #24
    STAA pause_ticks
    LDAA #8
def_hurt_cue:
    JSR def_audio_request
    LDX #def_hit_bolt
    LDAA hit_cause
    CMPA #1
    BEQ def_hurt_label
    LDX #def_hit_body
    CMPA #3
    BNE def_not_wall_hit
    LDX #def_hit_wall
def_not_wall_hit:
    CMPA #4
    BNE def_hurt_label
    LDX #def_hit_laser
def_hurt_label:
    JSR ui_footer
def_damage_done:
    RTS

def_drop:
    TST def_drop_kind
    BEQ def_drop_done
    TST item
    BNE def_drop_done
    LDAA def_drop_kind
    CMPA #3
    BNE def_drop_selected
    LDAA next_regular_drop
    EORA #3
    STAA next_regular_drop
def_drop_selected:
    STAA item
    LDAA def_drop_x
    STAA item + 1
    LDAA def_drop_y
    CMPA #46
    BLS def_drop_height
    LDAA #46
def_drop_height:
    STAA item + 2
    CLR item + 3
def_drop_done:
    RTS
def_item_update:
    TST item
    BEQ def_item_done
    INC item + 3
    LDAA item + 1
    SUBA #2
    STAA item + 1
    CMPA #12
    BLS def_item_expire
    SUBA ship_x
    BPL def_item_dx
    NEGA
def_item_dx:
    CMPA #8
    BHI def_item_done
    LDAA item + 2
    SUBA ship_y
    BPL def_item_distance
    NEGA
def_item_distance:
    CMPA #6
    BHI def_item_done
    LDAA item
    CMPA #2
    BEQ def_item_shield
    LDAA weapon
    CMPA #3
    BCC def_item_bonus
    INC weapon
    BRA def_item_bonus

def_item_shield:
    LDAA #1
    STAA shield
def_item_bonus:
    LDAA #5
    JSR def_audio_request
    LDAA #2
    JSR def_add_score
    LDAA #1
    STAA hud_dirty
def_item_expire:
    CLR item
def_item_done:
    RTS

def_add_score:
    LDAB ship_x
    CMPB #72
    BCS def_score_normal
    ASLA
def_score_normal:
    ADDA score
    CMPA #100
    BCS def_score_store
    SUBA #100
    INC score_hi
    LDAB score_hi
    CMPB #100
    BCS def_score_store
    LDAA #99
    STAA score_hi
def_score_store:
    STAA score
    LDAA #1
    STAA hud_dirty
    RTS

; Waves own the enemy slots: patrol -> splitter -> rush -> boss.
def_waves:
    LDAA stage
    CMPA #2
    BNE def_wave_regular
    JSR campaign_rush
    TST campaign_busy
    BNE def_wave_return_far
    TST item
    BNE def_wave_return_far
    INC stage
    JSR def_stage_header
    JMP def_start_boss
def_wave_return_far:
    RTS
def_wave_regular:
    LDAA stage
    CMPA #3
    BNE def_far_3
    JMP def_wave_boss
def_far_3:
    CMPA #1
    BNE def_far_4
    JMP def_wave_split
def_far_4:
    TST spawn_left
    BNE def_far_14
    JMP def_wave_empty
def_far_14:
    TST spawn_delay
    BEQ def_wave_spawn
    DEC spawn_delay
    RTS
def_wave_spawn:
    LDX #enemies
    TST 0,X
    BEQ def_spawn_slot
    LDX #enemies + 10
    TST 0,X
    BEQ def_far_5
    JMP def_wave_return
def_far_5:
def_spawn_slot:
    JSR def_clear_enemy
    LDAA spawn_serial
    BITA #1
    BEQ def_spawn_gunner
    LDAA #2
    BRA def_spawn_kind
def_spawn_gunner:
    LDAA #1
def_spawn_kind:
    LDAB sector
    BEQ def_spawn_original
    ADDA #5
def_spawn_original:
    STAA 0,X
    LDAA #1
    STAA 1,X
    STAA 6,X
    STX def_enemy_ptr
    LDAA spawn_serial
    ANDA #3
    TAB
    LDX #def_spawn_lanes
    ABX
    LDAA 0,X
    LDX def_enemy_ptr
    STAA 3,X
    LDAA #188
    STAA 2,X
    LDAA #128
    STAA 4,X
    LDAA #40
    STAA 7,X
    ; Alternate W/S when a free item slot is actually used, not at spawn time.
    LDAA #3
    STAA 8,X
    JSR def_new_generation
    INC spawn_serial
    DEC spawn_left
    LDAA #22
    TST stage
    BEQ def_spawn_delay
    LDAA #8
def_spawn_delay:
    STAA spawn_delay
    RTS
def_wave_empty:
    LDAA enemies
    ORAA enemies + 10
    BNE def_wave_return
    TST item
    BNE def_wave_return
    INC stage
    JSR def_stage_header
    LDAA stage
    CMPA #1
    BEQ def_start_split
    JMP def_start_boss
def_wave_split:
    TST gate_left
    BNE def_wave_return
    LDAA gates
    ORAA gates + 3
    BNE def_wave_return
    LDAA enemies
    ORAA enemies + 10
    BNE def_wave_return
    TST item
    BNE def_wave_return
    INC stage
    LDAA #18
    STAA spawn_left
    CLR spawn_delay
    JSR def_stage_header
    RTS
def_wave_return:
    RTS
def_wave_boss:
    RTS
def_start_split:
    JSR campaign_reset
    LDX #enemies
    JSR def_clear_enemy
    LDAA #3
    STAA 0,X
    STAA 1,X
    LDAA #188
    STAA 2,X
    LDAA #32
    STAA 3,X
    LDAA #64
    STAA 7,X
    LDAA #128
    STAA 4,X
    JMP def_new_generation
def_start_boss:
    ; Warning precedes the visible slide-in. No enemies or live bolts remain.
    LDAA #5
    STAA phase
    LDAA #24
    STAA pause_ticks
    CLR shots
    CLR shots + 4
    CLR bolts
    CLR bolts + 8
    JSR scene_clear
    LDX #def_warning_title
    LDD #framebuffer + 576 + 39
    JSR text
    LDX #def_warning_footer
    JSR ui_footer
    LDAA #9
    JMP def_audio_request
def_boss_enter:
    JSR scene_clear
    JSR def_redraw_header
    JSR def_footer
    LDX #enemies
    JSR def_clear_enemy
    LDAA #5
    STAA 0,X
    LDAB sector
    LDX #def_boss_hp
    ABX
    LDAA 0,X
    STAA enemies + 1
    LDX #enemies
    LDAA #188
    STAA 2,X
    LDAA #32
    STAA 3,X
    LDAA #64
    STAA 7,X
    LDAA #128
    STAA 4,X
    CLR boss_drop_mask
    CLR boss_mode
    LDAA #255
    STAA def_last_boss_mode
    JSR def_new_generation
    LDAA #1
    STAA phase
    STAA hud_dirty
    RTS

def_clear_enemy:
    STX def_clear_ptr
    LDAB #10
def_clear_record:
    CLR 0,X
    INX
    DECB
    BNE def_clear_record
    LDX def_clear_ptr
    RTS
def_new_generation:
    LDAA #$FE
    CPX #enemies
    BEQ def_generation_mask
    LDAA #$FD
def_generation_mask:
    ANDA shots + 3
    STAA shots + 3
    LDAA #$FE
    CPX #enemies
    BEQ def_generation_second
    LDAA #$FD
def_generation_second:
    ANDA shots + 7
    STAA shots + 7
    RTS

def_new:
    JSR def_audio_init
    JSR scene_clear
    LDX #def_state_begin
def_clear_state:
    CLR 0,X
    INX
    CPX #def_state_end
    BNE def_clear_state
    LDAA #1
    STAA phase
    STAA weapon
    STAA fire_enabled
    LDAA #2
    STAA next_regular_drop
    LDAA #3
    STAA hull
    LDAA #24
    STAA ship_x
    LDAA #32
    STAA ship_y
    LDAA #4
    STAA spawn_left
    JSR campaign_reset
    LDAA input_ticks
    STAA def_clock
    JSR def_redraw_header
def_sector_ready:
    LDAB sector
    ASLB
    LDX #def_sector_names
    ABX
    LDX 0,X
    JSR ui_footer
    LDAA #4
    STAA phase
    LDAA #14
    STAA pause_ticks
    LDAA #10
    JMP def_audio_request

def_redraw_header:
    LDAA #255
    STAA def_last_score
    STAA def_last_score_hi
    STAA def_last_hull
    STAA def_last_weapon
    STAA def_last_shield
    STAA def_last_boss_hp
    LDX #def_header
    LDD #framebuffer
    JSR text
    JMP def_stage_header

def_footer:
    TST message_ticks
    BNE def_footer_done
    LDX #def_auto_on
    TST front_zone
    BEQ def_footer_fire
    LDX #def_front_on
def_footer_fire:
    TST fire_enabled
    BNE def_footer_write
    LDX #def_auto_off
    LDAA charge
    CMPA #18
    BCS def_footer_write
    LDX #def_burst_ready
def_footer_write:
    JMP ui_footer
def_footer_done:
    RTS

def_stage_header:
    LDX #framebuffer + 108
    LDAB #84
    CLRA
def_header_clear:
    STAA 0,X
    INX
    DECB
    BNE def_header_clear
    LDAA stage
    ASLA
    LDX #def_stage_names
    TAB
    ABX
    LDX 0,X
    LDD #framebuffer + 132
    JSR text
    LDAA #47
    LDX #framebuffer + 108
    JSR glyph
    LDAA sector
    ADDA #49
    LDX #framebuffer + 114
    JSR glyph
    LDAA dirty_min
    CMPA #108
    BLS def_stage_mark_max
    LDAA #108
    STAA dirty_min
def_stage_mark_max:
    LDAA #191
    STAA dirty_max
    LDAA #1
    STAA dirty_pending
    STAA hud_dirty
    RTS

app_draw:
    LDAA phase
    CMPA #5
    BNE def_draw_not_warning
    JMP def_warning_draw
def_draw_not_warning:
    CMPA #4
    BCC def_world
    CMPA #1
    BEQ def_world
    LDAA #96
    STAA p3_x
    LDAA #38
    STAA p3_y
    LDAA #64
    STAA p3_scale
    LDAA def_tick
    ANDA #63
    STAA p3_yaw
    LDAA #4
    STAA p3_pitch
    LDX #p3_model_octa
    JMP p3_draw
def_world:
    TST hud_dirty
    BNE def_far_8
    JMP def_geometry
def_far_8:
    CLR hud_dirty
    LDAA score_hi
    CMPA def_last_score_hi
    BEQ def_hud_score_low
    STAA def_last_score_hi
    LDX #framebuffer + 12
    JSR ui_number
def_hud_score_low:
    LDAA score
    CMPA def_last_score
    BEQ def_hud_hull
    STAA def_last_score
    LDX #framebuffer + 24
    JSR ui_number
def_hud_hull:
    LDAA hull
    CMPA def_last_hull
    BEQ def_hud_weapon
    STAA def_last_hull
    LDX #framebuffer + 48
    JSR ui_number
def_hud_weapon:
    LDAA weapon
    CMPA def_last_weapon
    BEQ def_hud_shield
    STAA def_last_weapon
    LDX #framebuffer + 72
    JSR ui_number
def_hud_shield:
    LDAA shield
    CMPA def_last_shield
    BEQ def_hud_boss
    STAA def_last_shield
    LDX #framebuffer + 96
    JSR ui_number
def_hud_boss:
    LDAA stage
    CMPA #3
    BNE def_geometry
    LDAA enemies + 1
    CMPA def_last_boss_hp
    BEQ def_hud_boss_mode
    STAA def_last_boss_hp
    LDX #framebuffer + 144
    JSR ui_number
def_hud_boss_mode:
    LDAA boss_mode
    CMPA def_last_boss_mode
    BEQ def_geometry
    STAA def_last_boss_mode
    ASLA
    STAA def_tmp
    LDAA sector
    ASLA
    ASLA
    ASLA
    ADDA def_tmp
    TAB
    LDX #def_boss_names
    ABX
    LDX 0,X
    LDD #framebuffer + 162
    JSR text
    LDAA dirty_min
    CMPA #162
    BLS def_boss_mark_max
    LDAA #162
    STAA dirty_min
def_boss_mark_max:
    LDAA #185
    STAA dirty_max
    LDAA #1
    STAA dirty_pending
def_geometry:
    JSR campaign_draw
    ; Draw the farther enemy first if their screen-space silhouettes overlap.
    LDAA enemies + 3
    CMPA enemies + 13
    BHI def_draw_reverse
    LDX #enemies
    JSR def_enemy_draw
    LDX #enemies + 10
    JSR def_enemy_draw
    BRA def_draw_ship
def_draw_reverse:
    LDX #enemies + 10
    JSR def_enemy_draw
    LDX #enemies
    JSR def_enemy_draw
def_draw_ship:
    LDAA phase
    CMPA #6
    BCC def_ship_visible
    TST invulnerable
    BEQ def_ship_visible
    LDAA def_tick
    BITA #1
    BNE def_draw_particles
def_ship_visible:
    ; Fixed side profile: seven cached silhouette spans avoid repeating
    ; identical matrix/face work for the player. Enemies remain live 3D meshes.
    JSR def_ship_draw
    TST shield
    BEQ def_draw_particles
    LDAA ship_x
    SUBA #7
    STAA def_line
    STAA def_line + 2
    LDAA ship_y
    SUBA #5
    STAA def_line + 1
    ADDA #10
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
def_draw_particles:
    LDX #shots
    JSR def_shot_draw
    LDX #shots + 4
    JSR def_shot_draw
    LDX #bolts
    JSR def_bolt_draw
    LDX #bolts + 8
    JSR def_bolt_draw
    TST item
    BEQ def_draw_effects
    JSR def_item_draw
def_draw_effects:
    JMP def_effect_draw
def_draw_return:
    RTS

def_enemy_draw:
    TST 0,X
    BEQ def_draw_return
    STX def_draw_ptr
    CLR p3_mode
    TST 9,X
    BEQ def_enemy_pose
    INC p3_mode
def_enemy_pose:
    LDAA 2,X
    STAA p3_x
    LDAA 3,X
    STAA p3_y
    LDAA 7,X
    STAA p3_scale
    LDAA 4,X
    BPL def_enemy_full_size
    LDAA #192
    SUBA 2,X
    LDAB 0,X
    CMPB #5
    BNE def_entry_scale
    ASLA
def_entry_scale:
    CMPA p3_scale
    BCC def_enemy_full_size
    STAA p3_scale
def_enemy_full_size:
    LDAA def_tick
    ADDA 2,X
    ANDA #63
    STAA p3_yaw
    LDAA #8
    STAA p3_pitch
    LDAA 0,X
    CMPA #5
    BNE def_enemy_diver_pose
    LDAA boss_mode
    CMPA #2
    BNE def_enemy_open_pose
    LDAA #24
    STAA p3_pitch
    CLR p3_yaw
    BRA def_mesh_kind
def_enemy_open_pose:
    CMPA #3
    BNE def_mesh_kind
    CLR p3_pitch
    LDAA #8
    STAA p3_yaw
    BRA def_mesh_kind
def_enemy_diver_pose:
    LDAA 0,X
    CMPA #2
    BNE def_mesh_kind
    LDAA 4,X
    CMPA #12
    BCS def_mesh_kind
    ; The diver leans forward during the warning and committed charge.
    LDAA #24
    STAA p3_pitch
    CLR p3_yaw
def_mesh_kind:
    LDAA 0,X
    CMPA #3
    BEQ def_mesh_octa
    CMPA #5
    BEQ def_boss_mesh
    LDX #p3_model_tetra
    BRA def_mesh_draw
def_boss_mesh:
    LDAA sector
    BEQ def_boss_tetra
    CMPA #1
    BNE def_mesh_octa
    LDX #p3_model_prism
    BRA def_mesh_draw
def_boss_tetra:
    LDX #p3_model_tetra
    BRA def_mesh_draw
def_mesh_octa:
    LDX #p3_model_octa
def_mesh_draw:
    JSR p3_draw
    CLR p3_mode
    ; Short bar above the enemy is a persistent aim/charge warning.
    LDX def_draw_ptr
    LDAA 4,X
    BPL def_warning_active
    RTS
def_warning_active:
    LDAB 0,X
    CMPB #4
    BNE def_far_9
    JMP def_draw_return
def_far_9:
    CMPB #5
    BEQ def_boss_warning
    CMPA #8
    BCC def_far_10
    JMP def_draw_return
def_far_10:
    CMPA #20
    BCS def_warning
    CMPB #3
    BEQ def_parent_warning
    CMPB #2
    BNE def_far_11
    JMP def_draw_return
def_far_11:
    CMPA #40
    BCC def_far_12
    JMP def_draw_return
def_far_12:
    CMPA #52
    BCS def_warning
    RTS
def_parent_warning:
    CMPA #24
    BCS def_warning
    RTS
def_boss_warning:
    CMPA #8
    BCC def_far_13
    JMP def_draw_return
def_far_13:
    CMPA #16
    BCS def_warning
    CMPA #32
    BCC def_far_6
    JMP def_draw_return
def_far_6:
    CMPA #40
    BCS def_far_7
    JMP def_draw_return
def_far_7:
def_warning:
    LDAA 2,X
    SUBA #5
    STAA def_line
    ADDA #10
    STAA def_line + 2
    LDAA 3,X
    SUBA #7
    CMPA #8
    BCC def_warning_y
    LDAA #8
def_warning_y:
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JMP p3_line

def_shot_draw:
    TST 0,X
    BEQ def_particle_return
    LDAA 1,X
    STAA def_line
    ADDA #4
    STAA def_line + 2
    LDAA 2,X
    STAA def_line + 1
    STAA def_line + 3
    LDAA 3,X
    STAA def_tmp
    LDX #def_line
    JSR p3_line
    LDAA def_tmp
    BITA #64
    BEQ def_particle_return
    DEC def_line + 1
    DEC def_line + 3
    LDX #def_line
    JSR p3_line
    LDAA def_line + 1
    ADDA #2
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JMP p3_line

def_bolt_draw:
    TST 0,X
    BEQ def_particle_return
    LDD 1,X
    LSRD
    LSRD
    LSRD
    LSRD
    STAB def_line
    INCB
    CMPB #192
    BCS def_bolt_end
    DECB
def_bolt_end:
    STAB def_line + 2
    LDD 3,X
    LSRD
    LSRD
    LSRD
    LSRD
    STAB def_line + 1
    STAB def_line + 3
    LDX #def_line
    JSR p3_line
    INC def_line + 1
    INC def_line + 3
    LDX #def_line
    JMP p3_line
def_particle_return:
    RTS

; W uses three vertical strokes joined at the bottom. Shield is a plus sign.
; Axis-aligned spans share the renderer's erase/dirty tracking.
def_item_draw:
    LDAA item
    CMPA #1
    BEQ def_item_w
    LDAA item + 1
    SUBA #3
    STAA def_line
    ADDA #6
    STAA def_line + 2
    LDAA item + 2
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
    LDAA item + 1
    STAA def_line
    STAA def_line + 2
    LDAA item + 2
    SUBA #3
    STAA def_line + 1
    ADDA #6
    STAA def_line + 3
    LDX #def_line
    JMP p3_line
def_item_w:
    LDAA item + 1
    SUBA #3
    STAA def_line
    ADDA #6
    STAA def_line + 2
    LDAA item + 2
    ADDA #3
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
    LDAA item + 1
    STAA def_line
    STAA def_line + 2
    LDAA item + 2
    SUBA #2
    STAA def_line + 1
    LDX #def_line
    JSR p3_line
    LDAA item + 1
    SUBA #3
    STAA def_line
    STAA def_line + 2
    LDAA item + 2
    SUBA #3
    STAA def_line + 1
    ADDA #6
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
    LDAA item + 1
    ADDA #3
    STAA def_line
    STAA def_line + 2
    LDAA item + 2
    SUBA #3
    STAA def_line + 1
    ADDA #6
    STAA def_line + 3
    LDX #def_line
    JMP p3_line

def_ship_draw:
    CLR def_ship_row
def_ship_span:
    LDAB def_ship_row
    LDX #def_ship_widths
    ABX
    LDAA 0,X
    ADDA ship_x
    SUBA #5
    STAA def_line + 2
    LDAA ship_x
    SUBA #5
    STAA def_line
    LDAA ship_y
    SUBA #3
    ADDA def_ship_row
    STAA def_line + 1
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
    INC def_ship_row
    LDAA def_ship_row
    CMPA #7
    BCS def_ship_span
    ; Short alternating exhaust provides a motion cue at almost no LCD cost.
    LDAA ship_x
    SUBA #10
    STAA def_line
    ADDA #3
    STAA def_line + 2
    LDAA ship_y
    STAA def_line + 1
    STAA def_line + 3
    LDAA def_tick
    BITA #1
    BEQ def_ship_exhaust
    INC def_line
def_ship_exhaust:
    LDX #def_line
    JMP p3_line

; Presentation phases: 4 ready,5 warning,6 damage,7 death,8 boss explosion.
def_pause_update:
    DEC pause_ticks
    BEQ def_branch_14
    JMP def_pause_done
def_branch_14:
    LDAA phase
    CMPA #4
    BNE def_branch_15
    JMP def_ready_done
def_branch_15:
    CMPA #5
    BNE def_pause_not_warning
    JMP def_boss_enter
def_pause_not_warning:
    CMPA #6
    BNE def_branch_16
    JMP def_resume
def_branch_16:
    CMPA #7
    BNE def_clear_result
    LDAA #3
    STAA phase
    LDAA #12
    JSR def_audio_request
    JSR scene_clear
    LDX #def_lose
    BRA def_result
def_clear_result:
    LDAA sector
    CMPA #2
    BCC def_campaign_clear
    INC sector
    LDAA hull
    CMPA #3
    BCC def_sector_keep_hull
    INC hull
def_sector_keep_hull:
    CLR stage
    CLR enemies
    CLR enemies + 10
    CLR shots
    CLR shots + 4
    CLR bolts
    CLR bolts + 8
    CLR item
    CLR fx_age
    CLR invulnerable
    CLR spawn_serial
    CLR spawn_delay
    CLR message_ticks
    CLR front_zone
    CLR charge
    CLR burst_pending
    JSR campaign_reset
    LDAA #4
    STAA spawn_left
    LDAA #24
    STAA ship_x
    LDAA #32
    STAA ship_y
    JSR scene_clear
    JSR def_redraw_header
    JMP def_sector_ready
def_campaign_clear:
    LDAA #2
    STAA phase
    LDAA #11
    JSR def_audio_request
    JSR scene_clear
    LDX #def_win
def_result:
    LDD #framebuffer + 66
    JSR text
    LDX #def_score_label
    LDD #framebuffer
    JSR text
    LDAA score_hi
    LDX #framebuffer + 12
    JSR ui_number
    LDAA score
    LDX #framebuffer + 24
    JSR ui_number
    LDX #def_retry
    JMP ui_footer
def_ready_done:
    LDAA #1
    STAA phase
    JSR def_footer
    JMP def_waves
def_resume:
    LDAA #1
    STAA phase
    LDAA #12
    STAA invulnerable
    CLR bolts
    CLR bolts + 8
    JMP def_footer
def_pause_done:
    RTS

; XOR the actual framebuffer, so LCD verification still compares identical data.
; At most 192 bytes between polls; the single framebuffer is restored next update.
def_invert:
    LDX #framebuffer
def_invert_band:
    LDAB #192
def_invert_byte:
    COM 0,X
    INX
    DECB
    BNE def_invert_byte
    PSHX
    JSR input_poll
    PULX
    CPX #framebuffer + 1536
    BNE def_invert_band
    JMP dirty_all

def_explosion:
    LDAA 2,X
    STAA fx_x
    STAA def_drop_x
    LDAA 3,X
    STAA fx_y
    STAA def_drop_y
    LDAA #6
    STAA fx_age
    RTS

def_effect_draw:
    TST fx_age
    BEQ def_hit_effect
    LDAA fx_age
    COMA
    ANDA #7
    ADDA #2
    LDAB phase
    CMPB #8
    BNE def_burst_radius
    LDAA pause_ticks
    COMA
    ANDA #7
    ADDA #2
def_burst_radius:
    STAA def_radius
    LDAA fx_x
    STAA def_drop_x
    LDAA fx_y
    STAA def_drop_y
    JSR def_cross

def_hit_effect:
    LDAA phase
    CMPA #6
    BCS def_effect_done
    CMPA #7
    BHI def_effect_done
    LDAA ship_x
    STAA def_drop_x
    LDAA ship_y
    STAA def_drop_y
    LDAA #7
    STAA def_radius
    JSR def_cross
    LDAA pause_ticks
    LDAB phase
    CMPB #7
    BEQ def_death_flash
    CMPA #6
    BCS def_effect_done
    BRA def_flash
def_death_flash:
    CMPA #19
    BCS def_effect_done
def_flash:
    JSR def_invert
    LDAA #1
    STAA screen_inverted
def_effect_done:
    RTS

; Two diagonal rays mark the precise impact/explosion position.
def_cross:
    LDAA def_drop_x
    SUBA def_radius
    STAA def_line
    LDAA def_drop_x
    ADDA def_radius
    STAA def_line + 2
    LDAA def_drop_y
    SUBA def_radius
    STAA def_line + 1
    LDAA def_drop_y
    ADDA def_radius
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
    LDAA def_line + 1
    LDAB def_line + 3
    STAA def_line + 3
    STAB def_line + 1
    LDX #def_line
    JMP p3_line

def_warning_draw:
    ; Side shutters converge in a cleared screen, without erasing warning text.
    LDAA pause_ticks
    ANDA #7
    ASLA
    ADDA #12
    STAA def_line
    STAA def_line + 2
    LDAA #16
    STAA def_line + 1
    LDAA #46
    STAA def_line + 3
    LDX #def_line
    JSR p3_line
    LDAA #191
    SUBA def_line
    STAA def_line
    STAA def_line + 2
    LDX #def_line
    JSR p3_line
    ; Both shutters share a dirty span: redraw the text they enclose after erase.
    LDX #def_warning_title
    LDD #framebuffer + 576 + 39
    JMP text

.section .bss, bss
def_state_begin:
phase: .space 1
stage: .space 1
sector: .space 1
score_hi: .space 1
charge: .space 1
front_zone: .space 1
burst_pending: .space 1
message_ticks: .space 1
def_last_score_hi: .space 1
score: .space 1
hull: .space 1
weapon: .space 1
shield: .space 1
ship_x: .space 1
ship_y: .space 1
pause_ticks: .space 1
hit_cause: .space 1
screen_inverted: .space 1
fx_age: .space 1
fx_x: .space 1
fx_y: .space 1
def_velocity: .space 2
fire_enabled: .space 1
invulnerable: .space 1
enemies: .space 20
shots: .space 8
bolts: .space 16
item: .space 4
spawn_left: .space 1
spawn_delay: .space 1
spawn_serial: .space 1
def_tick: .space 1
def_clock: .space 1
def_cooldown: .space 1
def_enemy_bit: .space 1
def_enemy_ptr: .space 2
def_shot_ptr: .space 2
def_bolt_ptr: .space 2
def_draw_ptr: .space 2
def_clear_ptr: .space 2
def_radius: .space 1
def_damage_amount: .space 1
def_drop_kind: .space 1
def_drop_x: .space 1
def_drop_y: .space 1
def_bolt_origin_x: .space 1
def_bolt_origin_y: .space 1
def_tmp: .space 1
def_line: .space 4
boss_mode: .space 1
boss_drop_mask: .space 1
next_regular_drop: .space 1
fragment_kills: .space 1
def_last_score: .space 1
def_last_hull: .space 1
def_last_weapon: .space 1
def_last_shield: .space 1
def_last_boss_hp: .space 1
def_last_boss_mode: .space 1
def_ship_row: .space 1
def_state_end:
.section .data, data
; POLY DEFENDER
def_title: .byte 80,79,76,89,32,68,69,70,69,78,68,69,82,0
; COLLECT W/+  SURVIVE THE BOSS
def_intro: .byte 51,32,83,69,67,84,79,82,83,32,45,32,66,82,69,65,75,32,84,72,69,32,67,79,82,69,0
; 8/2 MOVE  SPACE/RETURN START
def_start: .byte 56,47,50,47,52,47,54,32,77,79,86,69,32,32,82,69,84,85,82,78,32,83,84,65,82,84,0
; AUTO ON  8/2 MOVE  SPACE TOGGLE
def_auto_on: .byte 65,85,84,79,32,83,80,65,67,69,58,67,72,65,82,71,69,32,83,67,79,82,69,32,88,49,0
; AUTO OFF 8/2 MOVE  SPACE TOGGLE
def_auto_off: .byte 67,72,65,82,71,73,78,71,32,45,32,83,80,65,67,69,32,84,79,32,70,73,82,69,0
; AREA CLEAR
def_win: .byte 77,73,83,83,73,79,78,32,67,79,77,80,76,69,84,69,0
; GAME OVER
def_lose: .byte 71,65,77,69,32,79,86,69,82,0
; SCORE
def_score_label: .byte 83,67,0
; SPACE/RETURN RETRY  BREAK EXIT
def_retry: .byte 83,80,65,67,69,47,82,69,84,85,82,78,32,82,69,84,82,89,32,32,66,82,69,65,75,32,69,88,73,84,0
; SC00 H03 W01 S00
def_header: .byte 83,67,48,48,48,48,32,72,48,51,32,87,48,49,32,83,48,48,0
; PATROL
def_stage_patrol: .byte 83,75,89,0
; SPLIT
def_stage_split: .byte 77,65,90,69,0
; RUSH
def_stage_rush: .byte 82,85,83,72,0
; BOSS
def_stage_boss: .byte 66,0
def_stage_names: .word def_stage_patrol,def_stage_split,def_stage_rush,def_stage_boss
def_spawn_lanes: .byte 22,42,30,38
; W:POWER +:SHIELD HIT:DOWN
def_rules: .byte 87,58,80,79,87,69,82,32,43,58,83,72,73,69,76,68,32,83,80,65,67,69,58,66,85,82,83,84,0
; AIM (padded)
def_boss_aim: .byte 65,73,77,32,0
; FIRE
def_boss_fire_name: .byte 70,73,82,69,0
; DIVE
def_boss_dive: .byte 68,73,86,69,0
; OPEN
def_boss_open: .byte 79,80,69,78,0
def_boss_names: .word def_boss_aim,def_boss_fire_name,def_boss_dive,def_boss_open
    .word def_boss_aim,def_beam_name,def_sweep_name,def_boss_open
    .word def_armor_name,def_beam_name,def_boss_dive,def_boss_open
def_beam_name: .byte 66,69,65,77,0
def_sweep_name: .byte 83,87,69,80,0
def_armor_name: .byte 65,82,77,82,0
; READY - 8/2 MOVE, AUTO FIRE
def_ready: .byte 82,69,65,68,89,32,45,32,56,47,50,47,52,47,54,32,77,79,86,69,0
; ! BOSS APPROACHING !
def_warning_title: .byte 33,32,66,79,83,83,32,65,80,80,82,79,65,67,72,73,78,71,32,33,0
; WATCH THE AIM - EVADE THEN FIRE
def_warning_footer: .byte 87,65,84,67,72,32,84,72,69,32,65,73,77,32,45,32,69,86,65,68,69,32,84,72,69,78,32,70,73,82,69,0
; HIT BY BOLT - IMPACT MARKED X
def_hit_bolt: .byte 72,73,84,32,66,89,32,66,79,76,84,32,45,32,73,77,80,65,67,84,32,77,65,82,75,69,68,32,88,0
; HIT BY ENEMY - IMPACT MARKED X
def_hit_body: .byte 72,73,84,32,66,89,32,69,78,69,77,89,32,45,32,73,77,80,65,67,84,32,77,65,82,75,69,68,32,88,0
; BOSS DESTROYED
def_boss_down: .byte 66,79,83,83,32,68,69,83,84,82,79,89,69,68,0
def_ship_widths: .byte 1,4,8,13,8,4,1

def_shield_broken: .byte 83,72,73,69,76,68,32,66,82,79,75,69,78,32,45,32,69,86,65,68,69,33,0
def_burst_ready: .byte 66,85,82,83,84,32,82,69,65,68,89,32,45,32,83,80,65,67,69,32,84,79,32,70,73,82,69,0
def_hit_wall: .byte 72,73,84,32,66,89,32,87,65,76,76,32,45,32,73,77,80,65,67,84,32,77,65,82,75,69,68,32,88,0
def_hit_laser: .byte 72,73,84,32,66,89,32,76,65,83,69,82,32,45,32,73,77,80,65,67,84,32,77,65,82,75,69,68,32,88,0
def_sector_1: .byte 83,69,67,84,79,82,32,49,32,45,32,79,85,84,83,75,73,82,84,83,0
def_sector_2: .byte 83,69,67,84,79,82,32,50,32,45,32,67,82,89,83,84,65,76,32,77,65,90,69,0
def_sector_3: .byte 83,69,67,84,79,82,32,51,32,45,32,82,69,65,67,84,79,82,32,67,79,82,69,0
def_sector_names: .word def_sector_1,def_sector_2,def_sector_3
def_boss_hp: .byte 20,24,30

def_front_on: .byte 65,85,84,79,32,83,80,65,67,69,58,67,72,65,82,71,69,32,83,67,79,82,69,32,88,50,0
