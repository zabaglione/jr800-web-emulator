; SPDX-License-Identifier: MIT
.global ace_stock_pos
.global ace_waste_count
.global ace_first
.global ace_available
.global ace_check
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR selection_active
    CLR ace_stock_pos
    CLR ace_waste_count
    LDAA #28
    STAA grid_stat
    LDAA #24
    STAA cursor
    LDAA stage
    LDAB #52
    MUL
    ADDD #ace_deals
    STD ace_pointer
    CLR ace_index
ace_load:
    LDX ace_pointer
    LDAA 0,X
    INX
    STX ace_pointer
    LDAB ace_index
    LDX #board
    ABX
    STAA 0,X
    INC ace_index
    LDAA ace_index
    CMPA #28
    BNE ace_load
    LDX ace_pointer
    STX ace_stock
    RTS
game_update:
    JSR cursor_blink_tick
    LDAA input_event
    BITA #16
    BNE ace_choose
    JSR grid_move
    CMPB #255
    BEQ ace_idle
    STAB cursor
    JMP grid_changed
ace_choose:
    LDAB cursor
    JSR ace_available
    TSTA
    BEQ ace_idle
    CMPA #13
    BEQ ace_king
    TST selection_active
    BEQ ace_select_first
    STAA ace_second_value
    LDAA cursor
    CMPA ace_first
    BEQ ace_idle
    LDAB ace_first
    JSR ace_available
    TSTA
    BEQ ace_idle
    ADDA ace_second_value
    CMPA #13
    BNE ace_idle
    JSR ace_snapshot
    LDAB ace_first
    JSR ace_take
    LDAB cursor
    JSR ace_take
    BRA ace_taken
ace_select_first:
    LDAA cursor
    STAA ace_first
    LDAA #1
    STAA selection_active
    JMP grid_changed
ace_king:
    JSR ace_snapshot
    LDAB cursor
    JSR ace_take
ace_taken:
    CLR selection_active
    JSR grid_count_move
    JMP ace_check
ace_idle:
    RTS
; B=tableau 0..27 or waste 28, A=rank if selectable, otherwise zero.
ace_available:
    CMPB #28
    BEQ ace_waste
    STAB ace_query
    LDX #board
    ABX
    LDAA 0,X
    BEQ ace_not_available
    STAA ace_query_rank
    CMPB #21
    BCC ace_bottom
    ASLB
    LDX #ace_children
    ABX
    LDAB 1,X
    STAB ace_child
    LDAB 0,X
    LDX #board
    ABX
    TST 0,X
    BNE ace_not_available
    LDAB ace_child
    LDX #board
    ABX
    TST 0,X
    BNE ace_not_available
ace_bottom:
    LDAA ace_query_rank
    RTS
ace_waste:
    LDAB ace_waste_count
    BEQ ace_not_available
    DECB
    LDX #board + 28
    ABX
    LDAA 0,X
    RTS
ace_not_available:
    CLRA
    RTS
ace_take:
    CMPB #28
    BEQ ace_pop_waste
    LDX #board
    ABX
    CLR 0,X
    DEC grid_stat
    RTS
ace_pop_waste:
    DEC ace_waste_count
    LDAB ace_waste_count
    LDX #board + 28
    ABX
    CLR 0,X
    RTS
ace_snapshot:
    LDAA ace_stock_pos
    STAA ace_undo_pos
    LDAA ace_waste_count
    STAA ace_undo_waste
    JMP grid_snapshot
game_aux:
    CMPA #1
    BNE ace_undo
    LDAA ace_stock_pos
    CMPA #24
    BCC ace_aux_done
    JSR ace_snapshot
    LDAB ace_stock_pos
    LDX ace_stock
    ABX
    LDAA 0,X
    LDAB ace_waste_count
    LDX #board + 28
    ABX
    STAA 0,X
    INC ace_stock_pos
    INC ace_waste_count
    JSR grid_count_move
    JMP ace_check
ace_undo:
    TST undo_valid
    BEQ ace_aux_done
    JSR grid_restore
    LDAA ace_undo_pos
    STAA ace_stock_pos
    LDAA ace_undo_waste
    STAA ace_waste_count
    CLR selection_active
    JMP ace_check
ace_aux_done:
    RTS
ace_check:
    TST grid_stat
    BNE ace_check_stock
    LDAA #4
    STAA phase
    RTS
ace_check_stock:
    LDAA ace_stock_pos
    CMPA #24
    BCS ace_check_done
    LDX #ace_ranks
    LDAB #14
    CLRA
ace_clear_ranks:
    STAA 0,X
    INX
    DECB
    BNE ace_clear_ranks
    CLR ace_index
ace_find_pair:
    LDAB ace_index
    JSR ace_available
    TSTA
    BEQ ace_pair_next
    CMPA #13
    BEQ ace_check_done
    STAA ace_query_rank
    LDAA #13
    SUBA ace_query_rank
    TAB
    LDX #ace_ranks
    ABX
    TST 0,X
    BNE ace_check_done
    LDAB ace_query_rank
    LDX #ace_ranks
    ABX
    LDAA #1
    STAA 0,X
ace_pair_next:
    INC ace_index
    LDAA ace_index
    CMPA #29
    BNE ace_find_pair
    LDAA #5
    STAA phase
ace_check_done:
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    BEQ ace_tile_done
    STAA ace_display_rank
    TST selection_active
    BEQ ace_tile_available
    CMPB ace_first
    BNE ace_tile_available
    ADDA #26
    RTS
ace_tile_available:
    JSR ace_available
    TSTA
    BEQ ace_tile_covered
    LDAA ace_display_rank
    ADDA #13
    RTS
ace_tile_covered:
    LDAA ace_display_rank
ace_tile_done:
    RTS
game_render:
    JSR cursor_blink_prepare
    JSR cursor_blink_board
    TST hud_ready
    BNE ace_render_hud
    JSR hud_begin
    ; Move only the static rule; the WASTE value may use its top pixel row.
    LDX #framebuffer + 192 * 4 + 4
    LDAB #56
ace_clear_rule:
    AIM #$FE,0,X
    INX
    DECB
    BNE ace_clear_rule
ace_render_hud:
    JSR visual_hud
    ; LEFT can repaint this LCD page, so restore the rule after its digits.
    LDX #framebuffer + 192 * 3 + 4
    LDAB #56
    CLR ace_rule_changed
ace_raise_rule:
    LDAA 0,X
    BMI ace_rule_next
    ORAA #$80
    STAA 0,X
    INC ace_rule_changed
ace_rule_next:
    INX
    DECB
    BNE ace_raise_rule
    TST ace_rule_changed
    BEQ ace_rule_done
    LDAA #3
    LDAB #4
    JSR dirty_mark
    LDAA #3
    LDAB #59
    JMP dirty_mark
ace_rule_done:
    RTS
.section .bss, bss
ace_stock: .space 2
ace_stock_pos: .space 1
ace_waste_count: .space 1
ace_first: .space 1
ace_pointer: .space 2
ace_index: .space 1
ace_query: .space 1
ace_query_rank: .space 1
ace_child: .space 1
ace_second_value: .space 1
ace_undo_pos: .space 1
ace_undo_waste: .space 1
ace_ranks: .space 14
ace_display_rank: .space 1
ace_rule_changed: .space 1
.section .data, data
ace_rank_labels: .byte 45,65,50,51,52,53,54,55,56,57,84,74,81,75
ace_waste_text: .byte 91,45,93,0
ace_sum_label: .byte 83,85,77,49,51,0
ace_moves_label: .byte 77,0
ace_left_label: .byte 76,69,70,84,0
ace_waste_label: .byte 87,65,83,84,69,0
ace_deck_label: .byte 68,69,67,75,0
ace_blank_label: .byte 32,0
ace_arrow_label: .byte 62,0
ace_selected_label: .byte 43,0

; The JR-800 input timer drives focus blinking; input immediately restores it.
.global cursor_blink_mask
.global cursor_blink_clock
.section .text, code
cursor_blink_prepare:
    TST hud_ready
    BEQ cursor_blink_show
    RTS
cursor_blink_tick:
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
