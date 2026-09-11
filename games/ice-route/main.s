; SPDX-License-Identifier: MIT
; All animation uses the SDK's 20000 CPU-cycle clock; menus freeze its state.
; mode: 0 ready, 1 introduction, 2 sliding, 3 clear celebration.
.global ice_mode
.global ice_reveal
.global ice_visible
.global ice_timer
.global ice_sfx_left
.global ice_pitch
.global ice_note
.section .text, code
game_render:
    JSR paint_board
    JMP visual_hud

game_start:
    JSR grid_reset
    JSR challenge_start
    CLR undo_valid
    CLR ice_sfx_left
    CLR ice_reveal
    CLR ice_visible
    LDAA #1
    STAA ice_mode
    LDAA #12
    STAA ice_timer
    LDX #board
    JSR challenge_load
    LDAB stage
    LDX #start_cells
    ABX
    LDAA 0,X
    STAA cursor
    LDAA #2
    STAA grid_stat
    RTS

game_update:
    LDAA input_ticks
    TST resume_pending
    BEQ ice_elapsed
    STAA ice_last
    CLR resume_pending
ice_elapsed:
    SUBA ice_last
    STAA ice_delta
    LDAA input_ticks
    STAA ice_last
    LDAA ice_sfx_left
    JSR ice_subtract
    STAA ice_sfx_left
    LDAA ice_mode
    BEQ ice_ready
    LDAA ice_timer
    JSR ice_subtract
    STAA ice_timer
    BEQ ice_due
    RTS
ice_due:
    LDAA ice_mode
    CMPA #1
    BNE ice_not_intro
    JMP ice_intro
ice_not_intro:
    CMPA #2
    BEQ ice_step
    JMP ice_clear_note
ice_subtract:
    SUBA ice_delta
    BCC ice_subtracted
    CLRA
ice_subtracted:
    RTS

ice_ready:
    JSR grid_move
    CMPB #255
    BEQ ice_idle
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ ice_idle
    LDAA input_event
    STAA ice_direction
    JSR grid_snapshot
    JSR grid_count_move
    LDAA #2
    STAA ice_mode
ice_step:
    ; Keep the original direction through the complete slide.
    LDAA ice_direction
    STAA input_event
    JSR grid_move
    CMPB #255
    BEQ ice_stop
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ ice_stop
    STAB cursor
    JSR challenge_touch
    CMPA #3
    BCS ice_exit
    CMPA #4
    BHI ice_step_draw
    CLR 0,X
    DEC grid_stat
    LDX #139
    JSR ice_cue
    BRA ice_step_draw
ice_exit:
    CMPA #2
    BNE ice_step_draw
    TST grid_stat
    BNE ice_step_draw
    LDAA #3
    STAA ice_mode
    CLR ice_note
    ; Flush the arrival before the first note and retain the solved board.
ice_step_draw:
    LDAA #5
    STAA ice_timer
    JMP grid_changed
ice_stop:
    CLR ice_mode
ice_idle:
    RTS

ice_intro:
    INC ice_reveal
    LDAA ice_reveal
    CMPA #5
    BCC ice_blink
    JSR challenge_invalidate
    LDX #185
    LDAA ice_reveal
    CMPA #3
    BCS ice_intro_cue
    LDX #139
ice_intro_cue:
    JSR ice_cue
    LDAA #15
    BRA ice_intro_draw
ice_blink:
    LDAA ice_visible
    EORA #1
    STAA ice_visible
    LDAA ice_reveal
    CMPA #5
    BNE ice_blink_next
    LDX #110
    JSR ice_cue
ice_blink_next:
    LDAA #7
    LDAB ice_reveal
    CMPB #11
    BCS ice_intro_draw
    CLR ice_mode
    JSR input_gate
    LDAA #0
ice_intro_draw:
    STAA ice_timer
    JMP grid_changed

ice_clear_note:
    LDAB ice_note
    CMPB #7
    BCS ice_next_note
    LDAA #4
    STAA phase
    JSR input_gate
    JMP grid_changed
ice_next_note:
    ASLB
    LDX #ice_melody
    ABX
    LDX 0,X
    STX ice_pitch
    LDAA #7
    STAA ice_sfx_left
    INCA
    STAA ice_timer
    INC ice_note
    LDAB ice_note
    CMPB #7
    BNE ice_clear_draw
    LDAA #18
    STAA ice_sfx_left
    LDAA #38
    STAA ice_timer
ice_clear_draw:
    ; Alternate raised-arm poses while the melody plays.
    JMP grid_changed

; Short SE and melody slices run in otherwise idle frame waits. Each slice
; returns for a key scan; no whole-note busy wait or extra framebuffer.
ice_cue:
    STX ice_pitch
    LDAA #5
    STAA ice_sfx_left
    RTS
ice_audio:
    LDAA phase
    CMPA #2
    BNE ice_audio_done
    TST ice_sfx_left
    BEQ ice_audio_done
    LDX ice_pitch
    LDD #2
    JSR sound_tone
ice_audio_done:
    RTS

game_aux:
    CMPA #1
    BNE ice_restart
    TST undo_valid
    BNE ice_undo
    RTS
ice_undo:
    CLR ice_mode
    CLR ice_sfx_left
    JMP grid_restore
ice_restart:
    JMP game_start

grid_value:
    CMPB cursor
    BNE ice_tile
    TST ice_visible
    BEQ ice_tile
    LDAA #5
    LDAB ice_mode
    CMPB #3
    BNE ice_value_done
    LDAA ice_note
    ANDA #1
    ADDA #6
ice_value_done:
    RTS
ice_tile:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #3
    BCS ice_value_done
    CMPA #4
    BHI ice_value_done
    SUBA #2
    CMPA ice_reveal
    BLS ice_item_visible
    CLRA
    RTS
ice_item_visible:
    ADDA #2
    RTS

game_bonus:
    RTS
.section .data, data
; Original C-E-G-C / G-A-C cadence, octave 5/6, E clock 1.2288 MHz.
ice_melody: .word 286,226,189,139,189,167,139
.section .bss, bss
ice_mode: .space 1
ice_timer: .space 1
ice_last: .space 1
ice_delta: .space 1
ice_reveal: .space 1
ice_visible: .space 1
ice_direction: .space 1
ice_sfx_left: .space 1
ice_pitch: .space 2
ice_note: .space 1
