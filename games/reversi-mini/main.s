; SPDX-License-Identifier: MIT
.global rev_legal
.global rev_cpu_disks
.global rev_passed
.global rev_result
.global rev_active
.global rev_animation_frame
.global rev_anim_phase
.global rev_anim_cell
.global rev_sprite
.global rev_queue
.global rev_queue_count
.global rev_queue_index
.global rev_placed_cell
.global rev_flash_left
.global rev_anim_clock
.global rev_disk_sound
.global rev_display
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR rev_result
    CLR rev_passed
    CLR rev_apply
    CLR rev_active
    LDX #board
    LDAA #1
    STAA 14,X
    STAA 21,X
    LDAA #2
    STAA 15,X
    STAA 20,X
    LDAA #8
    STAA cursor
    JSR rev_count_disks
    LDAA #1
    JMP rev_find_moves
game_update:
    JSR cursor_blink_tick
    TST rev_active
    BEQ rev_input
    JMP rev_animation_update
rev_input:
    LDAA input_event
    BITA #16
    BEQ rev_cursor_move
    LDAB cursor
    LDX #rev_legal
    ABX
    TST 0,X
    BNE rev_play
    RTS
rev_cursor_move:
    JSR grid_move
    CMPB #255
    BEQ rev_idle
    STAB cursor
    JMP grid_changed
rev_idle:
    RTS
rev_play:
    JSR grid_snapshot
    LDAA cursor
    STAA rev_player_cursor
    LDAA #255
    STAA cursor
    CLR rev_passed
    JSR grid_count_move
    LDAA #1
    STAA rev_who
    LDAB rev_player_cursor
    JSR rev_begin_place
    JMP rev_animation_update
rev_cpu_turn:
    LDAA #2
    JSR rev_find_moves
    TST rev_legal_count
    BNE rev_cpu_plays
    LDAA #1
    JSR rev_find_moves
    TST rev_legal_count
    BEQ rev_finish
    LDAA #2
    STAA rev_passed
    JMP rev_human_turn
rev_cpu_plays:
    LDAA #2
    STAA rev_who
    LDAB rev_best_cell
    JMP rev_begin_place
rev_cpu_done:
    LDAA #1
    JSR rev_find_moves
    TST rev_legal_count
    BNE rev_human_turn
    LDAA #1
    STAA rev_passed
    BRA rev_cpu_turn
rev_finish:
    CLR rev_active
    LDAA rev_player_cursor
    STAA cursor
    JSR rev_count_disks
    LDAA grid_stat
    CMPA rev_cpu_disks
    BCS rev_lost
    BEQ rev_drawn
    LDAA #1
    STAA rev_result
    LDAA #4
    STAA phase
    JMP grid_changed
rev_drawn:
    LDAA #3
    STAA rev_result
    LDAA #4
    STAA phase
    JMP grid_changed
rev_lost:
    LDAA #2
    STAA rev_result
    LDAA #5
    STAA phase
    JMP grid_changed
rev_human_turn:
    CLR rev_active
    LDAA rev_player_cursor
    STAA cursor
    JSR rev_count_disks
    JSR input_gate
    JMP grid_changed

; Collect the rays and retain their old faces for presentation. Apply the move
; with the same rule routine used by the native fixtures, then reveal each flip.
rev_begin_place:
    STAB rev_placed_cell
    STAB rev_anim_cell
    LDAA rev_who
    STAA rev_active
    LDX #board
    STX rev_copy_source
    LDX #rev_display
    STX rev_copy_dest
    LDAB #36
rev_copy_display:
    LDX rev_copy_source
    LDAA 0,X
    INX
    STX rev_copy_source
    LDX rev_copy_dest
    STAA 0,X
    INX
    STX rev_copy_dest
    DECB
    BNE rev_copy_display
    CLR rev_queue_count
    CLR rev_queue_index
    LDAA #2
    STAA rev_apply
    LDAB rev_placed_cell
    JSR rev_measure
    LDAB rev_placed_cell
    JSR rev_place
    LDX #rev_display
    LDAB rev_placed_cell
    ABX
    LDAA rev_active
    STAA 0,X
    JSR rev_count_disks
    LDAA #1
    STAA rev_anim_phase
    LDAA #6
    STAA rev_sprite
    LDAA #2
    STAA rev_anim_delay
    LDAA input_ticks
    STAA rev_anim_clock
    CLR resume_pending
    JMP rev_disk_sound

; An animated turn owns its LCD transfers, including its final board/menu.
; Tail-enter the shell's frame boundary after removing the game_update return
; address. Its ordinary flush would discard the intermediate transfer count.
rev_animation_update:
    CLR rev_frame_bytes
    CLR rev_frame_bytes + 1
    JSR rev_animation_loop
    TST redraw
    BEQ rev_animation_finish
    CLR redraw
    JSR render_play_result
rev_animation_finish:
    JSR rev_flush
    LDD rev_frame_bytes
    STD dirty_bytes
    PULX
    JMP frame_ready

; Present intermediate LCD states while polling the shell's emulated input
; clock. RETURN ends this update and yields to the ordinary shell menu.
rev_animation_loop:
    TST rev_active
    BNE rev_animation_live
    RTS
rev_animation_live:
    TST resume_pending
    BEQ rev_animation_draw
    LDAA input_ticks
    STAA rev_anim_clock
    CLR resume_pending
rev_animation_draw:
    JSR game_render
rev_animation_frame:
    JSR rev_flush
rev_animation_poll:
    JSR input_poll
    JSR input_take
    LDAA input_event
    BITA #32
    BEQ rev_animation_wait
    LDAA #3
    STAA phase
    CLR menu_choice
    JSR input_gate
    JSR draw_menu
    CLR redraw
    RTS
rev_animation_wait:
    LDAA input_ticks
    SUBA rev_anim_clock
    CMPA rev_anim_delay
    BCS rev_animation_poll
    LDAA input_ticks
    STAA rev_anim_clock
    JSR rev_animation_step
    BRA rev_animation_loop

rev_flush:
    CLR dirty_bytes
    CLR dirty_bytes + 1
    TST dirty_pending
    BEQ rev_flush_done
    JSR dirty_begin
rev_flush_next:
    JSR dirty_next
    BEQ rev_flush_done
    JSR input_poll
    BRA rev_flush_next
rev_flush_done:
    LDD dirty_bytes
    ADDD rev_frame_bytes
    STD rev_frame_bytes
    RTS

; 1..3: placed disk grows; 4: CPU location blink; 5..8: one disk flips.
rev_animation_step:
    LDAA rev_anim_phase
    CMPA #4
    BEQ rev_blink_step
    INC rev_anim_phase
    CMPA #1
    BEQ rev_narrow_new
    CMPA #2
    BEQ rev_full_new
    CMPA #3
    BEQ rev_placement_done
    CMPA #5
    BEQ rev_edge
    CMPA #6
    BEQ rev_narrow_new
    CMPA #7
    BEQ rev_flip_commit
    INC rev_queue_index
    BRA rev_next_disk
rev_narrow_new:
    LDAA rev_active
    ADDA #3
    STAA rev_sprite
    RTS
rev_full_new:
    LDAA rev_active
    STAA rev_sprite
    RTS
rev_edge:
    LDAA #6
    STAA rev_sprite
    RTS
rev_flip_commit:
    LDX #rev_display
    LDAB rev_anim_cell
    ABX
    LDAA rev_active
    STAA 0,X
    STAA rev_sprite
    JMP rev_count_disks
rev_placement_done:
    LDAA rev_active
    CMPA #2
    BNE rev_next_disk
    LDAA #6
    STAA rev_flash_left
    LDAA #8
    STAA rev_anim_delay
    RTS
rev_blink_step:
    DEC rev_flash_left
    BEQ rev_blink_done
    LDAA rev_sprite
    EORA #2
    STAA rev_sprite
    RTS
rev_blink_done:
    LDAA #2
    STAA rev_anim_delay
rev_next_disk:
    LDAB rev_queue_index
    CMPB rev_queue_count
    BEQ rev_move_done
    LDX #rev_queue
    ABX
    LDAB 0,X
    STAB rev_anim_cell
    LDAA #6
    SUBA rev_active
    STAA rev_sprite
    LDAA #5
    STAA rev_anim_phase
    JMP rev_disk_sound
rev_move_done:
    LDAA rev_active
    CMPA #1
    BNE rev_after_cpu
    JMP rev_cpu_turn
rev_after_cpu:
    JMP rev_cpu_done
rev_disk_sound:
    LDX #140
    LDAA rev_active
    CMPA #1
    BEQ rev_sound_ready
    LDX #110
rev_sound_ready:
    LDD #16
    JSR sound_tone
    JMP input_poll
rev_count_disks:
    CLR grid_stat
    CLR rev_cpu_disks
    LDX #board
    TST rev_active
    BEQ rev_count_ready
    LDX #rev_display
rev_count_ready:
    LDAB #36
rev_count_loop:
    LDAA 0,X
    CMPA #1
    BNE rev_count_cpu
    INC grid_stat
    BRA rev_count_next
rev_count_cpu:
    CMPA #2
    BNE rev_count_next
    INC rev_cpu_disks
rev_count_next:
    INX
    DECB
    BNE rev_count_loop
    RTS
; A player -> all legal moves and a positional CPU choice. No board mutation.
rev_find_moves:
    STAA rev_who
    CLR rev_scan
    CLR rev_legal_count
    CLR rev_best_rank
    CLR rev_apply
rev_scan_loop:
    LDAB rev_scan
    JSR rev_measure
    LDAB rev_scan
    LDX #rev_legal
    ABX
    LDAA rev_total
    STAA 0,X
    BEQ rev_scan_next
    INC rev_legal_count
    ASLA
    ADDA #128
    STAA rev_rank
    LDX #rev_weights
    ABX
    LDAA 0,X
    ADDA rev_rank
    CMPA rev_best_rank
    BLS rev_scan_next
    STAA rev_best_rank
    STAB rev_best_cell
rev_scan_next:
    JSR input_poll
    INC rev_scan
    LDAA rev_scan
    CMPA #36
    BNE rev_scan_loop
    RTS
rev_place:
    LDAA #1
    STAA rev_apply
    JSR rev_measure
    CLR rev_apply
    RTS
; B candidate. Apply 0 counts, 1 places/flips, 2 only queues the captured rays.
rev_measure:
    STAB rev_cell
    CLR rev_total
    LDX #board
    ABX
    TST 0,X
    BEQ rev_measure_empty
    RTS
rev_measure_empty:
    CLR rev_dir
rev_direction:
    LDAA rev_dir
    LDAB #36
    MUL
    ADDD #rev_links
    STD rev_pointer
    LDAA rev_cell
    STAA rev_walk
    CLR rev_length
rev_ray:
    JSR rev_next
    CMPB #255
    BEQ rev_next_direction
    LDX #board
    ABX
    LDAA 0,X
    BEQ rev_next_direction
    CMPA rev_who
    BEQ rev_bracket
    INC rev_length
    BRA rev_ray
rev_bracket:
    LDAA rev_length
    ADDA rev_total
    STAA rev_total
    TST rev_apply
    BEQ rev_next_direction
    TST rev_length
    BEQ rev_next_direction
    LDAA rev_cell
    STAA rev_walk
rev_flip_ray:
    JSR rev_next
    LDAA rev_apply
    CMPA #2
    BEQ rev_queue_disk
    LDX #board
    ABX
    LDAA rev_who
    STAA 0,X
    BRA rev_ray_applied
rev_queue_disk:
    TBA
    LDX #rev_queue
    LDAB rev_queue_count
    ABX
    STAA 0,X
    INC rev_queue_count
rev_ray_applied:
    DEC rev_length
    BNE rev_flip_ray
rev_next_direction:
    INC rev_dir
    LDAA rev_dir
    CMPA #8
    BNE rev_direction
    LDAA rev_apply
    CMPA #1
    BNE rev_measure_done
    TST rev_total
    BEQ rev_measure_done
    LDX #board
    LDAB rev_cell
    ABX
    LDAA rev_who
    STAA 0,X
rev_measure_done:
    RTS
rev_next:
    LDX rev_pointer
    LDAB rev_walk
    ABX
    LDAB 0,X
    STAB rev_walk
    RTS
game_aux:
    CMPA #1
    BNE rev_restart
    TST undo_valid
    BEQ rev_aux_done
    CLR rev_active
    JSR grid_restore
    CLR rev_result
    CLR rev_passed
    JSR rev_count_disks
    LDAA #1
    JMP rev_find_moves
rev_aux_done:
    RTS
rev_restart:
    JMP game_start
grid_value:
    TST rev_active
    BEQ rev_board_value
    CMPB rev_anim_cell
    BNE rev_board_value
    LDAA rev_sprite
    RTS
rev_board_value:
    LDX #board
    TST rev_active
    BEQ rev_board_read
    LDX #rev_display
rev_board_read:
    ABX
    LDAA 0,X
    BNE rev_tile_done
    TST rev_active
    BNE rev_tile_done
    LDX #rev_legal
    ABX
    TST 0,X
    BEQ rev_tile_done
    LDAA #3
rev_tile_done:
    RTS
game_render:
    JSR cursor_blink_prepare
    JSR cursor_blink_board
    JMP visual_hud

.section .bss, bss
rev_legal: .space 36
rev_legal_count: .space 1
rev_cpu_disks: .space 1
rev_passed: .space 1
rev_result: .space 1
rev_who: .space 1
rev_scan: .space 1
rev_best_cell: .space 1
rev_best_rank: .space 1
rev_rank: .space 1
rev_apply: .space 1
rev_cell: .space 1
rev_total: .space 1
rev_dir: .space 1
rev_pointer: .space 2
rev_walk: .space 1
rev_length: .space 1
rev_saved_result: .space 1
rev_active: .space 1
rev_display: .space 36
rev_copy_source: .space 2
rev_copy_dest: .space 2
rev_anim_phase: .space 1
rev_anim_cell: .space 1
rev_sprite: .space 1
rev_queue: .space 36
rev_queue_count: .space 1
rev_queue_index: .space 1
rev_placed_cell: .space 1
rev_flash_left: .space 1
rev_anim_clock: .space 1
rev_anim_delay: .space 1
rev_player_cursor: .space 1
rev_frame_bytes: .space 2
.section .data, data
rev_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
rev_space_label: .byte 32,83,80,65,67,69,32,32,0
rev_cpu_label: .byte 67,80,85,0
rev_win_label: .byte 89,79,85,32,87,73,78,0
rev_loss_label: .byte 67,80,85,32,87,73,78,0
rev_draw_label: .byte 68,82,65,87,32,32,32,0
rev_you_pass: .byte 89,79,85,32,80,65,83,83,0
rev_cpu_pass: .byte 67,80,85,32,80,65,83,83,0

; The JR-800 input timer drives focus blinking; input immediately restores it.
.global cursor_blink_mask
.global cursor_blink_clock
.section .text, code
cursor_blink_prepare:
    TST hud_ready
    BEQ cursor_blink_show
    RTS
cursor_blink_tick:
    TST rev_active
    BNE cursor_blink_show
    TST input_event
    BNE cursor_blink_show
    LDAA input_ticks
    SUBA cursor_blink_clock
    CMPA #25
    BCS cursor_blink_idle
    LDAA cursor_blink_mask
    EORA #128
    BRA cursor_blink_store
cursor_blink_show:
    LDAA #128
cursor_blink_store:
    CMPA cursor_blink_mask
    BEQ cursor_blink_time
    STAA cursor_blink_mask
    LDAA #1
    STAA redraw
cursor_blink_time:
    LDAA input_ticks
    STAA cursor_blink_clock
cursor_blink_idle:
    RTS
cursor_blink_board:
    LDAA cursor
    PSHA
    TST cursor_blink_mask
    BNE cursor_blink_paint
    LDAA #255
    STAA cursor
cursor_blink_paint:
    JSR paint_board
    PULA
    STAA cursor
    RTS
.section .bss, bss
cursor_blink_mask: .space 1
cursor_blink_clock: .space 1
