; SPDX-License-Identifier: MIT
; Logical card rules settle once. Presentation reveals their values one point
; at a time, using the same emulated input clock and LCD transfer accounting.
.global deck_active
.global deck_hp
.global deck_enemy_hp
.global deck_block
.global deck_enemy_block
.global deck_effect_frame
.global deck_intro_frame
.global deck_defeat_frame
.global deck_message
.global deck_flash_mask
.global deck_card_sounds
.global deck_reward_view
.extern sound_play
.section .text, code
deck_capture:
    LDAA hp
    STAA deck_hp
    LDAA enemy_hp
    STAA deck_enemy_hp
    LDAA block
    STAA deck_block
    LDAA enemy_block
    STAA deck_enemy_block
    CLR deck_reward_view
    RTS
deck_intro_begin:
    JSR deck_capture
    LDAA #3
    STAA deck_active
    CLR deck_message
    CLR deck_sound_done
    CLR deck_flash_mask
    CLR deck_flash
    RTS
deck_queue:
    STAA deck_active
    CLR deck_sound_done
    CLR deck_flash
    CLR deck_flash_mask
    LDAA phase
    STAA deck_next_phase
    LDAA #2
    STAA phase
    LDAA #3
    STAA deck_message
    LDAA deck_active
    CMPA #1
    BNE deck_queue_enemy
    LDAA card_id
    ADDA #10
    STAA deck_message
    TST deck_enemy_block
    BEQ deck_queue_ready
    LDAA enemy_block
    CMPA deck_enemy_block
    BEQ deck_queue_ready
    LDAA #2
    STAA deck_message
    LDAA enemy_hp
    CMPA deck_enemy_hp
    BNE deck_queue_ready
    DEC deck_message
    BRA deck_queue_ready
deck_queue_enemy:
    LDAA deck_intent
    CMPA #1
    BNE deck_queue_attack
    LDAA #5
    STAA deck_message
    BRA deck_queue_ready
deck_queue_attack:
    LDAA hp
    CMPA deck_hp
    BNE deck_queue_ready
    LDAA #4
    STAA deck_message
deck_queue_ready:
    JMP card_changed
; A bounded animation update owns its dirty-byte count, including a paused menu.
deck_animation_update:
    CLR deck_frame_bytes
    CLR deck_frame_bytes + 1
    CLR deck_paused
    LDAA deck_active
    CMPA #3
    BNE deck_effect_begin
    JSR deck_intro
    TST deck_paused
    BNE deck_animation_pause
    CLR deck_active
    JSR paint_clear
    BRA deck_animation_complete
deck_effect_begin:
    TST deck_sound_done
    BNE deck_effect_loop
    INC deck_sound_done
    JSR deck_card_sound
deck_effect_loop:
    JSR deck_step_values
    TST deck_flash_mask
    BEQ deck_effect_settled
    INC deck_flash
    JSR deck_render
    JSR deck_flush
deck_effect_frame:
    LDX #120
    LDD #3
    JSR sound_tone
    LDAB #3
    JSR deck_wait
    TST deck_paused
    BNE deck_animation_pause
    BRA deck_effect_loop
deck_effect_settled:
    CLR deck_flash
    JSR deck_render
    JSR deck_flush
    LDAB #8
    JSR deck_wait
    TST deck_paused
    BNE deck_animation_pause
    CLR deck_active
    LDAA deck_next_phase
    STAA phase
    TST battle_mode
    BEQ deck_animation_complete
    JSR deck_victory
    LDAA #1
    STAA deck_reward_view
    LDAA #7
    STAA deck_message
    JSR paint_clear
deck_animation_complete:
    CLR redraw
    JSR render_play_result
    BRA deck_animation_done
deck_animation_pause:
    LDAA #3
    STAA phase
    CLR menu_choice
    JSR input_gate
    JSR draw_menu
deck_animation_done:
    JSR deck_flush
    JSR input_gate
    JSR input_poll
    LDD deck_frame_bytes
    STD dirty_bytes
    PULX
    JMP frame_ready
; Shields settle first, then the corresponding HP. Only one number moves per cue.
deck_step_values:
    CLR deck_flash_mask
    LDX #deck_enemy_block
    LDAB enemy_block
    LDAA #4
    JSR deck_step_one
    TST deck_flash_mask
    BNE deck_step_done
    LDX #deck_enemy_hp
    LDAB enemy_hp
    LDAA #1
    JSR deck_step_one
    TST deck_flash_mask
    BNE deck_step_done
    LDX #deck_block
    LDAB block
    LDAA #8
    JSR deck_step_one
    TST deck_flash_mask
    BNE deck_step_done
    LDX #deck_hp
    LDAB hp
    LDAA #2
    JSR deck_step_one
deck_step_done:
    RTS
deck_step_one:
    STAA deck_step_bit
    LDAA 0,X
    CBA
    BEQ deck_step_done
    BHI deck_step_decrease
    INCA
    BRA deck_step_store
deck_step_decrease:
    DECA
deck_step_store:
    STAA 0,X
    LDAA deck_step_bit
    STAA deck_flash_mask
    RTS
; Poll throughout pauses and tones. RETURN can suspend the per-point animation.
deck_wait:
    STAB deck_wait_ticks
    LDAA input_ticks
    STAA deck_clock
deck_wait_loop:
    JSR input_poll
    JSR input_take
    LDAA input_event
    BITA #32
    BEQ deck_wait_time
    LDAA #1
    STAA deck_paused
    RTS
deck_wait_time:
    LDAA input_ticks
    SUBA deck_clock
    CMPA deck_wait_ticks
    BCS deck_wait_loop
    RTS
deck_flush:
    CLR dirty_bytes
    CLR dirty_bytes + 1
    TST dirty_pending
    BEQ deck_flush_done
    JSR dirty_begin
deck_flush_loop:
    JSR dirty_next
    BEQ deck_flush_done
    JSR input_poll
    BRA deck_flush_loop
deck_flush_done:
    LDD dirty_bytes
    ADDD deck_frame_bytes
    STD deck_frame_bytes
    RTS
deck_card_sound:
    LDX #deck_enemy_sound
    LDAA deck_active
    CMPA #1
    BNE deck_sound_play
    LDAA card_id
    LDAB #12
    MUL
    ADDD #deck_card_sounds
    XGDX
deck_sound_play:
    JMP sound_play
deck_intro:
    JSR deck_flush
deck_intro_frame:
    LDX #deck_intro_sound
    JSR sound_play
    LDAB #48
    JMP deck_wait
deck_victory:
    JSR paint_clear
    LDX #deck_result_art
    JSR deck_backdrop
    LDX #deck_victory_label
    LDAA #54
    LDAB #2
    JSR paint_text
    LDX #deck_next_label
    LDAA #40
    LDAB #4
    JSR hud_play_text
    JSR deck_flush
deck_defeat_frame:
    LDX #deck_victory_sound
    JSR sound_play
    LDAB #24
    JMP deck_wait
deck_render:
    LDAA deck_active
    CMPA #3
    BEQ deck_render_intro
    LDAA phase
    CMPA #4
    BNE deck_render_battle
    JMP deck_render_clear
deck_render_battle:
    LDAA deck_message
    PSHA
    LDAA deck_active
    CMPA #2
    BNE deck_render_status
    LDAA deck_flash_mask
    BITA #1
    BEQ deck_render_status
    LDAA #6
    STAA deck_message
deck_render_status:
    TST hud_ready
    BNE deck_render_fields
    JSR hud_begin
    LDX #deck_battle_art
    JSR deck_backdrop
deck_render_fields:
    LDX #hud_field_1
    LDAB #2
    JSR deck_field_flash
    LDX #hud_field_4
    LDAB #1
    JSR deck_field_flash
    LDX #hud_field_2
    LDAB #8
    JSR deck_field_flash
    LDX #hud_field_5
    LDAB #4
    JSR deck_field_flash
    JSR visual_hud
    PULA
    STAA deck_message
    LDAA #1
    STAA deck_portrait_band
    JMP deck_portrait
; Updating a field restores its normal pixels before applying a new flash.
deck_field_flash:
    CLR 4,X
    LDAA deck_flash
    BITA #1
    BEQ deck_field_refresh
    BITB deck_flash_mask
    BEQ deck_field_refresh
    LDAA #128
    STAA 4,X
deck_field_refresh:
    LDX 6,X
    CLR 0,X
    RTS
deck_render_intro:
    LDX #deck_arrival_art
    JSR deck_backdrop
    LDAA battle
    LDAB #11
    MUL
    ADDD #boss_names
    XGDX
    LDAA #104
    LDAB #2
    JSR paint_text
    LDAA battle
    LDAB #38
    MUL
    ADDD #boss_quotes
    STD deck_quote_ptr
    XGDX
    LDAA #104
    LDAB #4
    JSR hud_play_text
    LDD deck_quote_ptr
    ADDD #19
    XGDX
    LDAA #104
    LDAB #5
    JSR hud_play_text
    JMP deck_portrait_large
deck_render_clear:
    JSR paint_clear
    LDX #deck_result_art
    JSR deck_backdrop
    LDX #deck_clear_label
    LDAA #64
    LDAB #1
    JSR hud_play_text
    LDX #result_win
    LDAA #78
    LDAB #3
    JSR paint_text
    LDX #deck_clear_detail
    LDAA #50
    LDAB #6
    JMP hud_play_text
deck_portrait:
    LDAA battle
    LDAB #144
    MUL
    ADDD #boss_portraits
    STD deck_portrait_ptr
    LDAA #3
    STAA deck_portrait_rows
deck_portrait_loop:
    LDAA #2
    STAA paint_x
    LDAA deck_portrait_band
    STAA paint_band
    JSR paint_address
    LDD deck_portrait_ptr
    STD paint_source
    ADDD #48
    STD deck_portrait_ptr
    CLR paint_id
    LDAA deck_flash
    BITA #1
    BEQ deck_portrait_paint
    LDAA deck_flash_mask
    BITA #1
    BEQ deck_portrait_paint
    LDAA #128
    STAA paint_id
deck_portrait_paint:
    LDAA #48
    STAA paint_count
    JSR paint_blit
    INC deck_portrait_band
    DEC deck_portrait_rows
    BNE deck_portrait_loop
    RTS
 ; Double both axes for a 96x48 arrival portrait, without storing a second image.
deck_portrait_large:
    LDAA battle
    LDAB #144
    MUL
    ADDD #boss_portraits
    STD deck_portrait_ptr
    CLR deck_large_row
deck_large_band:
    LDAA deck_large_row
    LSRA
    LDAB #48
    MUL
    ADDD deck_portrait_ptr
    STD paint_source
    LDAA deck_large_row
    INCA
    STAA paint_band
    CLR paint_x
    JSR paint_address
    LDAA #48
    STAA paint_count
deck_large_column:
    LDX paint_source
    LDAA 0,X
    INX
    STX paint_source
    LDAB deck_large_row
    BITB #1
    BEQ deck_large_low
    LSRA
    LSRA
    LSRA
    LSRA
deck_large_low:
    ANDA #15
    TAB
    LDX #deck_double_bits
    ABX
    LDAA 0,X
    LDX paint_dest
    STAA 0,X
    STAA 1,X
    INX
    INX
    STX paint_dest
    DEC paint_count
    BNE deck_large_column
    JSR input_poll
    INC deck_large_row
    LDAA deck_large_row
    CMPA #6
    BCS deck_large_band
    JMP dirty_all
.section .bss, bss
deck_active: .space 1
deck_hp: .space 1
deck_enemy_hp: .space 1
deck_block: .space 1
deck_enemy_block: .space 1
deck_reward_view: .space 1
deck_message: .space 1
deck_next_phase: .space 1
deck_intent: .space 1
deck_sound_done: .space 1
deck_flash: .space 1
deck_flash_mask: .space 1
deck_step_bit: .space 1
deck_paused: .space 1
deck_frame_bytes: .space 2
deck_clock: .space 1
deck_wait_ticks: .space 1
deck_portrait_band: .space 1
deck_portrait_ptr: .space 2
deck_portrait_rows: .space 1
deck_quote_ptr: .space 2
deck_large_row: .space 1
.section .data, data
deck_double_bits: .byte 0,3,12,15,48,51,60,63,192,195,204,207,240,243,252,255
deck_victory_label: .byte 66,79,83,83,32,68,69,70,69,65,84,69,68,0
deck_next_label: .byte 67,72,79,79,83,69,32,65,32,67,65,82,68,32,84,79,32,80,82,79,67,69,69,68,0
deck_clear_label: .byte 67,73,82,67,85,73,84,32,82,69,83,84,79,82,69,68,0
deck_clear_detail: .byte 65,76,76,32,57,32,66,79,83,83,69,83,32,68,69,70,69,65,84,69,68,0
.section .runtime, data
deck_intro_sound: .word 360,25,230,35,160,45,0,0
deck_victory_sound: .word 286,30,226,40,189,50,139,70,0,0
.section .data, data
deck_enemy_sound: .word 300,25,360,35,0,0
deck_card_sounds:
    .word 180,22,100,15,0,0
    .word 360,25,300,25,0,0
    .word 120,12,80,18,0,0
    .word 300,15,90,40,0,0
    .word 440,25,220,45,0,0
    .word 226,20,139,40,0,0
    .word 410,16,460,28,0,0
    .word 360,15,120,35,0,0
    .word 180,25,286,35,0,0
    .word 500,35,90,80,0,0
    .word 280,20,140,20,0,0
    .word 190,14,190,35,0,0
