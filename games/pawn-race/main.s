; SPDX-License-Identifier: MIT
.global pawn_source
.global pawn_options
.global pawn_from
.global pawn_to
.global pawn_dir
.global pawn_side
.global pawn_valid
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR selection_active
    LDAB #0
    LDX #board
    LDAA #2
pawn_place_cpu:
    STAA 0,X
    INX
    INCB
    CMPB #12
    BNE pawn_place_cpu
    LDX #board + 24
    LDAA #1
pawn_place_human:
    STAA 0,X
    INX
    DECB
    BNE pawn_place_human
    LDAA #26
    STAA cursor
    LDAA #12
    STAA grid_stat
    RTS
game_update:
    LDAA input_event
    BITA #16
    BNE pawn_action
    JSR grid_move
    CMPB #255
    BNE pawn_cursor
    RTS
pawn_cursor:
    STAB cursor
    JMP grid_changed
pawn_action:
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE pawn_destination
    STAB pawn_source
    LDAA #1
    STAA selection_active
    JSR pawn_hints
    JMP grid_changed
pawn_destination:
    TST selection_active
    BEQ pawn_idle
    LDX #pawn_options
    ABX
    TST 0,X
    BEQ pawn_idle
    JSR grid_snapshot
    LDAA pawn_source
    STAA pawn_from
    LDAA cursor
    STAA pawn_to
    LDAA #1
    STAA pawn_side
    JSR pawn_apply
    CLR selection_active
    JSR grid_count_move
    LDAA pawn_to
    CMPA #6
    BCS pawn_win
    JSR pawn_choose_cpu
    LDAA pawn_best_from
    CMPA #255
    BEQ pawn_win
    STAA pawn_from
    LDAA pawn_best_to
    STAA pawn_to
    LDAA #2
    STAA pawn_side
    JSR pawn_apply
    JSR pawn_counts
    LDAA pawn_to
    CMPA #30
    BCC pawn_loss
    JSR pawn_human_available
    TST pawn_available
    BEQ pawn_loss
pawn_idle:
    RTS
pawn_win:
    JSR pawn_counts
    LDAA #4
    STAA phase
    RTS
pawn_loss:
    LDAA #5
    STAA phase
    RTS
; A=valid. Side 1 advances up, side 2 down; forward captures are forbidden.
pawn_valid:
    LDAB pawn_from
    LDX #board
    ABX
    LDAA 0,X
    CMPA pawn_side
    BNE pawn_invalid
    LDAA pawn_from
    LDAB #3
    MUL
    ADDB pawn_dir
    LDX #pawn_links
    LDAA pawn_side
    CMPA #1
    BEQ pawn_link_ready
    LDX #pawn_links + 108
pawn_link_ready:
    ABX
    LDAB 0,X
    STAB pawn_to
    CMPB #255
    BEQ pawn_invalid
    LDX #board
    ABX
    LDAA 0,X
    CMPA pawn_side
    BEQ pawn_invalid
    TST pawn_dir
    BNE pawn_valid_yes
    TSTA
    BNE pawn_invalid
pawn_valid_yes:
    LDAA #1
    RTS
pawn_invalid:
    CLRA
    RTS
pawn_apply:
    LDAB pawn_from
    LDX #board
    ABX
    CLR 0,X
    LDAB pawn_to
    LDX #board
    ABX
    LDAA pawn_side
    STAA 0,X
    RTS
pawn_hints:
    LDX #pawn_options
    LDAB #36
    CLRA
pawn_clear_options:
    STAA 0,X
    INX
    DECB
    BNE pawn_clear_options
    LDAA pawn_source
    STAA pawn_from
    LDAA #1
    STAA pawn_side
    CLR pawn_dir
pawn_hint_loop:
    JSR pawn_valid
    TSTA
    BEQ pawn_hint_next
    LDAB pawn_to
    LDX #pawn_options
    ABX
    LDAA #1
    STAA 0,X
pawn_hint_next:
    INC pawn_dir
    LDAA pawn_dir
    CMPA #3
    BNE pawn_hint_loop
    RTS
pawn_choose_cpu:
    LDAA #255
    STAA pawn_best_from
    CLR pawn_best_score
    CLR pawn_scan_from
pawn_cpu_from:
    LDAB pawn_scan_from
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BNE pawn_cpu_next_from
    CLR pawn_scan_dir
pawn_cpu_dir:
    LDAA pawn_scan_from
    STAA pawn_from
    LDAA pawn_scan_dir
    STAA pawn_dir
    LDAA #2
    STAA pawn_side
    JSR pawn_valid
    TSTA
    BEQ pawn_cpu_next
    JSR pawn_rate
    CMPA pawn_best_score
    BLS pawn_cpu_next
    STAA pawn_best_score
    LDAA pawn_trial_from
    STAA pawn_best_from
    LDAA pawn_trial_to
    STAA pawn_best_to
    LDAA pawn_best_score
    CMPA #255
    BEQ pawn_cpu_done
pawn_cpu_next:
    INC pawn_scan_dir
    LDAA pawn_scan_dir
    CMPA #3
    BNE pawn_cpu_dir
pawn_cpu_next_from:
    JSR input_poll
    INC pawn_scan_from
    LDAA pawn_scan_from
    CMPA #36
    BNE pawn_cpu_from
pawn_cpu_done:
    RTS
pawn_rate:
    LDAA pawn_from
    STAA pawn_trial_from
    LDAA pawn_to
    STAA pawn_trial_to
    TAB
    LDX #board
    ABX
    LDAA 0,X
    STAA pawn_capture
    JSR pawn_apply
    LDAA pawn_trial_to
    CMPA #30
    BCS pawn_rate_normal
    LDAA #255
    STAA pawn_score
    BRA pawn_rate_restore
pawn_rate_normal:
    ; Prefer progress, then captures. Hard mode also rejects an immediate loss.
    ADDA #32
    STAA pawn_score
    TST pawn_capture
    BEQ pawn_probe
    LDAA #4
    TST stage
    BEQ pawn_capture_bonus
    LDAA #16
pawn_capture_bonus:
    ADDA pawn_score
    STAA pawn_score
pawn_probe:
    JSR pawn_human_available
    TST pawn_available
    BNE pawn_rate_threat
    LDAA #255
    STAA pawn_score
    BRA pawn_rate_restore
pawn_rate_threat:
    LDAA stage
    CMPA #2
    BNE pawn_rate_risk
    TST pawn_threat
    BEQ pawn_rate_risk
    LDAA #1
    STAA pawn_score
    BRA pawn_rate_restore
pawn_rate_risk:
    TST stage
    BEQ pawn_rate_restore
    TST pawn_risk
    BEQ pawn_rate_restore
    LDAA pawn_score
    SUBA #14
    STAA pawn_score
pawn_rate_restore:
    LDAB pawn_trial_from
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    LDAB pawn_trial_to
    LDX #board
    ABX
    LDAA pawn_capture
    STAA 0,X
    LDAA pawn_score
    RTS
pawn_human_available:
    CLR pawn_available
    CLR pawn_threat
    CLR pawn_risk
    CLR pawn_from
    LDAA #1
    STAA pawn_side
pawn_probe_from:
    LDAB pawn_from
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE pawn_probe_next_from
    CLR pawn_dir
pawn_probe_dir:
    JSR pawn_valid
    TSTA
    BEQ pawn_probe_next
    STAA pawn_available
    LDAA pawn_to
    CMPA #6
    BCC pawn_probe_risk
    LDAA #1
    STAA pawn_threat
pawn_probe_risk:
    LDAA pawn_to
    CMPA pawn_trial_to
    BNE pawn_probe_next
    LDAA #1
    STAA pawn_risk
pawn_probe_next:
    INC pawn_dir
    LDAA pawn_dir
    CMPA #3
    BNE pawn_probe_dir
pawn_probe_next_from:
    JSR input_poll
    INC pawn_from
    LDAA pawn_from
    CMPA #36
    BNE pawn_probe_from
    RTS
pawn_counts:
    CLR grid_stat
    LDX #board
    LDAB #36
pawn_count_loop:
    LDAA 0,X
    CMPA #2
    BNE pawn_count_next
    INC grid_stat
pawn_count_next:
    INX
    DECB
    BNE pawn_count_loop
    RTS
game_aux:
    CMPA #1
    BNE pawn_restart
    CLR selection_active
    JMP grid_restore
pawn_restart:
    JMP game_start
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    TST selection_active
    BEQ pawn_tile_done
    CMPB pawn_source
    BNE pawn_hint_tile
    LDAA #3
    RTS
pawn_hint_tile:
    STAA pawn_tile_piece
    LDX #pawn_options
    ABX
    TST 0,X
    BEQ pawn_tile_original
    LDAA #4
    TST pawn_tile_piece
    BEQ pawn_tile_done
    LDAA #5
    RTS
pawn_tile_original:
    LDAA pawn_tile_piece
pawn_tile_done:
    RTS
game_render:
    JSR grid_render
    LDX #pawn_blank_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #pawn_pick_label
    TST selection_active
    BEQ pawn_status
    LDX #pawn_move_label
pawn_status:
    LDAA #138
    LDAB #5
    JMP paint_text
.section .bss, bss
pawn_source: .space 1
pawn_options: .space 36
pawn_from: .space 1
pawn_to: .space 1
pawn_dir: .space 1
pawn_side: .space 1
pawn_scan_from: .space 1
pawn_scan_dir: .space 1
pawn_best_from: .space 1
pawn_best_to: .space 1
pawn_best_score: .space 1
pawn_trial_from: .space 1
pawn_trial_to: .space 1
pawn_capture: .space 1
pawn_score: .space 1
pawn_available: .space 1
pawn_threat: .space 1
pawn_risk: .space 1
pawn_tile_piece: .space 1
.section .data, data
pawn_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
pawn_pick_label: .byte 80,73,67,75,0
pawn_move_label: .byte 77,79,86,69,0
