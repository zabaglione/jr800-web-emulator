; SPDX-License-Identifier: MIT
.global hex_charge
.global hex_result
.global hex_maps
.global hex_you_gap
.global hex_cpu_gap
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR hex_result
    CLR selection_active
    LDAA #1
    STAA hex_charge
    LDAA #14
    STAA cursor
    JMP hex_refresh
game_update:
    LDAA input_event
    BITA #16
    BNE hex_action
    JSR grid_move
    CMPB #255
    BEQ hex_idle
    STAB cursor
    JMP grid_changed
hex_action:
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    TST selection_active
    BNE hex_conversion
    TSTA
    BEQ hex_valid
    RTS
hex_conversion:
    CMPA #2
    BNE hex_idle
    TST hex_charge
    BEQ hex_idle
    JSR hex_support
    LDAA hex_adjacent
    CMPA #2
    BCS hex_idle
hex_valid:
    JSR grid_snapshot
    LDAA hex_charge
    STAA hex_saved_charge
    TST selection_active
    BEQ hex_place
    CLR hex_charge
hex_place:
    CLR selection_active
    LDAB cursor
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    JSR grid_count_move
    JSR hex_analyze
    TST hex_you_gap
    BEQ hex_won
    JSR hex_cpu
    JSR hex_refresh
    TST hex_cpu_gap
    BEQ hex_lost
hex_idle:
    RTS
hex_won:
    LDAA #1
    STAA hex_result
    LDAA #4
    STAA phase
    RTS
hex_lost:
    LDAA #2
    STAA hex_result
    LDAA #5
    STAA phase
    RTS
hex_support:
    LDAA cursor
    LDAB #6
    MUL
    ADDD #hex_links
    STD hex_link_pointer
    CLR hex_direction
    CLR hex_adjacent
hex_support_loop:
    LDX hex_link_pointer
    LDAB hex_direction
    ABX
    LDAB 0,X
    CMPB #255
    BEQ hex_support_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE hex_support_next
    INC hex_adjacent
hex_support_next:
    INC hex_direction
    LDAA hex_direction
    CMPA #6
    BNE hex_support_loop
    RTS
; Four edge distance fields are needed only when choosing the CPU move.
hex_analyze:
    CLR hex_edge
hex_analyze_loop:
    JSR hex_path
    INC hex_edge
    LDAA hex_edge
    CMPA #4
    BNE hex_analyze_loop
    JMP hex_gaps
; A displayed position only needs top->bottom and left->right results.
hex_refresh:
    CLR hex_edge
    JSR hex_path
    LDAA #2
    STAA hex_edge
    JSR hex_path
hex_gaps:
    LDAA #255
    STAA hex_cpu_gap
    STAA hex_you_gap
    CLRB
    LDX #hex_maps + 30
hex_cpu_gap_loop:
    LDAA 0,X
    CMPA hex_cpu_gap
    BCC hex_cpu_gap_next
    STAA hex_cpu_gap
hex_cpu_gap_next:
    INX
    INCB
    CMPB #6
    BNE hex_cpu_gap_loop
    LDX #hex_maps + 72 + 5
    STX hex_gap_pointer
    CLR hex_scan
hex_you_gap_loop:
    LDX hex_gap_pointer
    LDAA 0,X
    CMPA hex_you_gap
    BCC hex_you_gap_next
    STAA hex_you_gap
hex_you_gap_next:
    LDD hex_gap_pointer
    ADDD #6
    STD hex_gap_pointer
    INC hex_scan
    LDAA hex_scan
    CMPA #6
    BNE hex_you_gap_loop
    LDAA hex_you_gap
    STAA grid_stat
    RTS
; 0/1 deque traversal, 36 vertices. Own cells cost 0, empty 1, enemy blocked.
; A vertex enters at most twice; a 128-slot ring leaves ample capacity.
hex_path:
    LDAA hex_edge
    LDAB #36
    MUL
    ADDD #hex_maps
    STD hex_map_pointer
    LDAA hex_edge
    LDAB #36
    MUL
    ADDD #hex_sources
    STD hex_source_pointer
    LDAA #2
    LDAB hex_edge
    CMPB #2
    BCS hex_path_owner
    LDAA #1
hex_path_owner:
    STAA hex_owner
    CLR hex_queue_head
    CLR hex_queue_tail
    CLR hex_queue_count
    CLR hex_scan
hex_path_init:
    LDAB hex_scan
    LDX #hex_visited
    ABX
    CLR 0,X
    LDX #board
    ABX
    LDAA 0,X
    BEQ hex_cost_empty
    CMPA hex_owner
    BEQ hex_cost_own
    LDAA #255
    BRA hex_cost_ready
hex_cost_empty:
    LDAA #1
    BRA hex_cost_ready
hex_cost_own:
    CLRA
hex_cost_ready:
    LDX #hex_cost
    ABX
    STAA 0,X
    STAA hex_new_distance
    LDX hex_source_pointer
    ABX
    TST 0,X
    BNE hex_start_cell
    LDAA #255
    BRA hex_init_distance
hex_start_cell:
    LDAA hex_new_distance
    CMPA #255
    BEQ hex_init_distance
    TSTA
    BEQ hex_seed_front
    JSR hex_push_back
    BRA hex_seed_ready
hex_seed_front:
    JSR hex_push_front
hex_seed_ready:
    LDAA hex_new_distance
    LDAB hex_scan
hex_init_distance:
    LDX hex_map_pointer
    ABX
    STAA 0,X
    INC hex_scan
    LDAA hex_scan
    CMPA #36
    BNE hex_path_init
hex_path_next_node:
    TST hex_queue_count
    BNE hex_pop_node
    RTS
hex_pop_node:
    JSR hex_pop_front
    LDX #hex_visited
    ABX
    TST 0,X
    BNE hex_path_next_node
    STAB hex_node
    LDX hex_map_pointer
    ABX
    LDAA 0,X
    STAA hex_best_distance
hex_visit_node:
    LDX #hex_visited
    ABX
    LDAA #1
    STAA 0,X
    LDAA hex_node
    LDAB #6
    MUL
    ADDD #hex_links
    STD hex_link_pointer
    CLR hex_direction
hex_relax:
    LDX hex_link_pointer
    LDAB hex_direction
    ABX
    LDAB 0,X
    CMPB #255
    BEQ hex_relax_next
    LDX #hex_cost
    ABX
    LDAA 0,X
    CMPA #255
    BEQ hex_relax_next
    ADDA hex_best_distance
    LDX hex_map_pointer
    ABX
    CMPA 0,X
    BCC hex_relax_next
    STAA 0,X
    LDX #hex_cost
    ABX
    TST 0,X
    BEQ hex_relax_front
    JSR hex_push_back
    BRA hex_relax_next
hex_relax_front:
    JSR hex_push_front
hex_relax_next:
    INC hex_direction
    LDAA hex_direction
    CMPA #6
    BNE hex_relax
    JSR input_poll
    JMP hex_path_next_node
hex_push_front:
    STAB hex_queue_value
    LDAA hex_queue_head
    DECA
    ANDA #127
    STAA hex_queue_head
    TAB
    LDX #hex_queue
    ABX
    LDAA hex_queue_value
    STAA 0,X
    INC hex_queue_count
    RTS
hex_push_back:
    STAB hex_queue_value
    LDAB hex_queue_tail
    LDX #hex_queue
    ABX
    LDAA hex_queue_value
    STAA 0,X
    LDAA hex_queue_tail
    INCA
    ANDA #127
    STAA hex_queue_tail
    INC hex_queue_count
    RTS
hex_pop_front:
    LDAB hex_queue_head
    LDX #hex_queue
    ABX
    LDAB 0,X
    LDAA hex_queue_head
    INCA
    ANDA #127
    STAA hex_queue_head
    DEC hex_queue_count
    RTS
hex_cpu:
    CLR hex_scan
    CLR hex_best_rank
    LDAA #255
    STAA hex_best_cell
hex_cpu_candidate:
    LDAB hex_scan
    LDX #board
    ABX
    TST 0,X
    BEQ hex_cpu_empty
    JMP hex_cpu_next
hex_cpu_empty:
    LDX #hex_maps
    ABX
    LDAA 0,X
    ADDA 36,X
    BCS hex_attack_far
    CMPA #30
    BLS hex_attack_ready
hex_attack_far:
    LDAA #30
hex_attack_ready:
    STAA hex_attack
    LDAA 72,X
    ADDA 108,X
    BCS hex_defend_far
    CMPA #30
    BLS hex_defend_ready
hex_defend_far:
    LDAA #30
hex_defend_ready:
    STAA hex_defend
    LDAA hex_attack
    CMPA #2
    BNE hex_no_cpu_win
    LDAA #255
    BRA hex_rank_ready
hex_no_cpu_win:
    LDAA hex_defend
    CMPA #2
    BNE hex_distance_rank
    LDAA #240
    BRA hex_rank_ready
hex_distance_rank:
    LDAA hex_attack
    ASLA
    ASLA
    STAA hex_rank
    LDAA hex_defend
    ASLA
    ADDA hex_rank
    STAA hex_rank
    LDAA #181
    SUBA hex_rank
    LDAB hex_scan
    LDX #hex_weights
    ABX
    ADDA 0,X
hex_rank_ready:
    CMPA hex_best_rank
    BLS hex_cpu_next
    STAA hex_best_rank
    LDAA hex_scan
    STAA hex_best_cell
hex_cpu_next:
    INC hex_scan
    LDAA hex_scan
    CMPA #36
    BEQ hex_cpu_chosen
    JMP hex_cpu_candidate
hex_cpu_chosen:
    LDAB hex_best_cell
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    RTS
game_aux:
    CMPA #1
    BNE hex_undo
    TST hex_charge
    BEQ hex_aux_done
    LDAA #1
    STAA selection_active
hex_aux_done:
    RTS
hex_undo:
    TST undo_valid
    BEQ hex_aux_done
    JSR grid_restore
    LDAA hex_saved_charge
    STAA hex_charge
    CLR selection_active
    CLR hex_result
    JMP hex_refresh
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR grid_render
    LDX #hex_left_label
    LDAA #4
    LDAB #3
    JSR paint_text
    LDX #hex_right_label
    LDAA #118
    LDAB #3
    JSR paint_text
    LDX #hex_top_label
    LDAA #70
    CLRB
    JSR paint_text
    LDX #hex_bottom_label
    LDAA #70
    LDAB #7
    JSR paint_text
    TST hex_result
    BNE hex_result_display
    LDX #hex_relay_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDAA #174
    STAA paint_x
    LDAA #5
    STAA paint_band
    LDAA hex_charge
    JSR paint_number
    LDX #hex_space_label
    TST selection_active
    BEQ hex_action_label
    LDX #hex_convert_label
hex_action_label:
    LDAA #132
    LDAB #6
    JMP paint_text
hex_result_display:
    LDX #hex_blank_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #hex_win_label
    LDAA hex_result
    CMPA #1
    BEQ hex_result_label
    LDX #hex_lose_label
hex_result_label:
    LDAA #138
    LDAB #4
    JMP paint_text
.section .bss, bss
hex_charge: .space 1
hex_saved_charge: .space 1
hex_result: .space 1
hex_maps: .space 144
hex_visited: .space 36
hex_cost: .space 36
hex_you_gap: .space 1
hex_cpu_gap: .space 1
hex_edge: .space 1
hex_map_pointer: .space 2
hex_source_pointer: .space 2
hex_link_pointer: .space 2
hex_gap_pointer: .space 2
hex_owner: .space 1
hex_scan: .space 1
hex_new_distance: .space 1
hex_best_distance: .space 1
hex_node: .space 1
hex_direction: .space 1
hex_adjacent: .space 1
hex_attack: .space 1
hex_defend: .space 1
hex_rank: .space 1
hex_best_rank: .space 1
hex_best_cell: .space 1
hex_queue: .space 128
hex_queue_head: .space 1
hex_queue_tail: .space 1
hex_queue_count: .space 1
hex_queue_value: .space 1
.section .data, data
hex_left_label: .byte 62,0
hex_right_label: .byte 60,0
hex_top_label: .byte 86,0
hex_bottom_label: .byte 94,0
hex_relay_label: .byte 82,76,89,0
hex_space_label: .byte 32,83,80,65,67,69,32,32,32,32,0
hex_convert_label: .byte 67,79,78,86,69,82,84,32,32,32,0
hex_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
hex_win_label: .byte 89,79,85,32,87,73,78,0
hex_lose_label: .byte 67,80,85,32,87,73,78,0
