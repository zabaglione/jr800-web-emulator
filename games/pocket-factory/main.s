; SPDX-License-Identifier: MIT
.global board
.global items
.global cursor
.global tool
.global running
.global shipped_a
.global shipped_b
.global target_a
.global target_b
.global factory_steps
.section .text, code
game_start:
    JSR challenge_start
    LDX #next_items
    JSR challenge_load
    LDD next_items
    STD target_a
    CLR factory_index
factory_bitmap_cell:
    LDAB factory_index
    LSRB
    LSRB
    LSRB
    LDX #next_items + 6
    ABX
    LDAA 0,X
    LDAB factory_index
    ANDB #7
    BEQ factory_bitmap_bit
factory_bitmap_shift:
    LSRA
    DECB
    BNE factory_bitmap_shift
factory_bitmap_bit:
    ANDA #1
    BEQ factory_bitmap_store
    LDAA #5
factory_bitmap_store:
    LDAB factory_index
    LDX #board
    ABX
    STAA 0,X
    INC factory_index
    LDAA factory_index
    CMPA #112
    BNE factory_bitmap_cell
    CLR factory_index
factory_ports:
    LDAB factory_index
    LDX #next_items + 2
    ABX
    LDAB 0,X
    LDX #board
    ABX
    LDAA factory_index
    INCA
    STAA 0,X
    INC factory_index
    LDAA factory_index
    CMPA #4
    BNE factory_ports
    CLR tool
    CLR running
    CLR shipped_a
    CLR shipped_b
    CLR factory_steps
    CLR factory_clock
    CLR emission
    LDAA #18
    STAA cursor
    JMP clear_items
clear_items:
    LDX #items
    LDAB #112
    CLRA
factory_clear_items:
    STAA 0,X
    INX
    DECB
    BNE factory_clear_items
    RTS
game_update:
    JSR cursor_blink_tick
    TST resume_pending
    BEQ factory_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA factory_clock
factory_clock_ready:
    TST selection_active
    BEQ factory_play
    LDAA input_event
    BITA #16
    BNE factory_tool_confirm
    BITA #5
    BEQ factory_tool_right
    TST tool
    BEQ factory_tool_last
    DEC tool
    JMP factory_changed
factory_tool_last:
    LDAA #6
    STAA tool
    JMP factory_changed
factory_tool_right:
    BITA #10
    BNE factory_tool_next
    RTS
factory_tool_next:
    INC tool
    LDAA tool
    CMPA #7
    BCC factory_tool_zero
    JMP factory_changed
factory_tool_zero:
    CLR tool
    JMP factory_changed
factory_tool_confirm:
    CLR selection_active
    JSR input_gate
    JSR paint_clear
    JMP factory_changed
factory_play:
    LDAA input_event
    BITA #16
    BNE factory_place
    BITA #1
    BEQ factory_down
    LDAA cursor
    CMPA #16
    BCS factory_simulate
    SUBA #16
    BRA factory_move
factory_down:
    BITA #2
    BEQ factory_left
    LDAA cursor
    CMPA #96
    BCC factory_main_tool
    ADDA #16
    BRA factory_move
factory_left:
    BITA #4
    BEQ factory_right
    LDAA cursor
    BITA #15
    BEQ factory_main_tool
    DECA
    BRA factory_move
factory_right:
    BITA #8
    BEQ factory_simulate
    LDAA cursor
    ANDA #15
    CMPA #15
    BEQ factory_simulate
    LDAA cursor
    INCA
factory_move:
    STAA cursor
    LDAA #1
    STAA redraw
    BRA factory_simulate
factory_place:
    TST running
    BNE factory_simulate
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    BEQ factory_place_tile
    CMPA #6
    BCS factory_simulate
factory_place_tile:
    LDAA tool
    ADDA #6
    CMPA #12
    BCS factory_place_store
    CLRA
factory_place_store:
    CMPA 0,X
    BEQ factory_idle
    STAA 0,X
    JSR challenge_step
factory_changed:
    LDAA #1
    STAA redraw
factory_idle:
    RTS
factory_main_tool:
    TST running
    BNE factory_simulate
    LDAA #1
    STAA selection_active
    JMP factory_changed
factory_simulate:
    TST running
    BEQ factory_idle
    LDAA input_ticks
    SUBA factory_clock
    CMPA #12
    BCS factory_idle
    LDAA input_ticks
    STAA factory_clock
    JSR simulate
    JMP factory_changed
game_aux:
    CMPA #1
    BNE factory_toggle_run
    TST running
    BNE factory_idle
    LDAA #1
    STAA selection_active
    RTS
factory_toggle_run:
    LDAA running
    EORA #1
    STAA running
    RTS
; Conservative two-phase transport: no overwrites, at most one cell per tick.
simulate:
    INC factory_steps
    LDX #items
    STX factory_source
    LDX #next_items
    STX factory_dest
    LDAB #112
factory_copy_items:
    LDX factory_source
    LDAA 0,X
    INX
    STX factory_source
    LDX factory_dest
    STAA 0,X
    INX
    STX factory_dest
    DECB
    BNE factory_copy_items
    CLR factory_index
factory_item_loop:
    LDAB factory_index
    LDX #items
    ABX
    LDAA 0,X
    BNE factory_item_exists
    JMP factory_item_next
factory_item_exists:
    STAA factory_item
    LDX #board
    ABX
    LDAA 0,X
    STAA factory_tile
    CMPA #3
    BEQ factory_sink_a
    CMPA #4
    BEQ factory_sink_b
    CMPA #10
    BEQ factory_process_a
    CMPA #11
    BEQ factory_process_b
    BRA factory_transport
factory_sink_a:
    LDAA factory_item
    CMPA #3
    BEQ factory_ship_a
    JMP factory_item_next
factory_ship_a:
    LDAA shipped_a
    CMPA target_a
    BCC factory_consume
    INC shipped_a
    BRA factory_consume
factory_sink_b:
    LDAA factory_item
    CMPA #4
    BEQ factory_ship_b
    JMP factory_item_next
factory_ship_b:
    LDAA shipped_b
    CMPA target_b
    BCC factory_consume
    INC shipped_b
factory_consume:
    LDX #next_items
    LDAB factory_index
    ABX
    CLR 0,X
    BRA factory_item_next
factory_process_a:
    LDAA factory_item
    CMPA #1
    BNE factory_transport
    LDAA #3
    BRA factory_convert
factory_process_b:
    LDAA factory_item
    CMPA #2
    BNE factory_transport
    LDAA #4
factory_convert:
    LDAB factory_index
    LDX #next_items
    ABX
    STAA 0,X
    BRA factory_item_next
factory_transport:
    LDAB factory_tile
    LDX #flow_deltas
    ABX
    LDAA 0,X
    ADDA factory_index
    CMPA #112
    BCC factory_item_next
    STAA factory_next
    TAB
    LDX #board
    ABX
    LDAA 0,X
    CMPA #3
    BCS factory_item_next
    CMPA #5
    BEQ factory_item_next
    ; Prevent wraparound of the left/right belt at the grid edge.
    LDAA factory_next
    EORA factory_index
    ANDA #$F0
    BEQ factory_destination
    LDAA factory_next
    SUBA factory_index
    CMPA #16
    BEQ factory_destination
    CMPA #240
    BNE factory_item_next
factory_destination:
    LDX #items
    ABX
    TST 0,X
    BNE factory_item_next
    LDX #next_items
    ABX
    TST 0,X
    BNE factory_item_next
    LDAA factory_item
    STAA 0,X
    CMPA #3
    BCS factory_raw_transport
    JSR challenge_touch
factory_raw_transport:
    JMP factory_consume
factory_item_next:
    INC factory_index
    LDAA factory_index
    CMPA #112
    BEQ factory_emit
    JMP factory_item_loop
factory_emit:
    INC emission
    LDAA emission
    CMPA #3
    BNE factory_commit
    CLR emission
    CLRB
factory_emit_loop:
    LDX #board
    ABX
    LDAA 0,X
    BEQ factory_emit_next
    CMPA #3
    BCC factory_emit_next
    LDX #next_items
    ABX
    TST 0,X
    BNE factory_emit_next
    STAA 0,X
factory_emit_next:
    INCB
    CMPB #112
    BNE factory_emit_loop
factory_commit:
    CLRB
factory_commit_loop:
    LDX #next_items
    ABX
    LDAA 0,X
    LDX #items
    ABX
    STAA 0,X
    INCB
    CMPB #112
    BNE factory_commit_loop
    LDAA shipped_a
    CMPA target_a
    BCS factory_sim_done
    LDAA shipped_b
    CMPA target_b
    BCS factory_sim_done
    CLR running
    LDAA #4
    STAA phase
factory_sim_done:
    RTS
game_tile:
    STAB factory_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    STAA factory_tile
    LDX #items
    ABX
    LDAA 0,X
    BEQ factory_tile_base
    LDAB #12
    MUL
    ADDB factory_tile
    TBA
    BRA factory_tile_cursor
factory_tile_base:
    LDAA factory_tile
factory_tile_cursor:
    TST selection_active
    BNE factory_tile_done
    LDAB factory_tile_cell
    CMPB cursor
    BNE factory_tile_done
    ORAA cursor_blink_mask
factory_tile_done:
    RTS
game_bonus:
    RTS
game_render:
    JSR cursor_blink_prepare
    JSR paint_board
    JMP visual_hud
.section .bss, bss
target_a: .space 1
target_b: .space 1
board: .space 112
items: .space 112
next_items: .space 112
cursor: .space 1
tool: .space 1
running: .space 1
shipped_a: .space 1
shipped_b: .space 1
factory_steps: .space 1
factory_clock: .space 1
emission: .space 1
factory_source: .space 2
factory_dest: .space 2
factory_index: .space 1
factory_item: .space 1
factory_tile: .space 1
factory_next: .space 1
factory_tile_cell: .space 1

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
.section .bss, bss
cursor_blink_mask: .space 1
cursor_blink_clock: .space 1
