; SPDX-License-Identifier: MIT
; Numeric keypad actions, edge-triggered input, and a RAM-retained checkpoint.
.global entry
.global frame_ready
.global redraw
.global new_game
.global generate_world
.global update_visibility
.global commit_turn
.global state_begin
.extern init
.extern clear
.extern present
.extern text
.extern glyph
.extern framebuffer
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR init
    LDD G_MAGIC
    SUBD #$5244
    BNE cold_start
    LDD G_MAGIC + 2
    SUBD #$3031
    BNE cold_start
    LDAA G_SUSPEND
    CMPA #1
    BNE cold_start
    CLR G_SUSPEND
    LDAA #1
    STAA G_MODE
    CLR G_PREVIOUS
    CLR G_PENDING
    CLR G_AUTOWALK
    JSR select_floor
    JSR update_visibility
    JMP redraw
cold_start:
    LDX #state_begin
    CLRA
cold_zero:
    STAA 0,X
    INX
    CPX #state_begin + 1536
    BNE cold_zero
    LDD #1
    STD G_SEED
    LDAA #1
    STAA G_DIFFICULTY
    JMP redraw

new_game:
    LDD G_SEED
    STD G_RNG
    LDX #G_X
    CLRA
new_zero:
    STAA 0,X
    INX
    CPX #G_RNG
    BNE new_zero
    LDX #G_BAG
new_bag_zero:
    STAA 0,X
    INX
    CPX #G_SEED
    BNE new_bag_zero
    CLR G_FLOOR
    CLR G_SUSPEND
    CLR G_MENU
    LDD #frame_ready
    STD G_READY_ADDRESS
    LDD #$5244
    STD G_MAGIC
    LDD #$3031
    STD G_MAGIC + 2
    LDAA #3
    STAA G_ATTACK
    LDAA #1
    STAA G_LEVEL
    CLR G_POISON
    CLR G_POISON_TICK
    CLR G_AUTOWALK
    LDX #difficulty_depth
    LDAB G_DIFFICULTY
    ABX
    LDAA 0,X
    STAA G_DEPTH
    LDX #difficulty_hp
    LDAB G_DIFFICULTY
    ABX
    LDAA 0,X
    STAA G_HP
    STAA G_MAX_HP
    LDX #difficulty_food
    ABX
    LDAA 0,X
    STAA G_FOOD
    ; Appearance -> effect is shuffled once per adventure.
    LDD #$0001
    STD G_IDENTITIES
    LDAA #2
    STAA G_IDENTITIES + 2
    LDD #$0304
    STD G_IDENTITIES + 3
    JSR random
    ANDA #1
    BEQ shuffle_second
    LDD #$0100
    STD G_IDENTITIES
shuffle_second:
    JSR random
    ANDA #1
    BEQ shuffle_scroll
    LDAA G_IDENTITIES + 1
    LDAB G_IDENTITIES + 2
    STAB G_IDENTITIES + 1
    STAA G_IDENTITIES + 2
shuffle_scroll:
    JSR random
    ANDA #1
    BEQ shuffle_done
    LDD #$0403
    STD G_IDENTITIES + 3
shuffle_done:
    JSR generate_world
    CLR G_FLOOR
    JSR select_floor
    ; Stairs are at fixed room centers, with varied rooms/corridors around them.
    LDAA #4
    STAA G_X
    LDAA #4
    STAA G_Y
    LDAA #1
    STAA G_MODE
    CLR G_MESSAGE
    JSR update_visibility
    RTS

redraw:
    JSR render_screen
    JSR poll_key
    JSR present
    JSR poll_key
frame_ready:
    ; A stable turn boundary, with no active subroutine frames.
    NOP
idle:
    JSR poll_key
    TST G_MODE
    BNE idle_seed_done
    LDD G_SEED
    ADDD #1
    BNE seed_nonzero
    ADDD #1
seed_nonzero:
    STD G_SEED
idle_seed_done:
    LDAA G_PENDING
    BNE dispatch
    LDAA G_MODE
    CMPA #7
    BEQ frame_ready
    TST G_AUTOWALK
    BEQ idle
    LDD G_REPEAT
    ADDD #1
    STD G_REPEAT
    SUBD #1800
    BCS idle
    CLRA
    CLRB
    STD G_REPEAT
    JSR safe_walk
    TST SAFE_FLAG
    BNE repeat_step
    CLR G_AUTOWALK
    BRA idle
repeat_step:
    LDAA G_PREVIOUS
    STAA G_KEY
    JSR world_direction
    JMP redraw
dispatch:
    STAA G_KEY
    CLR G_PENDING
    CLR G_AUTOWALK
    CLRA
    CLRB
    STD G_REPEAT
    LDAA G_MODE
    BEQ title_input
    CMPA #1
    BEQ world_input
    CMPA #5
    BEQ inspect_input
    CMPA #7
    BEQ suspend_input
    CMPA #8
    BCC end_input
    JSR menu_input
    JMP redraw
title_input:
    LDAA G_KEY
    CMPA #5
    BEQ title_start
    CMPA #1
    BEQ difficulty_up
    CMPA #2
    BNE title_done
    LDAA G_DIFFICULTY
    CMPA #2
    BEQ title_done
    INC G_DIFFICULTY
    BRA title_done
difficulty_up:
    TST G_DIFFICULTY
    BEQ title_done
    DEC G_DIFFICULTY
title_done:
    JMP redraw
title_start:
    JSR new_game
    JMP redraw
world_input:
    LDAA G_KEY
    CMPA #5
    BEQ open_menu
    CMPA #6
    BEQ world_done
    CMPA #11
    BEQ world_wait
    JSR world_direction
world_done:
    JMP redraw
world_wait:
    CLR G_MESSAGE
    JSR commit_turn
    JMP redraw
open_menu:
    LDAA #2
    STAA G_MODE
    CLR G_MENU
    JMP redraw
inspect_input:
    JSR inspect_move
    JMP redraw
suspend_input:
    LDAA G_KEY
    CMPA #5
    BNE suspend_stay
    CLR G_SUSPEND
    LDAA #1
    STAA G_MODE
suspend_stay:
    JMP redraw
end_input:
    LDAA G_KEY
    CMPA #5
    BNE end_stay
    CLR G_MODE
end_stay:
    JMP redraw

; 1 up, 2 down, 3 left, 4 right, 5 confirm, 6 back.
; 7 northwest, 8 northeast, 9 southwest, 10 southeast, 11 wait.
; E-392: active-low key positions, explicit idle $FF is required.
poll_key:
    CLRB
    LDAA $0FFD
    BITA #1
    BNE poll_down
    LDAB #1
    BRA poll_sample
poll_down:
    LDAA $0FFE
    BITA #4
    BNE poll_left
    LDAB #2
    BRA poll_sample
poll_left:
    BITA #16
    BNE poll_right
    LDAB #3
    BRA poll_sample
poll_right:
    BITA #64
    BNE poll_northwest
    LDAB #4
    BRA poll_sample
poll_northwest:
    BITA #128
    BNE poll_southwest
    LDAB #7
    BRA poll_sample
poll_southwest:
    BITA #2
    BNE poll_southeast
    LDAB #9
    BRA poll_sample
poll_southeast:
    BITA #8
    BNE poll_wait
    LDAB #10
    BRA poll_sample
poll_wait:
    BITA #32
    BNE poll_northeast
    LDAB #11
    BRA poll_sample
poll_northeast:
    LDAA $0FFD
    BITA #2
    BNE poll_confirm
    LDAB #8
    BRA poll_sample
poll_confirm:
    LDAA $0F7F
    BITA #64
    BNE poll_back
    LDAB #5
    BRA poll_sample
poll_back:
    LDAA $0FEF
    BITA #1
    BNE poll_sample
    LDAB #6
poll_sample:
    TSTB
    BNE poll_edge
    CLR G_AUTOWALK
poll_edge:
    CMPB G_PREVIOUS
    BEQ poll_done
    STAB G_PREVIOUS
    TSTB
    BEQ poll_done
    STAB G_PENDING
poll_done:
    RTS

.section .bss, bss
state_begin: .space 1536
