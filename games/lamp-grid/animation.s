; SPDX-License-Identifier: MIT
.global lamp_active
.global lamp_animation_frame
.global lamp_anim_phase
.global lamp_anim_cell
.global lamp_anim_cursor
.global lamp_sprite
.global lamp_queue
.global lamp_queue_count
.global lamp_queue_index
.global lamp_display
.global lamp_view_bonus
.global lamp_flip_sound
.section .text, code
; Retain the visible state while the normal rule routine applies the complete
; cross. This also lets UNDO cancel a move interrupted by the pause menu.
lamp_animation_begin:
    LDAA cursor
    STAA lamp_anim_cursor
    STAA lamp_queue
    LDAA #1
    STAA lamp_queue_count
    STAA lamp_active
    CLR lamp_queue_index
    LDAA challenge_bonus
    STAA lamp_view_bonus
    LDX #board
    STX lamp_copy_source
    LDX #lamp_display
    STX lamp_copy_dest
    LDAB #25
lamp_copy_display:
    LDX lamp_copy_source
    LDAA 0,X
    INX
    STX lamp_copy_source
    LDX lamp_copy_dest
    STAA 0,X
    INX
    STX lamp_copy_dest
    DECB
    BNE lamp_copy_display
    LDX #neighbors
    STX lamp_source
    LDAA #4
    STAA lamp_index
lamp_queue_neighbors:
    LDX lamp_source
    LDAB cursor
    ABX
    LDAA 0,X
    CMPA #255
    BEQ lamp_queue_skip
    LDX #lamp_queue
    LDAB lamp_queue_count
    ABX
    STAA 0,X
    INC lamp_queue_count
lamp_queue_skip:
    LDD lamp_source
    ADDD #25
    STD lamp_source
    DEC lamp_index
    BNE lamp_queue_neighbors
    RTS

lamp_animation_start:
    LDAA #255
    STAA cursor
    CLR resume_pending
    LDAA input_ticks
    STAA lamp_anim_clock
    JMP lamp_next_cell

; Intermediate states own their LCD writes. Enter the shell's frame boundary
; after removing the game_update return address, preserving the transfer total.
lamp_animation_update:
    CLR lamp_frame_bytes
    CLR lamp_frame_bytes + 1
    JSR lamp_animation_loop
    TST redraw
    BEQ lamp_animation_finish
    CLR redraw
    JSR render_play_result
lamp_animation_finish:
    JSR lamp_flush
    LDD lamp_frame_bytes
    STD dirty_bytes
    PULX
    JMP frame_ready

lamp_animation_loop:
    TST lamp_active
    BNE lamp_animation_live
    RTS
lamp_animation_live:
    TST resume_pending
    BEQ lamp_animation_draw
    LDAA input_ticks
    STAA lamp_anim_clock
    CLR resume_pending
lamp_animation_draw:
    JSR game_render
lamp_animation_frame:
    JSR lamp_flush
lamp_animation_poll:
    JSR input_poll
    JSR input_take
    LDAA input_event
    BITA #32
    BEQ lamp_animation_wait
    LDAA #3
    STAA phase
    CLR menu_choice
    JSR input_gate
    JSR draw_menu
    CLR redraw
    RTS
lamp_animation_wait:
    LDAA input_ticks
    SUBA lamp_anim_clock
    CMPA #2
    BCS lamp_animation_poll
    LDAA input_ticks
    STAA lamp_anim_clock
    JSR lamp_animation_step
    BRA lamp_animation_loop

lamp_flush:
    CLR dirty_bytes
    CLR dirty_bytes + 1
    TST dirty_pending
    BEQ lamp_flush_done
    JSR dirty_begin
lamp_flush_next:
    JSR dirty_next
    BEQ lamp_flush_done
    JSR input_poll
    BRA lamp_flush_next
lamp_flush_done:
    LDD dirty_bytes
    ADDD lamp_frame_bytes
    STD lamp_frame_bytes
    RTS

; Each lamp shows its old narrow face, edge, new narrow face, then full face.
lamp_animation_step:
    LDAA lamp_anim_phase
    INC lamp_anim_phase
    CMPA #1
    BEQ lamp_edge
    CMPA #2
    BEQ lamp_new_face
    CMPA #3
    BEQ lamp_commit_face
    INC lamp_queue_index
lamp_next_cell:
    LDAB lamp_queue_index
    CMPB lamp_queue_count
    BEQ lamp_animation_done
    LDX #lamp_queue
    ABX
    LDAB 0,X
    STAB lamp_anim_cell
    LDX #lamp_display
    LDAA lamp_view_bonus
    STAA lamp_face_bonus
    JSR lamp_value_at
    ADDA #4
    STAA lamp_sprite
    LDAA #1
    STAA lamp_anim_phase
    JMP lamp_flip_sound
lamp_edge:
    LDAA #8
    STAA lamp_sprite
    RTS
lamp_new_face:
    JSR lamp_final_value
    ADDA #4
    STAA lamp_sprite
    RTS
lamp_commit_face:
    LDX #board
    LDAB lamp_anim_cell
    ABX
    LDAA 0,X
    LDX #lamp_display
    ABX
    STAA 0,X
    LDAA challenge_bonus
    STAA lamp_view_bonus
    JSR lamp_final_value
    STAA lamp_sprite
    JMP lamp_count
lamp_final_value:
    LDAA challenge_bonus
    STAA lamp_face_bonus
    LDAB lamp_anim_cell
    LDX #board
    JMP lamp_value_at
lamp_animation_done:
    CLR lamp_active
    LDAA lamp_anim_cursor
    STAA cursor
    JSR lamp_count
    TST grid_stat
    BNE lamp_animation_gate
    LDAA #4
    STAA phase
lamp_animation_gate:
    JSR input_gate
    JMP grid_changed
lamp_flip_sound:
    LDX #140
    LDD #12
    JSR sound_tone
    JMP input_poll

.section .bss, bss
lamp_active: .space 1
lamp_anim_phase: .space 1
lamp_anim_cell: .space 1
lamp_anim_cursor: .space 1
lamp_sprite: .space 1
lamp_queue: .space 5
lamp_queue_count: .space 1
lamp_queue_index: .space 1
lamp_display: .space 25
lamp_view_bonus: .space 1
lamp_face_bonus: .space 1
lamp_anim_clock: .space 1
lamp_copy_source: .space 2
lamp_copy_dest: .space 2
lamp_frame_bytes: .space 2
