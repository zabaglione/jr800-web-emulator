; SPDX-License-Identifier: MIT
.global loop_marks
.global loop_paths
.global loop_next
.global loop_undo
.section .text, code
game_start:
    JSR grid_reset
    LDX #loop_marks
    LDAB #72
    CLRA
loop_clear:
    STAA 0,X
    INX
    DECB
    BNE loop_clear
    LDAA #1
    STAA loop_next
    JSR challenge_start
    ; The jewel belongs to the full cell sprite, not a tiny shared corner mark.
    LDAA #255
    STAA challenge_view_cells
    STAA challenge_view_cells + 1
    LDX #board
    JSR challenge_load
    LDAB #0
loop_find_start:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #5
    BEQ loop_found_start
    INCB
    BRA loop_find_start
loop_found_start:
    STAB cursor
    LDAA #3
    STAA grid_stat
    LDAB cursor
    STAB loop_history
    LDX #loop_marks
    ABX
    LDAA #1
    STAA 0,X
    RTS
game_update:
    LDAA input_event
    BITA #16
    BEQ loop_input_direction
    JMP loop_close
loop_input_direction:
    CLR loop_direction
    BITA #1
    BNE loop_move
    INC loop_direction
    BITA #2
    BNE loop_move
    INC loop_direction
    BITA #4
    BNE loop_move
    INC loop_direction
    BITA #8
    BNE loop_move
    RTS
loop_move:
    JSR grid_move
    CMPB #255
    BEQ loop_idle
    STAB loop_to
    LDX #board
    ABX
    TST 0,X
    BEQ loop_idle
    LDX #loop_marks
    ABX
    TST 0,X
    BEQ loop_unvisited
    TST moves
    BEQ loop_idle
    LDAB moves
    DECB
    LDX #loop_history
    ABX
    LDAA 0,X
    CMPA loop_to
    BNE loop_idle
    JMP loop_undo
loop_unvisited:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BCS loop_accept
    DECA
    CMPA loop_next
    BNE loop_idle
    INC loop_next
    DEC grid_stat
loop_accept:
    JSR challenge_step
    LDAA cursor
    STAA loop_from
    JSR loop_connect
    LDAA loop_to
    STAA cursor
    INC moves
    LDAB moves
    LDX #loop_history
    ABX
    STAA 0,X
    LDX #loop_directions
    ABX
    LDAA loop_direction
    STAA 0,X
    LDAB cursor
    LDX #loop_marks
    ABX
    LDAA #1
    STAA 0,X
    JSR game_bonus
    JSR challenge_invalidate
    JMP grid_changed
loop_idle:
    RTS
loop_connect:
    LDAB loop_direction
    LDX #loop_bits
    ABX
    LDAA 0,X
    STAA loop_mask
    LDX #loop_opposite
    ABX
    LDAA 0,X
    LDAB loop_to
    LDX #loop_paths
    ABX
    ORAA 0,X
    STAA 0,X
    LDAB loop_from
    LDX #loop_paths
    ABX
    LDAA loop_mask
    ORAA 0,X
    STAA 0,X
    RTS
loop_undo:
    TST moves
    BEQ loop_idle
    JSR challenge_cost
    LDAB cursor
    LDX #loop_marks
    ABX
    CLR 0,X
    LDX #loop_paths
    ABX
    CLR 0,X
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BCS loop_undo_path
    DEC loop_next
    INC grid_stat
loop_undo_path:
    LDAB moves
    LDX #loop_directions
    ABX
    LDAB 0,X
    LDX #loop_bits
    ABX
    LDAA 0,X
    COMA
    STAA loop_mask
    DEC moves
    LDAB moves
    LDX #loop_history
    ABX
    LDAB 0,X
    STAB cursor
    LDX #loop_paths
    ABX
    LDAA 0,X
    ANDA loop_mask
    STAA 0,X
    JSR game_bonus
    JSR challenge_invalidate
    JMP grid_changed
loop_close:
    TST grid_stat
    BNE loop_close_done
    LDAA loop_next
    CMPA #4
    BNE loop_close_done
    CLR loop_direction
loop_closing_direction:
    LDAA loop_direction
    LDAB #36
    MUL
    ADDB cursor
    LDX #neighbors
    ABX
    LDAA 0,X
    CMPA loop_history
    BEQ loop_closed
    INC loop_direction
    LDAA loop_direction
    CMPA #4
    BNE loop_closing_direction
loop_close_done:
    RTS
loop_closed:
    STAA loop_to
    JSR challenge_step
    LDAA cursor
    STAA loop_from
    JSR loop_connect
    LDAA #4
    STAA phase
    JSR game_bonus
    JSR challenge_invalidate
    JMP grid_changed
game_aux:
    CMPA #1
    BNE loop_restart
    JMP loop_undo
loop_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    BEQ loop_tile_done
    STAA loop_value
    LDX #loop_marks
    ABX
    TST 0,X
    BEQ loop_unmarked_tile
    LDX #loop_paths
    ABX
    LDAB 0,X
    LDAA loop_value
    CMPA #2
    BCS loop_path_tile
    SUBA #2
    ASLA
    ASLA
    ASLA
    ASLA
    ABA
    TAB
    LDX #loop_gate_sprites
    ABX
    LDAA 0,X
    RTS
loop_path_tile:
    TBA
    ADDA #6
loop_tile_done:
    RTS
loop_unmarked_tile:
    LDAA loop_value
    CMPB challenge_cells
    BEQ loop_bonus_tile
    CMPB challenge_cells + 1
    BNE loop_tile_done
loop_bonus_tile:
    LDAA #LOOP_BONUS_SPRITE
    RTS
game_bonus:
    CLR challenge_bonus
    LDAB challenge_cells
    LDX #loop_marks
    ABX
    TST 0,X
    BEQ loop_bonus_second
    JSR challenge_touch
loop_bonus_second:
    LDAB challenge_cells + 1
    LDX #loop_marks
    ABX
    TST 0,X
    BEQ loop_bonus_done
    JSR challenge_touch
loop_bonus_done:
    RTS
game_render:
    TST hud_ready
    BNE loop_render_board
    JSR hud_begin
    LDX #framebuffer
    STX unpack_dest
    LDX #loop_panel_art
    JSR puzzle_unpack
    JSR dirty_all
    LDX #loop_hud_cache
    LDAA #255
    LDAB #9
loop_clear_hud_cache:
    STAA 0,X
    INX
    DECB
    BNE loop_clear_hud_cache
    CLR paint_index
loop_panel_cache:
    LDAB paint_index
    LDX #view_cells
    ABX
    LDAA 0,X
    CMPA #255
    BNE loop_panel_next
    LDX #tile_cache
    ABX
    CLR 0,X
loop_panel_next:
    INC paint_index
    LDAA paint_index
    CMPA #112
    BNE loop_panel_cache
loop_render_board:
    JSR paint_board
    JSR visual_hud
    LDD challenge_moves
    LDX #loop_hud_used
    JSR hud_number
    LDD challenge_par
    LDX #loop_hud_par
    JSR hud_number
    CLRA
    LDAB challenge_bonus
    BITB #1
    BEQ loop_bonus_count_second
    INCA
loop_bonus_count_second:
    BITB #2
    BEQ loop_bonus_count_done
    INCA
loop_bonus_count_done:
    TAB
    CLRA
    LDX #loop_hud_bonus
    JSR hud_number
    CLR loop_icon_index
loop_gate_legend:
    LDAA #4
    STAA paint_band
    LDAB loop_icon_index
    LDX #loop_gate_x
    ABX
    LDAB 0,X
    LDAA loop_icon_index
    ADDA #2
    JSR loop_draw_icon
    LDAA #5
    STAA paint_band
    LDAA loop_icon_index
    INCA
    CMPA loop_next
    BCS loop_gate_passed
    BEQ loop_gate_next
    CLRA
    BRA loop_gate_state
loop_gate_passed:
    LDAA #2
    BRA loop_gate_state
loop_gate_next:
    LDAA #1
loop_gate_state:
    LDAB loop_icon_index
    LDX #loop_gate_x
    ABX
    LDAB 0,X
    JSR loop_draw_state
    INC loop_icon_index
    LDAA loop_icon_index
    CMPA #3
    BNE loop_gate_legend
    LDAA #7
    STAA paint_band
    LDAB #142
    LDAA challenge_bonus
    BITA #1
    JSR loop_bonus_icon
    LDAB #168
    LDAA challenge_bonus
    BITA #2
    JSR loop_bonus_icon
    LDX #loop_draw_label
    TST grid_stat
    BNE loop_draw_action
    LDX #loop_close_label
loop_draw_action:
    LDAA #28
    LDAB #7
    JMP paint_text
; Z indicates that this bonus jewel has not yet been collected.
loop_bonus_icon:
    BEQ loop_bonus_missing
    LDAA #2
    BRA loop_draw_state
loop_bonus_missing:
    LDAA #LOOP_BONUS_SPRITE
    BRA loop_draw_icon
loop_draw_state:
    STAB paint_x
    LDAB #16
    MUL
    ADDD #loop_state_art
    STD paint_source
    BRA loop_blit_icon
; A = 16x8 sprite, B = physical x; paint_band is set by the caller.
loop_draw_icon:
    STAB paint_x
    LDAB #16
    MUL
    ADDD #tiles + 8
    STD paint_source
loop_blit_icon:
    JSR paint_address
    LDAA #16
    STAA paint_count
    CLR paint_id
    JMP paint_blit

.section .bss, bss
loop_marks: .space 36
loop_paths: .space 36
loop_history: .space 36
loop_directions: .space 36
loop_next: .space 1
loop_cell: .space 1
loop_pointer: .space 2
loop_direction: .space 1
loop_from: .space 1
loop_to: .space 1
loop_mask: .space 1
loop_letter: .space 2
loop_value: .space 1
loop_icon_index: .space 1
loop_hud_cache: .space 9
.section .data, data
loop_bits: .byte 1,2,4,8
loop_opposite: .byte 2,1,8,4
loop_gate_x: .byte 132,150,168
loop_hud_used:
    .byte 168,1,4,0,0,0
    .word loop_hud_cache,0
loop_hud_par:
    .byte 168,2,4,0,0,0
    .word loop_hud_cache + 3,0
loop_hud_bonus:
    .byte 171,6,1,0,0,0
    .word loop_hud_cache + 6,0
