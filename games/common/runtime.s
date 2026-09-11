; SPDX-License-Identifier: MIT
; Shared JR-800 application shell. No host gameplay or hidden host state.
.global entry
.global update_begin
.global frame_ready
.global phase
.global stage
.global menu_choice
.global game_ticks
.global selection_active
.global resume_pending
.global redraw
.global seed
.extern init
.extern clear
.extern framebuffer
.extern font
.extern basic_save
.extern basic_check_break
.extern dirty_all
.extern dirty_reset
.extern dirty_mark
.extern dirty_begin
.extern dirty_next
.extern dirty_pending
.extern dirty_bytes
.extern input_init
.extern input_poll
.extern input_take
.extern input_gate
.extern input_held
.extern input_event
.extern input_ticks
.extern sound_tone
.extern sound_port
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR dirty_reset
    JSR input_init
    LDAA #$10
    STAA $0000
    LDAA #$EF
    STAA sound_port
    CLR stage
    LDAA #$5D
    STAA seed
; @if puzzle
    JSR challenge_init
; @endif
    JSR show_title
    JMP flush
frame_ready:
    LDAA input_ticks
    STAA frame_tick
frame_wait:
; @if ice_route
    JSR ice_audio
; @else
    JSR clear_audio
; @endif
    JSR input_poll
    LDAA input_ticks
    CMPA frame_tick
    BEQ frame_wait
    JSR input_take
update_begin:
    LDAA input_event
    BITA #16
    BEQ dispatch_phase
    JSR chirp
dispatch_phase:
    LDAA phase
; @if puzzle
    CMPA #6
    BNE dispatch_not_password
    JMP challenge_edit_update
dispatch_not_password:
    TSTA
; @endif
    BNE dispatch_not_title
    JMP update_title
dispatch_not_title:
    CMPA #1
    BNE dispatch_not_select
    JMP update_select
dispatch_not_select:
    CMPA #2
    BNE dispatch_menu
    INC game_ticks
    LDAA input_event
    BITA #32
    BEQ update_play
    TST selection_active
    BEQ open_menu
    CLR selection_active
    JSR input_gate
    JSR paint_clear
    JSR game_render
    JMP flush
open_menu:
    LDAA #3
    STAA phase
    CLR menu_choice
    JSR input_gate
    JSR draw_menu
    JMP flush
update_play:
    JSR game_update
    TST redraw
    BNE play_redraw
    JMP flush
play_redraw:
    CLR redraw
    JSR render_play_result
    JMP flush
dispatch_menu:
    CMPA #3
    BNE update_result
    JMP update_menu
update_title:
    INC seed
    LDAA input_event
    BITA #16
    BEQ flush
    JSR show_select
    JMP flush
update_select:
; @if puzzle
    JMP challenge_select_update
; @else
    LDAA input_event
    BITA #32
    BEQ select_confirm
    JSR show_title
    JMP flush
select_confirm:
    BITA #16
    BEQ select_move
    JSR begin_game
    JMP flush
select_move:
    BITA #5
    BEQ select_right
    TST stage
    BEQ select_wrap_last
    DEC stage
    BRA select_redraw
select_wrap_last:
    LDAA #STAGES - 1
    STAA stage
    BRA select_redraw
select_right:
    BITA #10
    BEQ flush
    INC stage
    LDAA stage
    CMPA #STAGES
    BCS select_redraw
    CLR stage
select_redraw:
    JSR draw_select
    BRA flush
; @endif
update_result:
; @if ice_route
; @else
    TST clear_active
    BEQ result_input
    JSR clear_update
    JMP flush
result_input:
; @endif
    LDAA input_event
    BITA #32
    BEQ result_confirm
    JSR show_select
    BRA flush
result_confirm:
    BITA #16
    BEQ flush
    LDAA phase
    CMPA #4
    BNE result_retry
    INC stage
    LDAA stage
    CMPA #STAGES
    BCS result_retry
    CLR stage
result_retry:
    JSR begin_game
flush:
    CLR dirty_bytes
    CLR dirty_bytes + 1
    TST dirty_pending
    BEQ flush_done
    JSR dirty_begin
flush_loop:
    JSR dirty_next
    BEQ flush_done
    JSR input_poll
    BRA flush_loop
flush_done:
    JMP frame_ready
begin_game:
    LDAA #2
    STAA phase
    LDAA #1
    STAA resume_pending
    CLR selection_active
    CLR game_ticks
    CLR redraw
    JSR input_gate
    JSR paint_clear
    JSR game_start
    JMP game_render
show_select:
; @if puzzle
    JMP challenge_select_show
draw_select:
    JMP challenge_select_draw
; @else
    LDAA #1
    STAA phase
    JSR input_gate
    JSR paint_clear
    LDX #game_name
    LDAA #12
    LDAB #1
    JSR paint_text
    LDX #select_label
    LDAB #STAGES
    CMPB #3
    BNE select_label_ready
    LDX #difficulty_label
select_label_ready:
    LDAA #12
    LDAB #3
    JSR paint_text
    LDX #select_help
    LDAA #12
    LDAB #6
    JSR paint_text
    LDX #select_help2
    LDAA #12
    LDAB #7
    JSR paint_text
draw_select:
    LDAA #126
    STAA paint_x
    LDAA #3
    STAA paint_band
    LDAA stage
    INCA
    JMP paint_number
; @endif
show_title:
    CLR phase
    JSR input_gate
    JSR paint_clear
; @if puzzle
    LDX #framebuffer
    STX unpack_dest
    LDX #title_art
    JSR puzzle_unpack
; @else
    LDX #title_art
    STX title_source
    LDX #framebuffer
    STX title_dest
    LDAA #12
    STAA title_chunks
title_chunk:
    LDAB #128
title_copy:
    LDX title_source
    LDAA 0,X
    INX
    STX title_source
    LDX title_dest
    STAA 0,X
    INX
    STX title_dest
    DECB
    BNE title_copy
    JSR input_poll
    DEC title_chunks
    BNE title_chunk
; @endif
    JMP fanfare
update_menu:
    LDAA input_event
    BITA #32
    BNE menu_resume
    BITA #16
    BNE menu_activate
    BITA #5
    BEQ menu_down
    TST menu_choice
    BEQ menu_up_wrap
    DEC menu_choice
    BRA menu_repaint
menu_up_wrap:
    LDAA #5
    STAA menu_choice
    BRA menu_repaint
menu_down:
    BITA #10
    BEQ menu_done
    INC menu_choice
    LDAA menu_choice
    CMPA #6
    BCS menu_repaint
    CLR menu_choice
menu_repaint:
    JSR draw_menu
menu_done:
    JMP flush
menu_activate:
    LDAA menu_choice
    BEQ menu_resume
    CMPA #3
    BEQ menu_retry
    CMPA #4
    BEQ menu_select
    CMPA #5
    BEQ menu_title
    PSHA
    LDAA #2
    STAA phase
    PULA
    JSR game_aux
    BRA menu_refresh
menu_resume:
    LDAA #2
    STAA phase
menu_refresh:
    LDAA #1
    STAA resume_pending
    JSR input_gate
    JSR paint_clear
    JSR render_play_result
    JMP flush
menu_retry:
    JSR begin_game
    JMP flush
menu_select:
    JSR show_select
    JMP flush
menu_title:
    JSR show_title
    JMP flush
draw_menu:
    JSR paint_clear
    LDX #menu_heading
    LDAA #12
    CLRB
    JSR paint_text
    CLR menu_row
menu_draw_loop:
    LDAA menu_row
    ASLA
    TAB
    LDX #menu_labels
    ABX
    LDX 0,X
    LDAA #30
    LDAB menu_row
    INCB
    JSR paint_text
    LDAA menu_row
    CMPA menu_choice
    BNE menu_draw_next
    LDX #menu_arrow
    LDAA #12
    LDAB menu_row
    INCB
    JSR paint_text
menu_draw_next:
    INC menu_row
    LDAA menu_row
    CMPA #6
    BNE menu_draw_loop
    RTS
render_play_result:
; @if puzzle
    LDAA phase
    CMPA #4
    BNE puzzle_render_live
    JSR game_bonus
puzzle_render_live:
; @endif
    JSR game_render
    LDAA phase
    CMPA #4
    BEQ win_game
    CMPA #5
    BEQ lose_game
    RTS
win_game:
; @if puzzle
    JSR challenge_award
; @endif
; @if ice_route
    ; ICE ROUTE has already played its clear melody over the solved board.
; @else
    JMP clear_begin
; @endif
    LDAA #4
    STAA phase
    LDX #result_win
    BRA result_draw
lose_game:
    LDAA #5
    STAA phase
    LDX #result_lose
result_draw:
    STX result_label
; @if puzzle
    JMP challenge_result
; @else
    JSR input_gate
    LDX #result_border
    LDAA #36
    LDAB #2
    JSR paint_text
    LDX #result_blank
    LDAA #36
    LDAB #3
    JSR paint_text
    LDX result_label
    LDAA #78
    LDAB #3
    JSR paint_text
    LDX #result_again
    LDAA #36
    LDAB #4
    JSR paint_text
    LDX #result_border
    LDAA #36
    LDAB #5
    JSR paint_text
    RTS
; @endif
; Nonzero deterministic 8-bit LFSR, seeded by title waiting time.
random:
    LDAA seed
    LSRA
    BCC random_store
    EORA #$B8
random_store:
    STAA seed
    RTS
; Short UI cue, about 6ms nominal. Title/clear notes poll between tiny blocks.
chirp:
    LDX #100
    LDD #8
    JSR sound_tone
    JMP input_poll
fanfare:
    CLR note_index
note_begin:
    LDAA #4
    STAA note_blocks
note_block:
    LDAB note_index
    LDX #note_periods
    ABX
    LDX 0,X
    LDD #2
    JSR sound_tone
    JSR input_poll
    DEC note_blocks
    BNE note_block
    INC note_index
    INC note_index
    LDAA note_index
    CMPA #6
    BNE note_begin
    RTS
.section .bss, bss
note_index: .space 1
note_blocks: .space 1
phase: .space 1
stage: .space 1
menu_choice: .space 1
menu_row: .space 1
game_ticks: .space 1
redraw: .space 1
selection_active: .space 1
resume_pending: .space 1
seed: .space 1
frame_tick: .space 1
title_source: .space 2
title_dest: .space 2
title_chunks: .space 1
result_label: .space 2
.section .data, data
note_periods: .word 300,240,200
difficulty_label: .byte 68,73,70,70,73,67,85,76,84,89,0
select_label: .byte 83,84,65,71,69,0 ; STAGE
select_help: .byte 87,65,83,68,32,79,82,32,50,52,54,56,32,32,32,83,80,65,67,69,32,83,84,65,82,84,0 ; WASD OR 2468   SPACE START
select_help2: .byte 82,69,84,85,82,78,32,66,65,67,75,32,32,32,66,82,69,65,75,32,66,65,83,73,67,0 ; RETURN BACK   BREAK BASIC
menu_heading: .byte 80,65,85,83,69,68,0 ; PAUSED
menu_resume_label: .byte 82,69,83,85,77,69,0 ; RESUME
menu_retry_label: .byte 82,69,84,82,89,0 ; RETRY
menu_select_label: .byte 83,69,76,69,67,84,32,76,69,86,69,76,0 ; SELECT LEVEL
menu_title_label: .byte 84,73,84,76,69,0 ; TITLE
menu_arrow: .byte 45,0 ; -
result_win: .byte 67,76,69,65,82,0 ; CLEAR
result_lose: .byte 70,65,73,76,69,68,0 ; FAILED
result_again: .byte 124,32,83,80,65,67,69,58,32,67,79,78,84,73,78,85,69,32,32,124,0
result_border: .byte 43,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,43,0
result_blank: .byte 124,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,124,0
menu_labels:
    .word menu_resume_label,aux1_label,aux2_label,menu_retry_label,menu_select_label,menu_title_label
.section .text, code
