; SPDX-License-Identifier: MIT
.global mine_armed
.global mine_count
.global mine_flags
.global mine_blast
.global mine_generate
.global mine_reveal
.section .text, code
game_start:
    JSR grid_reset
    CLR mine_armed
    CLR mine_flags
    CLR selection_active
    LDAA #255
    STAA mine_blast
    LDAB stage
    LDX #mine_totals
    ABX
    LDAA 0,X
    STAA mine_count
    LDAA #98
    SUBA mine_count
    STAA grid_stat
    JSR challenge_start
    LDAB stage
    LDX #mine_starts
    ABX
    LDAA 0,X
    STAA cursor
    JSR mine_generate
    CLR mine_changed
    LDAB cursor
    JMP mine_reveal
game_update:
    LDAA input_event
    BITA #16
    BNE mine_action
    JSR grid_move
    CMPB #255
    BNE mine_skip_1
    JMP mine_idle
mine_skip_1:
    STAB cursor
    JMP grid_changed
mine_action:
    TST selection_active
    BNE mine_flag
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    BITA #64
    BEQ mine_skip_2
    JMP mine_idle
mine_skip_2:
    TST mine_armed
    BNE mine_open
    JSR mine_generate
mine_open:
    CLR mine_changed
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    BITA #32
    BEQ mine_single
    JSR mine_chord
    BRA mine_after_open
mine_single:
    JSR mine_reveal
mine_after_open:
    TST mine_changed
    BNE mine_skip_3
    JMP mine_idle
mine_skip_3:
    JSR challenge_step
    JSR grid_count_move
    TST grid_stat
    BEQ mine_skip_4
    JMP mine_idle
mine_skip_4:
    JMP mine_victory
mine_flag:
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    BITA #32
    BEQ mine_skip_5
    JMP mine_idle
mine_skip_5:
    BITA #64
    BNE mine_unflag
    LDAA mine_flags
    CMPA mine_count
    BCS mine_skip_6
    JMP mine_idle
mine_skip_6:
    INC mine_flags
    LDAA 0,X
    ORAA #64
    STAA 0,X
    JSR challenge_step
    JMP grid_count_move
mine_unflag:
    ANDA #191
    STAA 0,X
    DEC mine_flags
    JSR challenge_step
    JMP grid_count_move
mine_idle:
    RTS
; Fixed authored mine bitmap; clues are computed on the JR-800.
mine_generate:
    LDX #mine_queue
    JSR challenge_load
    CLR mine_cell
mine_bitmap_cell:
    LDAB mine_cell
    LSRB
    LSRB
    LSRB
    LDX #mine_queue
    ABX
    LDAA 0,X
    LDAB mine_cell
    ANDB #7
    BEQ mine_bitmap_bit
mine_bitmap_shift:
    LSRA
    DECB
    BNE mine_bitmap_shift
mine_bitmap_bit:
    ANDA #1
    ASLA
    ASLA
    ASLA
    ASLA
    LDAB mine_cell
    LDX #board
    ABX
    STAA 0,X
    INC mine_cell
    LDAA mine_cell
    CMPA #98
    BNE mine_bitmap_cell
    CLR mine_cell
mine_number_cell:
    LDAB mine_cell
    LDX #board
    ABX
    LDAA 0,X
    ANDA #127
    STAA 0,X
    BITA #16
    BNE mine_number_next
    LDAA mine_cell
    LDAB #8
    MUL
    ADDD #mine_links
    STD mine_neighbor_pointer
    CLR mine_dir
    CLR mine_number
mine_number_adj:
    LDX mine_neighbor_pointer
    LDAB mine_dir
    ABX
    LDAB 0,X
    CMPB #255
    BEQ mine_number_adj_next
    LDX #board
    ABX
    LDAA 0,X
    BITA #16
    BEQ mine_number_adj_next
    INC mine_number
mine_number_adj_next:
    INC mine_dir
    LDAA mine_dir
    CMPA #8
    BNE mine_number_adj
    LDAB mine_cell
    LDX #board
    ABX
    LDAA 0,X
    ORAA mine_number
    STAA 0,X
mine_number_next:
    JSR input_poll
    INC mine_cell
    LDAA mine_cell
    CMPA #98
    BNE mine_number_cell
    LDAA #1
    STAA mine_armed
    RTS
; Reveal a cell and flood its zero-valued region. Mark on enqueue, never twice.
mine_reveal:
    CLR mine_head
    CLR mine_tail
    JSR mine_enqueue
mine_visit:
    LDAA phase
    CMPA #5
    BEQ mine_reveal_done
    LDAB mine_head
    CMPB mine_tail
    BEQ mine_reveal_done
    LDX #mine_queue
    ABX
    LDAB 0,X
    STAB mine_cell
    LDX #board
    ABX
    LDAA 0,X
    ANDA #15
    BNE mine_visit_next
    LDAA mine_cell
    LDAB #8
    MUL
    ADDD #mine_links
    STD mine_neighbor_pointer
    CLR mine_dir
mine_flood_adj:
    LDX mine_neighbor_pointer
    LDAB mine_dir
    ABX
    LDAB 0,X
    CMPB #255
    BEQ mine_flood_next
    JSR mine_enqueue
mine_flood_next:
    INC mine_dir
    LDAA mine_dir
    CMPA #8
    BNE mine_flood_adj
mine_visit_next:
    JSR input_poll
    INC mine_head
    BRA mine_visit
mine_reveal_done:
    RTS
mine_enqueue:
    LDX #board
    ABX
    LDAA 0,X
    BITA #96
    BNE mine_reveal_done
    BITA #16
    BNE mine_hit
    ORAA #32
    STAA 0,X
    TBA
    LDAB mine_tail
    LDX #mine_queue
    ABX
    STAA 0,X
    INC mine_tail
    DEC grid_stat
    LDAA #1
    STAA mine_changed
    RTS
mine_hit:
    STAB mine_blast
    LDAA #5
    STAA phase
    LDAA #1
    STAA mine_changed
    RTS
mine_chord:
    LDAA 0,X
    ANDA #15
    STAA mine_chord_number
    LDAA cursor
    LDAB #8
    MUL
    ADDD #mine_links
    STD mine_chord_pointer
    CLR mine_chord_index
    CLR mine_chord_flags
mine_chord_count:
    JSR mine_chord_neighbor
    CMPB #255
    BEQ mine_chord_count_next
    LDX #board
    ABX
    LDAA 0,X
    BITA #64
    BEQ mine_chord_count_next
    INC mine_chord_flags
mine_chord_count_next:
    INC mine_chord_index
    LDAA mine_chord_index
    CMPA #8
    BNE mine_chord_count
    LDAA mine_chord_flags
    CMPA mine_chord_number
    BNE mine_chord_done
    CLR mine_chord_index
mine_chord_open:
    JSR mine_chord_neighbor
    CMPB #255
    BEQ mine_chord_open_next
    JSR mine_reveal
    LDAA phase
    CMPA #5
    BEQ mine_chord_done
mine_chord_open_next:
    INC mine_chord_index
    LDAA mine_chord_index
    CMPA #8
    BNE mine_chord_open
mine_chord_done:
    RTS
mine_chord_neighbor:
    LDX mine_chord_pointer
    LDAB mine_chord_index
    ABX
    LDAB 0,X
    RTS
mine_victory:
    CLR selection_active
    LDAA #4
    STAA phase
    RTS
game_aux:
    CMPA #1
    BNE mine_restart
    LDAA #1
    STAA selection_active
    RTS
mine_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    STAA mine_tile_value
    LDAA phase
    CMPA #5
    BNE mine_normal_tile
    LDAA mine_tile_value
    BITA #16
    BEQ mine_wrong_flag
    LDAA #12
    CMPB mine_blast
    BNE mine_tile_done
    LDAA #13
    RTS
mine_wrong_flag:
    BITA #64
    BEQ mine_normal_tile
    LDAA #14
    RTS
mine_normal_tile:
    LDAA mine_tile_value
    BITA #64
    BEQ mine_open_tile
    LDAA #11
    RTS
mine_open_tile:
    BITA #32
    BEQ mine_covered
    ANDA #15
    RTS
mine_covered:
    LDAA #10
mine_tile_done:
    RTS
game_bonus:
    CLR challenge_bonus
    LDAA mine_flags
    CMPA mine_count
    BNE mine_bonus_done
    INC challenge_bonus
mine_bonus_done:
    RTS
game_render:
    JSR paint_board
    JMP visual_hud

.section .bss, bss
mine_armed: .space 1
mine_count: .space 1
mine_flags: .space 1
mine_blast: .space 1
mine_changed: .space 1
mine_placed: .space 1
mine_cell: .space 1
mine_dir: .space 1
mine_number: .space 1
mine_neighbor_pointer: .space 2
mine_queue: .space 98
mine_head: .space 1
mine_tail: .space 1
mine_chord_index: .space 1
mine_chord_number: .space 1
mine_chord_flags: .space 1
mine_chord_pointer: .space 2
mine_tile_value: .space 1
.section .data, data
mine_f_label: .byte 70,0
mine_m_label: .byte 77,0
mine_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
mine_open_label: .byte 79,80,69,78,0
mine_flag_label: .byte 70,76,65,71,0
