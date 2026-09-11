; SPDX-License-Identifier: MIT
.global dispatch_running
.global dispatch_time
.global dispatch_gates
.global dispatch_switch
.global dispatch_selected
.global dispatch_positions
.global dispatch_targets
.global dispatch_proposed
.global dispatch_done
.global dispatch_total
.global dispatch_next_train
.global dispatch_reason
.global dispatch_world
.section .text, code
game_start:
    JSR grid_reset
    CLR dispatch_running
    CLR dispatch_time
    CLR dispatch_gates
    CLR dispatch_gates + 1
    CLR dispatch_switch
    CLR dispatch_selected
    CLR dispatch_done
    CLR dispatch_next_train
    CLR dispatch_reason
    LDAA #19
    STAA cursor
    LDAA #255
    LDX #dispatch_positions
    LDAB #4
dispatch_clear_trains:
    STAA 0,X
    INX
    DECB
    BNE dispatch_clear_trains
    CLR dispatch_targets
    CLR dispatch_targets + 1
    CLR dispatch_targets + 2
    CLR dispatch_targets + 3
    LDAA stage
    LDAB #31
    MUL
    ADDD #dispatch_schedules
    XGDX
    LDAA 0,X
    STAA dispatch_total
    INX
    STX dispatch_schedule
    LDAA input_ticks
    STAA dispatch_clock
    RTS
game_aux:
    CMPA #1
    BNE dispatch_reset
    LDAA dispatch_running
    EORA #1
    STAA dispatch_running
    RTS
dispatch_reset:
    JMP game_start
game_update:
    TST resume_pending
    BEQ dispatch_input
    CLR resume_pending
    LDAA input_ticks
    STAA dispatch_clock
dispatch_input:
    LDAA input_event
    BITA #5
    BEQ dispatch_next_control
    TST dispatch_selected
    BNE dispatch_prev_control
    LDAA #3
    STAA dispatch_selected
dispatch_prev_control:
    DEC dispatch_selected
    BRA dispatch_cursor
dispatch_next_control:
    BITA #10
    BEQ dispatch_toggle
    INC dispatch_selected
    LDAA dispatch_selected
    CMPA #3
    BCS dispatch_cursor
    CLR dispatch_selected
dispatch_cursor:
    LDAB dispatch_selected
    LDX #dispatch_controls
    ABX
    LDAA 0,X
    STAA cursor
    LDAA #1
    STAA redraw
dispatch_toggle:
    LDAA input_event
    BITA #16
    BEQ dispatch_timer
    LDAB dispatch_selected
    CMPB #2
    BEQ dispatch_toggle_switch
    LDX #dispatch_gates
    ABX
    LDAA 0,X
    EORA #1
    STAA 0,X
    BRA dispatch_controls_changed
dispatch_toggle_switch:
    LDAA dispatch_switch
    EORA #1
    STAA dispatch_switch
dispatch_controls_changed:
    LDAA #1
    STAA redraw
dispatch_timer:
    TST dispatch_running
    BEQ dispatch_update_done
    LDAA input_ticks
    SUBA dispatch_clock
    CMPA #10
    BCS dispatch_update_done
    LDAA input_ticks
    STAA dispatch_clock
    JMP dispatch_world
dispatch_update_done:
    RTS
dispatch_world:
    INC dispatch_time
    LDAA #1
    STAA redraw
    CLR dispatch_actor
dispatch_propose:
    LDAB dispatch_actor
    LDX #dispatch_positions
    ABX
    LDAA 0,X
    CMPA #255
    BEQ dispatch_save_proposal
    CMPA #2
    BNE dispatch_lower_gate
    TST dispatch_gates
    BEQ dispatch_save_proposal
dispatch_lower_gate:
    CMPA #7
    BNE dispatch_route
    TST dispatch_gates + 1
    BEQ dispatch_save_proposal
dispatch_route:
    CMPA #14
    BNE dispatch_normal_route
    LDAA #15
    TST dispatch_switch
    BEQ dispatch_save_proposal
    LDAA #21
    BRA dispatch_save_proposal
dispatch_normal_route:
    TAB
    LDX #dispatch_next_nodes
    ABX
    LDAA 0,X
dispatch_save_proposal:
    LDAB dispatch_actor
    LDX #dispatch_proposed
    ABX
    STAA 0,X
    INC dispatch_actor
    LDAA dispatch_actor
    CMPA #4
    BNE dispatch_propose
    LDAA #4
    STAA dispatch_pass
dispatch_block_pass:
    CLR dispatch_actor
dispatch_block_train:
    LDAB dispatch_actor
    LDX #dispatch_proposed
    ABX
    LDAA 0,X
    CMPA #255
    BEQ dispatch_block_next
    STAA dispatch_option
    LDX #dispatch_positions
    ABX
    CMPA 0,X
    BEQ dispatch_block_next
    CLR dispatch_other
dispatch_block_check:
    LDAA dispatch_other
    CMPA dispatch_actor
    BEQ dispatch_block_check_next
    TAB
    LDX #dispatch_positions
    ABX
    LDAA 0,X
    CMPA dispatch_option
    BNE dispatch_block_check_next
    LDX #dispatch_proposed
    ABX
    CMPA 0,X
    BNE dispatch_block_check_next
    LDAB dispatch_actor
    LDX #dispatch_positions
    ABX
    LDAA 0,X
    LDX #dispatch_proposed
    ABX
    STAA 0,X
    BRA dispatch_block_next
dispatch_block_check_next:
    INC dispatch_other
    LDAA dispatch_other
    CMPA #4
    BNE dispatch_block_check
dispatch_block_next:
    INC dispatch_actor
    LDAA dispatch_actor
    CMPA #4
    BNE dispatch_block_train
    DEC dispatch_pass
    BNE dispatch_block_pass
    CLR dispatch_actor
dispatch_collision_train:
    LDAB dispatch_actor
    LDX #dispatch_proposed
    ABX
    LDAA 0,X
    CMPA #255
    BEQ dispatch_collision_next
    STAA dispatch_option
    LDX #dispatch_positions
    ABX
    LDAA 0,X
    STAA dispatch_origin
    INC dispatch_actor
    LDAA dispatch_actor
    STAA dispatch_other
    DEC dispatch_actor
dispatch_collision_other:
    LDAA dispatch_other
    CMPA #4
    BCC dispatch_collision_next
    TAB
    LDX #dispatch_proposed
    ABX
    LDAA 0,X
    CMPA #255
    BEQ dispatch_collision_more
    CMPA dispatch_option
    BEQ dispatch_crash
    CMPA dispatch_origin
    BNE dispatch_collision_more
    LDX #dispatch_positions
    ABX
    LDAA 0,X
    CMPA dispatch_option
    BEQ dispatch_crash
dispatch_collision_more:
    INC dispatch_other
    BRA dispatch_collision_other
dispatch_collision_next:
    INC dispatch_actor
    LDAA dispatch_actor
    CMPA #4
    BNE dispatch_collision_train
    CLR dispatch_actor
dispatch_commit:
    LDAB dispatch_actor
    LDX #dispatch_proposed
    ABX
    LDAA 0,X
    LDX #dispatch_positions
    ABX
    STAA 0,X
    CMPA #20
    BEQ dispatch_arrive_a
    CMPA #26
    BNE dispatch_commit_next
    LDAA #1
    BRA dispatch_arrive
dispatch_arrive_a:
    CLRA
dispatch_arrive:
    LDX #dispatch_targets
    ABX
    CMPA 0,X
    BNE dispatch_wrong_exit
    LDX #dispatch_positions
    ABX
    LDAA #255
    STAA 0,X
    INC dispatch_done
dispatch_commit_next:
    INC dispatch_actor
    LDAA dispatch_actor
    CMPA #4
    BNE dispatch_commit
    JSR dispatch_spawn
    LDAA dispatch_done
    CMPA dispatch_total
    BEQ dispatch_complete
    LDAA dispatch_time
    CMPA #180
    BCC dispatch_timeout
    RTS
dispatch_crash:
    LDAA #1
    BRA dispatch_fail
dispatch_wrong_exit:
    LDAA #2
    BRA dispatch_fail
dispatch_timeout:
    LDAA #3
dispatch_fail:
    STAA dispatch_reason
    CLR dispatch_running
    LDAA #5
    STAA phase
    RTS
dispatch_complete:
    CLR dispatch_running
    LDAA #4
    STAA phase
    RTS
dispatch_spawn:
    LDAA dispatch_next_train
    CMPA dispatch_total
    BEQ dispatch_spawn_done
    LDAB #3
    MUL
    ADDD dispatch_schedule
    XGDX
    LDAA dispatch_time
    CMPA 0,X
    BCS dispatch_spawn_done
    LDAA 2,X
    STAA dispatch_target
    LDAA 1,X
    LDAB #5
    MUL
    STAB dispatch_entry
    CLR dispatch_actor
    LDAA #255
    STAA dispatch_free
dispatch_spawn_scan:
    LDAB dispatch_actor
    LDX #dispatch_positions
    ABX
    LDAA 0,X
    CMPA dispatch_entry
    BEQ dispatch_spawn_done
    CMPA #255
    BNE dispatch_spawn_scan_next
    STAB dispatch_free
dispatch_spawn_scan_next:
    INC dispatch_actor
    LDAA dispatch_actor
    CMPA #4
    BNE dispatch_spawn_scan
    LDAB dispatch_free
    CMPB #255
    BEQ dispatch_spawn_done
    LDX #dispatch_positions
    ABX
    LDAA dispatch_entry
    STAA 0,X
    LDX #dispatch_targets
    ABX
    LDAA dispatch_target
    STAA 0,X
    INC dispatch_next_train
dispatch_spawn_done:
    RTS
grid_value:
    STAB dispatch_tile_cell
    CLR dispatch_scan
dispatch_tile_train:
    LDAB dispatch_scan
    LDX #dispatch_positions
    ABX
    LDAB 0,X
    CMPB #255
    BEQ dispatch_tile_train_next
    LDX #dispatch_node_cells
    ABX
    LDAA 0,X
    CMPA dispatch_tile_cell
    BNE dispatch_tile_train_next
    LDAB dispatch_scan
    LDX #dispatch_targets
    ABX
    LDAA 0,X
    ADDA #16
    RTS
dispatch_tile_train_next:
    INC dispatch_scan
    LDAA dispatch_scan
    CMPA #4
    BNE dispatch_tile_train
    LDAB dispatch_tile_cell
    CMPB #19
    BEQ dispatch_tile_upper
    CMPB #83
    BEQ dispatch_tile_lower
    CMPB #56
    BEQ dispatch_tile_switch
    CMPB #29
    BEQ dispatch_tile_a
    CMPB #93
    BEQ dispatch_tile_b
    LDX #dispatch_track
    ABX
    LDAA 0,X
    RTS
dispatch_tile_upper:
    LDAA dispatch_gates
    ADDA #18
    RTS
dispatch_tile_lower:
    LDAA dispatch_gates + 1
    ADDA #18
    RTS
dispatch_tile_switch:
    LDAA #9
    TST dispatch_switch
    BEQ dispatch_tile_done
    LDAA #12
dispatch_tile_done:
    RTS
dispatch_tile_a:
    LDAA #16
    RTS
dispatch_tile_b:
    LDAA #17
    RTS
game_render:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
dispatch_running: .space 1
dispatch_time: .space 1
dispatch_gates: .space 2
dispatch_switch: .space 1
dispatch_selected: .space 1
dispatch_done: .space 1
dispatch_total: .space 1
dispatch_next_train: .space 1
dispatch_reason: .space 1
dispatch_positions: .space 4
dispatch_targets: .space 4
dispatch_proposed: .space 4
dispatch_clock: .space 1
dispatch_schedule: .space 2
dispatch_actor: .space 1
dispatch_other: .space 1
dispatch_pass: .space 1
dispatch_option: .space 1
dispatch_origin: .space 1
dispatch_target: .space 1
dispatch_entry: .space 1
dispatch_free: .space 1
dispatch_tile_cell: .space 1
dispatch_scan: .space 1
dispatch_hud_force: .space 1
dispatch_old_time: .space 1
dispatch_old_done: .space 1
dispatch_old_running: .space 1
dispatch_old_switch: .space 1
dispatch_old_next: .space 1
dispatch_old_reason: .space 1
.section .data, data
dispatch_controls: .byte 19,83,56
dispatch_next_label: .byte 78,69,88,84,0
dispatch_done_label: .byte 68,79,78,69,0
dispatch_left_label: .byte 76,69,70,84,0
dispatch_exit_label: .byte 69,88,73,84,0
dispatch_set_label: .byte 83,69,84,0
dispatch_run_label: .byte 82,85,78,0
dispatch_a_label: .byte 65,0
dispatch_b_label: .byte 66,0
dispatch_end_label: .byte 69,78,68,0
dispatch_route_labels: .byte 85,62,65,0,85,62,66,0,68,62,65,0,68,62,66,0
dispatch_reason_labels: .byte 67,82,65,83,72,0,82,79,85,84,69,0,84,73,77,69,32,0
