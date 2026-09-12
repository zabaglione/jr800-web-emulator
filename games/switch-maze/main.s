; SPDX-License-Identifier: MIT
.global gates
.section .text, code
game_render:
    JSR paint_board
    JMP visual_hud

game_start:
    JSR grid_reset
    JSR challenge_start
    CLR undo_valid
    CLR gates
    LDX #board
    JSR challenge_load
    LDAB stage
    LDX #start_cells
    ABX
    LDAA 0,X
    STAA cursor
    LDAA #2
    STAA grid_stat
    CLR switch_intro_bytes
    CLR switch_intro_bytes + 1
    JSR game_render
    LDAB cursor
    JSR switch_intro_cell
    LDX #board
    CLRB
switch_find_exit:
    LDAA 0,X
    CMPA #8
    BEQ switch_intro_exit
    INX
    INCB
    BRA switch_find_exit
switch_intro_exit:
    JSR switch_intro_cell
    JSR input_gate
    ; This introduction owns all LCD transfers. Leave both begin_game frames
    ; at the ordinary shell boundary with their complete transfer count.
    LDD switch_intro_bytes
    STD dirty_bytes
    PULX
    PULX
    JMP frame_ready
game_update:
    JSR grid_move
    CMPB #255
    BNE switch_candidate
    RTS
switch_candidate:
    STAB switch_target
    LDX #board
    ABX
    LDAA 0,X
    STAA switch_tile
    CMPA #1
    BNE switch_door_a
    RTS
switch_door_a:
    CMPA #6
    BNE switch_door_b
    LDAA gates
    BITA #1
    BNE switch_accept
    RTS
switch_door_b:
    CMPA #7
    BNE switch_accept
    LDAA gates
    BITA #2
    BNE switch_accept
    RTS
switch_accept:
    JSR grid_snapshot
    LDAA gates
    STAA switch_saved_gates
    LDAB switch_target
    STAB cursor
    JSR challenge_touch
    LDAA switch_tile
    CMPA #2
    BEQ switch_key
    CMPA #3
    BEQ switch_key
    CMPA #4
    BEQ switch_a
    CMPA #5
    BEQ switch_b
    CMPA #8
    BNE switch_finish
    TST grid_stat
    BNE switch_finish
    LDAA #4
    STAA phase
    BRA switch_finish
switch_key:
    LDX #board
    ABX
    CLR 0,X
    DEC grid_stat
    BRA switch_finish
switch_a:
    LDAA gates
    EORA #1
    STAA gates
    BRA switch_finish
switch_b:
    LDAA gates
    EORA #2
    STAA gates
switch_finish:
    JMP grid_count_move
game_aux:
    CMPA #1
    BNE switch_restart
    TST undo_valid
    BEQ switch_idle
    JSR grid_restore
    LDAA switch_saved_gates
    STAA gates
switch_idle:
    RTS
switch_restart:
    JMP begin_game
grid_value:
    CMPB cursor
    BNE switch_board_tile
    LDAA #11
    RTS
switch_board_tile:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #6
    BNE switch_tile_b
    LDAB gates
    BITB #1
    BEQ switch_tile_done
    LDAA #9
    RTS
switch_tile_b:
    CMPA #7
    BNE switch_tile_done
    LDAB gates
    BITB #2
    BEQ switch_tile_done
    LDAA #10
switch_tile_done:
    RTS
.section .bss, bss
gates: .space 1
switch_saved_gates: .space 1
switch_source: .space 2
switch_index: .space 1
switch_target: .space 1
switch_tile: .space 1

.global switch_intro_target
.global switch_intro_step
.global switch_intro_frame
.section .bss, bss
switch_intro_target: .space 1
switch_intro_step: .space 1
switch_intro_clock: .space 1
switch_intro_bytes: .space 2

; The start cue finishes before the exit cue; ordinary controls begin only
; after both. These cycle-timed LCD updates use the program's existing RAM.
.section .runtime, code
switch_intro_cell:
    LDX #view_cells
    CLRA
switch_intro_find:
    CMPB 0,X
    BEQ switch_intro_found
    INX
    INCA
    BRA switch_intro_find
switch_intro_found:
    STAA switch_intro_target
    CLR switch_intro_step
switch_intro_blink:
    LDAB switch_intro_target
    JSR game_tile
    LDAB switch_intro_step
    BITB #1
    BNE switch_intro_face
    EORA #128
switch_intro_face:
    LDAB switch_intro_target
    JSR paint_tile
    JSR dirty_begin
switch_intro_flush:
    JSR dirty_next
    BEQ switch_intro_accumulate
    JSR input_poll
    BRA switch_intro_flush
switch_intro_accumulate:
    LDD dirty_bytes
    ADDD switch_intro_bytes
    STD switch_intro_bytes
switch_intro_frame:
    TST switch_intro_step
    BNE switch_intro_wait
    LDX #250
    LDAB grid_cell
    CMPB cursor
    BEQ switch_intro_note
    LDX #180
switch_intro_note:
    LDD #20
    JSR sound_tone
switch_intro_wait:
    LDAA input_ticks
    STAA switch_intro_clock
switch_intro_delay:
    JSR input_poll
    LDAA input_ticks
    SUBA switch_intro_clock
    CMPA #8
    BCS switch_intro_delay
    INC switch_intro_step
    LDAA switch_intro_step
    CMPA #4
    BNE switch_intro_blink
    RTS

.section .text, code
game_bonus:
    RTS
