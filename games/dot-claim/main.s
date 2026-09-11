; SPDX-License-Identifier: MIT
.global dot_cursor
.global dot_player_score
.global dot_cpu_score
.global dot_captured
.global dot_edge
.global dot_side
.global dot_claim
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR dot_cursor
    JSR dot_set_cursor
    JMP dot_counts
game_update:
    LDAA input_event
    BITA #16
    BNE dot_action
    LDX #dot_neighbors
    BITA #1
    BNE dot_direction
    LDX #dot_neighbors + 31
    BITA #2
    BNE dot_direction
    LDX #dot_neighbors + 62
    BITA #4
    BNE dot_direction
    LDX #dot_neighbors + 93
    BITA #8
    BEQ dot_idle
dot_direction:
    LDAB dot_cursor
    ABX
    LDAA 0,X
    STAA dot_cursor
    JSR dot_set_cursor
    JMP grid_changed
dot_action:
    LDAB dot_cursor
    LDX #board
    ABX
    TST 0,X
    BNE dot_idle
    JSR grid_snapshot
    LDAA dot_cursor
    STAA dot_edge
    LDAA #1
    STAA dot_side
    JSR dot_claim
    JSR grid_count_move
    TST dot_captured
    BNE dot_finish_turn
dot_cpu_turn:
    JSR dot_choose
    LDAA dot_best_edge
    CMPA #255
    BEQ dot_finish_turn
    STAA dot_edge
    LDAA #2
    STAA dot_side
    JSR dot_claim
    JSR input_poll
    TST dot_captured
    BNE dot_cpu_turn
dot_finish_turn:
    JSR dot_counts
    TST grid_stat
    BNE dot_idle
    LDAA #4
    STAA phase
    LDAA dot_player_score
    CMPA dot_cpu_score
    BCC dot_idle
    LDAA #5
    STAA phase
dot_idle:
    RTS
dot_set_cursor:
    LDAB dot_cursor
    LDX #dot_cells
    ABX
    LDAA 0,X
    STAA cursor
    RTS
; Count occupied edges around box B. Independent of edge ownership.
dot_count_box:
    ASLB
    ASLB
    LDX #dot_box_edges
    ABX
    STX dot_box_pointer
    CLR dot_count
    CLR dot_count_index
dot_count_edges:
    LDX dot_box_pointer
    LDAB dot_count_index
    ABX
    LDAB 0,X
    LDX #board
    ABX
    TST 0,X
    BEQ dot_count_next
    INC dot_count
dot_count_next:
    INC dot_count_index
    LDAA dot_count_index
    CMPA #4
    BNE dot_count_edges
    LDAA dot_count
    RTS
dot_adjacent:
    LDAA dot_edge
    ASLA
    ADDA dot_adj_index
    TAB
    LDX #dot_edge_boxes
    ABX
    LDAB 0,X
    STAB dot_box
    RTS
dot_claim:
    LDAB dot_edge
    LDX #board
    ABX
    LDAA dot_side
    STAA 0,X
    CLR dot_captured
    CLR dot_adj_index
dot_claim_adj:
    JSR dot_adjacent
    CMPB #255
    BEQ dot_claim_next
    LDX #board + 31
    ABX
    TST 0,X
    BNE dot_claim_next
    JSR dot_count_box
    CMPA #4
    BNE dot_claim_next
    LDAB dot_box
    LDX #board + 31
    ABX
    LDAA dot_side
    STAA 0,X
    INC dot_captured
dot_claim_next:
    INC dot_adj_index
    LDAA dot_adj_index
    CMPA #2
    BNE dot_claim_adj
    RTS
dot_choose:
    LDAA #255
    STAA dot_best_edge
    CLR dot_best_score
    CLR dot_edge
dot_candidate:
    LDAB dot_edge
    LDX #board
    ABX
    TST 0,X
    BNE dot_candidate_next
    TST stage
    BEQ dot_first_edge
    LDAA #2
    STAA 0,X
    LDAA #32
    STAA dot_score
    CLR dot_adj_index
dot_rate_adj:
    JSR dot_adjacent
    CMPB #255
    BEQ dot_rate_next
    JSR dot_count_box
    CMPA #4
    BNE dot_rate_risk
    LDAA dot_score
    ADDA #64
    STAA dot_score
    BRA dot_rate_next
dot_rate_risk:
    CMPA #3
    BNE dot_rate_next
    LDAA stage
    CMPA #2
    BNE dot_rate_next
    LDAA dot_score
    SUBA #12
    STAA dot_score
dot_rate_next:
    INC dot_adj_index
    LDAA dot_adj_index
    CMPA #2
    BNE dot_rate_adj
    LDAB dot_edge
    LDX #board
    ABX
    CLR 0,X
    LDAA dot_score
    CMPA dot_best_score
    BLS dot_candidate_next
    STAA dot_best_score
    LDAA dot_edge
    STAA dot_best_edge
dot_candidate_next:
    JSR input_poll
    INC dot_edge
    LDAA dot_edge
    CMPA #31
    BNE dot_candidate
    RTS
dot_first_edge:
    LDAA dot_edge
    STAA dot_best_edge
    RTS
dot_counts:
    CLR dot_player_score
    CLR dot_cpu_score
    LDAA #12
    STAA grid_stat
    LDX #board + 31
    LDAB #12
dot_score_loop:
    LDAA 0,X
    BEQ dot_score_next
    DEC grid_stat
    CMPA #1
    BNE dot_score_cpu
    INC dot_player_score
    BRA dot_score_next
dot_score_cpu:
    INC dot_cpu_score
dot_score_next:
    INX
    DECB
    BNE dot_score_loop
    RTS
game_aux:
    CMPA #1
    BNE dot_restart
    JSR grid_restore
    LDAB cursor
    LDX #dot_ids
    ABX
    LDAA 0,X
    STAA dot_cursor
    JMP dot_counts
dot_restart:
    JMP game_start
grid_value:
    LDX #dot_types
    ABX
    LDAA 0,X
    STAA dot_tile_type
    LDX #dot_ids
    ABX
    LDAB 0,X
    TSTA
    BEQ dot_node_tile
    CMPA #3
    BEQ dot_box_tile
    LDX #board
    ABX
    TST 0,X
    BEQ dot_edge_blank
    LDAA #17
    BRA dot_edge_kind
dot_edge_blank:
    LDAA #16
dot_edge_kind:
    LDAB dot_tile_type
    CMPB #2
    BNE dot_tile_done
    ADDA #2
dot_tile_done:
    RTS
dot_box_tile:
    LDX #board + 31
    ABX
    LDAA 0,X
    ADDA #20
    RTS
dot_node_tile:
    ASLB
    ASLB
    LDX #dot_node_edges
    ABX
    STX dot_node_pointer
    CLR dot_node_value
    LDAA #1
    STAA dot_node_bit
    CLR dot_node_index
dot_node_loop:
    LDX dot_node_pointer
    LDAB dot_node_index
    ABX
    LDAB 0,X
    CMPB #255
    BEQ dot_node_next
    LDX #board
    ABX
    TST 0,X
    BEQ dot_node_next
    LDAA dot_node_bit
    ORAA dot_node_value
    STAA dot_node_value
dot_node_next:
    ASL dot_node_bit
    INC dot_node_index
    LDAA dot_node_index
    CMPA #4
    BNE dot_node_loop
    LDAA dot_node_value
    RTS
game_render:
    JSR grid_render
    LDX #dot_you_label
    LDAA #66
    CLRB
    JSR paint_text
    LDAA #78
    STAA paint_x
    CLR paint_band
    LDAA dot_player_score
    JSR paint_number
    LDX #dot_cpu_label
    LDAA #108
    CLRB
    JSR paint_text
    LDAA #120
    STAA paint_x
    CLR paint_band
    LDAA dot_cpu_score
    JSR paint_number
    LDX #dot_blank_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #dot_pick_label
    LDAA #138
    LDAB #5
    JMP paint_text
.section .bss, bss
dot_cursor: .space 1
dot_player_score: .space 1
dot_cpu_score: .space 1
dot_edge: .space 1
dot_side: .space 1
dot_captured: .space 1
dot_box_pointer: .space 2
dot_count: .space 1
dot_count_index: .space 1
dot_adj_index: .space 1
dot_box: .space 1
dot_best_edge: .space 1
dot_best_score: .space 1
dot_score: .space 1
dot_tile_type: .space 1
dot_node_pointer: .space 2
dot_node_value: .space 1
dot_node_bit: .space 1
dot_node_index: .space 1
.section .data, data
dot_you_label: .byte 89,0
dot_cpu_label: .byte 67,0
dot_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
dot_pick_label: .byte 80,73,67,75,0
