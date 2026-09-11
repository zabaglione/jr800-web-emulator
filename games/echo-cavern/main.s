; SPDX-License-Identifier: MIT
.global echo_air
.global echo_left
.global echo_score
.global echo_known
.global echo_mapped
.global echo_sonar
.global echo_ping
.section .text, code
game_start:
    JSR grid_reset
    LDAA #15
    STAA cursor
    CLR echo_score
    CLR echo_score + 1
    CLR echo_mapped
    LDAA #3
    STAA echo_left
    LDAA stage
    ANDA #$FC
    STAA echo_temp
    LDAA #64
    SUBA echo_temp
    STAA echo_air
    LDAA stage
    LDAB #98
    MUL
    ADDD #echo_levels
    STD echo_pointer
    CLR echo_index
echo_load:
    LDX echo_pointer
    LDAA 0,X
    INX
    STX echo_pointer
    LDAB echo_index
    LDX #board
    ABX
    STAA 0,X
    LDX #echo_known
    ABX
    CLR 0,X
    INC echo_index
    LDAA echo_index
    CMPA #98
    BNE echo_load
    JMP echo_ping
game_aux:
    CMPA #1
    BEQ echo_sonar
    JMP game_start
echo_sonar:
    LDAA #1
    STAA redraw
    LDAA echo_air
    CMPA #2
    BHI echo_pay_scan
    JMP echo_die
echo_pay_scan:
    SUBA #2
    STAA echo_air
    JMP echo_ping
game_update:
    CLR resume_pending
    LDAA input_event
    BITA #16
    BNE echo_sonar
    JSR grid_move
    CMPB #255
    BNE echo_neighbor
    RTS
echo_neighbor:
    STAB echo_next
    JSR echo_reveal
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE echo_enter
    RTS
echo_enter:
    STAA echo_value
    STAB cursor
    JSR grid_count_move
    LDAA echo_value
    LDAB #1
    CMPA #4
    BNE echo_move_cost
    LDAB #3
echo_move_cost:
    STAB echo_cost
    LDAA echo_air
    CMPA echo_cost
    BHI echo_pay_move
    JMP echo_die
echo_pay_move:
    SUBA echo_cost
    STAA echo_air
    LDAB cursor
    LDX #board
    ABX
    LDAA echo_value
    CMPA #2
    BNE echo_tank
    CLR 0,X
    DEC echo_left
    LDD echo_score
    ADDD #100
    STD echo_score
    RTS
echo_tank:
    CMPA #3
    BNE echo_exit
    CLR 0,X
    LDAA echo_air
    ADDA #24
    CMPA #99
    BLS echo_fill_air
    LDAA #99
echo_fill_air:
    STAA echo_air
    RTS
echo_exit:
    CMPA #5
    BNE echo_done
    TST echo_left
    BNE echo_done
    LDAA echo_air
    LDAB #5
    MUL
    ADDD echo_score
    STD echo_score
    LDAA #4
    STAA phase
    RTS
echo_die:
    CLR echo_air
    LDAA #5
    STAA phase
echo_done:
    RTS
; B = cell. Reveal once; no second framebuffer or repeated map rendering.
echo_reveal:
    LDX #echo_known
    ABX
    TST 0,X
    BNE echo_reveal_done
    INC 0,X
    INC echo_mapped
    LDAA #1
    STAA redraw
echo_reveal_done:
    RTS
; Four-neighbour breadth-first sound wave, range three; walls absorb sound.
echo_ping:
    LDX #echo_distance
    LDAB #98
    LDAA #255
echo_clear_distance:
    STAA 0,X
    INX
    DECB
    BNE echo_clear_distance
    CLR echo_head
    LDAA #1
    STAA echo_tail
    LDAB cursor
    STAB echo_queue
    LDX #echo_distance
    ABX
    CLR 0,X
    JSR echo_reveal
echo_pop:
    LDAB echo_head
    LDX #echo_queue
    ABX
    LDAB 0,X
    STAB echo_scan_cell
    LDX #echo_distance
    ABX
    LDAA 0,X
    INCA
    STAA echo_depth
    CLR echo_direction
echo_neighbor_scan:
    LDAA echo_direction
    LDAB #98
    MUL
    ADDD #neighbors
    XGDX
    LDAB echo_scan_cell
    ABX
    LDAB 0,X
    CMPB #255
    BEQ echo_scan_next
    LDX #echo_distance
    ABX
    LDAA 0,X
    CMPA #255
    BNE echo_scan_next
    LDAA echo_depth
    STAA 0,X
    STAB echo_next
    JSR echo_reveal
    LDAA echo_depth
    CMPA #3
    BEQ echo_scan_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ echo_scan_next
    LDAB echo_tail
    LDX #echo_queue
    ABX
    LDAA echo_next
    STAA 0,X
    INC echo_tail
echo_scan_next:
    INC echo_direction
    LDAA echo_direction
    CMPA #4
    BNE echo_neighbor_scan
    INC echo_head
    LDAA echo_head
    ANDA #3
    BNE echo_scan_more
    JSR input_poll
echo_scan_more:
    LDAA echo_head
    CMPA echo_tail
    BEQ echo_scan_done
    JMP echo_pop
echo_scan_done:
    RTS
grid_value:
    CMPB cursor
    BNE echo_tile_known
    LDAA #7
    RTS
echo_tile_known:
    LDX #echo_known
    ABX
    TST 0,X
    BNE echo_tile_map
    LDAA #6
    RTS
echo_tile_map:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
echo_air: .space 1
echo_left: .space 1
echo_score: .space 2
echo_mapped: .space 1
echo_known: .space 98
echo_distance: .space 98
echo_queue: .space 98
echo_head: .space 1
echo_tail: .space 1
echo_direction: .space 1
echo_scan_cell: .space 1
echo_depth: .space 1
echo_next: .space 1
echo_value: .space 1
echo_cost: .space 1
echo_pointer: .space 2
echo_index: .space 1
echo_temp: .space 1
echo_hud_force: .space 1
echo_old_air: .space 1
echo_old_left: .space 1
echo_old_mapped: .space 1
echo_old_score: .space 2
.section .data, data
echo_air_label: .byte 65,73,82,0
echo_left_label: .byte 76,69,70,84,0
echo_mapped_label: .byte 77,65,80,0
echo_space_label: .byte 83,80,65,67,69,0
