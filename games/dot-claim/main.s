; SPDX-License-Identifier: MIT
.global dot_cursor
.global dot_row
.global dot_column
.global dot_player_score
.global dot_cpu_score
.global dot_captured
.global dot_edge
.global dot_side
.global dot_claim
.global dot_cpu_active
.global dot_flash_visible
.global dot_flash_left
.global dot_flash_clock
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR dot_cursor
    CLR dot_cpu_active
    JSR dot_sync_cursor
    JMP dot_counts
game_update:
    JSR cursor_blink_tick
    TST dot_cpu_active
    BEQ dot_controls
    JMP dot_flash_update
dot_controls:
    LDAA input_event
    BITA #16
    BEQ dot_directions
    JMP dot_action
dot_directions:
    BITA #1
    BNE dot_up
    BITA #2
    BNE dot_down
    BITA #4
    BNE dot_left
    BITA #8
    BEQ dot_control_idle
dot_right:
    LDAB dot_cursor
    LDX #dot_columns
    ABX
    LDAA 0,X
    ADDA #2
    CMPA #9
    BCC dot_control_idle
    STAA dot_column
    BRA dot_select
dot_left:
    LDAB dot_cursor
    LDX #dot_columns
    ABX
    LDAA 0,X
    SUBA #2
    BCS dot_control_idle
    STAA dot_column
    BRA dot_select
dot_up:
    TST dot_row
    BEQ dot_control_idle
    DEC dot_row
    BRA dot_select
dot_down:
    LDAA dot_row
    CMPA #6
    BEQ dot_control_idle
    INC dot_row
; Keep the horizontal aim when crossing staggered rows so up/down never drifts.
dot_select:
    LDAA dot_row
    LDAB #9
    MUL
    ADDB dot_column
    LDX #dot_targets
    ABX
    LDAA 0,X
    STAA dot_cursor
    JSR dot_set_cursor
    JMP grid_changed
dot_control_idle:
    RTS
dot_action:
    LDAB dot_cursor
    LDX #board
    ABX
    TST 0,X
    BNE dot_control_idle
    JSR grid_snapshot
    LDAA dot_column
    STAA dot_undo_column
    LDAA dot_cursor
    STAA dot_edge
    LDAA #1
    STAA dot_side
    JSR dot_claim
    JSR grid_count_move
    TST dot_captured
    BEQ dot_cpu_turn
    JMP dot_finish_turn
dot_cpu_turn:
    JSR dot_choose
    LDAA dot_best_edge
    CMPA #255
    BNE dot_cpu_claim
    JMP dot_finish_turn
dot_cpu_claim:
    STAA dot_edge
    LDAA #2
    STAA dot_side
    JSR dot_claim
    JSR dot_counts
    LDAA #1
    STAA dot_cpu_active
    STAA dot_flash_visible
    LDAA #6
    STAA dot_flash_left
    LDAA input_ticks
    STAA dot_flash_clock
    CLR resume_pending
    LDAA #255
    STAA cursor
    JMP grid_changed
; Show every CPU edge for six 10-tick intervals, including extra turns.
; The timer is measured by the JR-800 program; menus freeze the animation.
dot_flash_update:
    LDAA input_ticks
    TST resume_pending
    BEQ dot_flash_elapsed
    STAA dot_flash_clock
    CLR resume_pending
dot_flash_elapsed:
    SUBA dot_flash_clock
    CMPA #10
    BCC dot_flash_tick
    RTS
dot_flash_tick:
    LDAA input_ticks
    STAA dot_flash_clock
    DEC dot_flash_left
    BEQ dot_flash_done
    LDAA dot_flash_visible
    EORA #1
    STAA dot_flash_visible
    JMP grid_changed
dot_flash_done:
    CLR dot_cpu_active
    JSR input_gate
    JSR dot_set_cursor
    JSR grid_changed
    TST dot_captured
    BEQ dot_finish_turn
    JMP dot_cpu_turn
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
dot_sync_cursor:
    LDAB dot_cursor
    LDX #dot_rows
    ABX
    LDAA 0,X
    STAA dot_row
    LDX #dot_columns
    ABX
    LDAA 0,X
    STAA dot_column
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
    TST undo_valid
    BEQ dot_aux_idle
    CLR dot_cpu_active
    JSR grid_restore
    LDAB cursor
    LDX #dot_ids
    ABX
    LDAA 0,X
    STAA dot_cursor
    JSR dot_sync_cursor
    LDAA dot_undo_column
    STAA dot_column
    JMP dot_counts
dot_aux_idle:
    RTS
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
    JSR dot_visible_edge
    TSTA
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
    CMPA #2
    BNE dot_box_visible
    TST dot_cpu_active
    BEQ dot_box_visible
    TST dot_flash_visible
    BNE dot_box_visible
    STAB dot_tile_box
    LDAB dot_edge
    ASLB
    LDX #dot_edge_boxes
    ABX
    LDAB dot_tile_box
    CMPB 0,X
    BEQ dot_box_hidden
    CMPB 1,X
    BNE dot_box_visible
dot_box_hidden:
    CLRA
dot_box_visible:
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
    JSR dot_visible_edge
    TSTA
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
; B = edge. Blink only its pixels, never its logical ownership.
dot_visible_edge:
    LDX #board
    ABX
    LDAA 0,X
    TST dot_cpu_active
    BEQ dot_visible_done
    TST dot_flash_visible
    BNE dot_visible_done
    CMPB dot_edge
    BNE dot_visible_done
    CLRA
dot_visible_done:
    RTS
game_render:
    JSR cursor_blink_prepare
    JSR cursor_blink_board
    JMP visual_hud

.section .bss, bss
dot_cursor: .space 1
dot_row: .space 1
dot_column: .space 1
dot_undo_column: .space 1
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
dot_cpu_active: .space 1
dot_flash_visible: .space 1
dot_flash_left: .space 1
dot_flash_clock: .space 1
dot_tile_box: .space 1
.section .data, data
dot_you_label: .byte 89,0
dot_cpu_label: .byte 67,0
dot_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
dot_pick_label: .byte 80,73,67,75,0

; The JR-800 input timer drives focus blinking; input immediately restores it.
.global cursor_blink_mask
.global cursor_blink_clock
.section .text, code
cursor_blink_prepare:
    TST hud_ready
    BEQ cursor_blink_show
    RTS
cursor_blink_tick:
    TST dot_cpu_active
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
