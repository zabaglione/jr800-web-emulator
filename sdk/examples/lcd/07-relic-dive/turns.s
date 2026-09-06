; SPDX-License-Identifier: MIT
.section .text, code
world_direction:
    CLR ACTION
    CLR G_MESSAGE
    LDAB G_KEY
    STAB LAST_DIRECTION
    DECB
    LDX #direction_dx
    ABX
    LDAA G_X
    ADDA 0,X
    STAA NX
    LDX #direction_dy
    ABX
    LDAA G_Y
    ADDA 0,X
    STAA NY
    CMPB #6
    BCS direction_ready
    ; A diagonal requires both orthogonal side cells to be floor.
    LDAA NX
    PSHA
    LDAA G_X
    STAA NX
    JSR walkable
    TAB
    PULA
    STAA NX
    TSTB
    BEQ direction_blocked
    LDAA NY
    PSHA
    LDAA G_Y
    STAA NY
    JSR walkable
    TAB
    PULA
    STAA NY
    TSTB
    BEQ direction_blocked
direction_ready:
    JSR walkable
    TSTA
    BNE direction_open
direction_blocked:
    RTS
direction_open:
    JSR find_enemy
    TSTA
    BEQ player_move
    ; Melee never repeats while held, including the final killing blow.
    STAA CANDIDATE
    LDAA G_ATTACK
    TST G_BUFF
    BEQ attack_unbuffed
    INCA
attack_unbuffed:
    LDAB CANDIDATE
    DECB
    LDX #enemy_defense
    ABX
    SUBA 0,X
attack_damage:
    TSTA
    BNE attack_positive
    INCA
attack_positive:
    STAA DAMAGE
    LDX E_PTR
    LDAA 3,X
    SUBA DAMAGE
    BHI enemy_wounded
    CLR 3,X
    INC G_KILLS
    LDAA #3
    STAA G_MESSAGE
    LDAA G_LEVEL
    CMPA #32
    BCC attack_committed
    LDAA G_KILLS
    CMPA #3
    BCS attack_committed
    CLR G_KILLS
    INC G_LEVEL
    LDAA G_MAX_HP
    ADDA #2
    STAA G_MAX_HP
    LDAA G_HP
    ADDA #2
    STAA G_HP
    LDAA #4
    STAA G_MESSAGE
    BRA attack_committed
enemy_wounded:
    STAA 3,X
    LDAA #2
    STAA G_MESSAGE
attack_committed:
    JMP commit_turn
player_move:
    LDAA NX
    STAA G_X
    LDAA NY
    STAA G_Y
    JSR pickup
    LDAA G_MODE
    CMPA #9
    BEQ player_won
    LDAA G_KEY
    CMPA #4
    BHI player_won
    LDAA #1
    STAA G_AUTOWALK
player_won:
    JMP commit_turn

walkable:
    LDAA NX
    CMPA #24
    BCC not_walkable
    LDAB NY
    CMPB #16
    BCC not_walkable
    JSR cell
    ANDA #127
    RTS
not_walkable:
    CLRA
    RTS

commit_turn:
    LDD G_TURNS
    ADDD #1
    STD G_TURNS
    LDAA G_MODE
    CMPA #9
    BNE turn_continues
    CLR G_AUTOWALK
    RTS
turn_continues:
    TST G_HP
    BNE turn_player_alive
    JMP turn_die
turn_player_alive:
    TST G_BUFF
    BEQ turn_food
    DEC G_BUFF
turn_food:
    TST G_POISON
    BEQ poison_tick_done
    DEC G_POISON
    INC G_POISON_TICK
    LDAA G_POISON_TICK
    ANDA #1
    BNE poison_tick_done
    TST G_HP
    BEQ poison_tick_done
    DEC G_HP
    LDAA #16
    STAA G_MESSAGE
poison_tick_done:
    TST G_HP
    BNE poison_survived
    JMP turn_die
poison_survived:
    TST G_FOOD
    BEQ starving
    DEC G_FOOD
    CLR G_STARVE
    INC G_REGEN
    LDAA G_REGEN
    CMPA #12
    BCS turn_enemies
    CLR G_REGEN
    LDAA G_HP
    CMPA G_MAX_HP
    BCC turn_enemies
    INC G_HP
    BRA turn_enemies
starving:
    CLR G_REGEN
    INC G_STARVE
    LDAA G_STARVE
    ANDA #1
    BNE turn_enemies
    TST G_HP
    BEQ turn_enemies
    DEC G_HP
turn_enemies:
    LDAA G_MODE
    CMPA #9
    BNE turn_not_won
    CLR G_AUTOWALK
    RTS
turn_not_won:
    JSR update_visibility
    TST NEW_SIGHT
    BEQ no_new_sight
    CLR G_AUTOWALK
no_new_sight:
    JSR enemies_turn
    TST G_HP
    BNE turn_alive
turn_die:
    CLR G_SUSPEND
    CLR G_AUTOWALK
    LDAA #8
    STAA G_MODE
turn_alive:
    LDAA G_FOOD
    CMPA #40
    BHI turn_done
    CLR G_AUTOWALK
    TST G_MESSAGE
    BNE turn_done
    LDAA #5
    STAA G_MESSAGE
turn_done:
    RTS

enemies_turn:
    CLR E_INDEX
enemy_turn_loop:
    JSR poll_key
    TST G_HP
    BNE enemy_player_alive
    RTS
enemy_player_alive:
    JSR enemy_pointer
    TST 3,X
    BNE enemy_active
    JMP enemy_turn_next
enemy_active:
    LDAA 2,X
    STAA ENEMY_KIND
    CMPA #8
    BNE enemy_no_regen
    LDAA G_TURNS + 1
    ANDA #3
    BNE enemy_no_regen
    LDAA 3,X
    CMPA #12
    BCC enemy_no_regen
    INC 3,X
enemy_no_regen:
    LDAA ENEMY_KIND
    CMPA #1
    BNE enemy_awake
    LDAA G_TURNS + 1
    ANDA #1
    BEQ enemy_awake
    JMP enemy_turn_next
enemy_awake:
    LDAA 0,X
    LDAB 1,X
    JSR visible_cell
    TST 0,X
    BEQ enemy_memory
    LDAA ENEMY_KIND
    CMPA #12
    BNE enemy_not_ranged
    JSR enemy_pointer
    LDAA 0,X
    CMPA G_X
    BEQ enemy_shoot
    LDAA 1,X
    CMPA G_Y
    BNE enemy_not_ranged
enemy_shoot:
    JSR enemy_strike
    LDAA #15
    STAA G_MESSAGE
    JMP enemy_turn_next
enemy_not_ranged:
    JSR enemy_pointer
    LDAA G_X
    STAA 4,X
    LDAA G_Y
    STAA 5,X
enemy_memory:
    JSR enemy_pointer
    LDAA 4,X
    CMPA #255
    BNE enemy_has_target
    JMP enemy_turn_next
enemy_has_target:
    LDAA ENEMY_KIND
    CMPA #9
    BNE enemy_not_fleeing
    TST 6,X
    BEQ enemy_not_fleeing
    LDAA #23
    LDAB 0,X
    CMPB G_X
    BHI flee_x
    CLRA
flee_x:
    STAA 4,X
    LDAA #15
    LDAB 1,X
    CMPB G_Y
    BHI flee_y
    CLRA
flee_y:
    STAA 5,X
enemy_not_fleeing:
    LDAA ENEMY_KIND
    CMPA #6
    BNE enemy_chase
    JSR random
    ANDA #3
    BNE bat_chase
    JSR random
    ANDA #3
    TAB
    LDX #direction_dx
    ABX
    LDAA 0,X
    STAA NX
    LDX #direction_dy
    ABX
    LDAA 0,X
    STAA NY
    JSR enemy_pointer
    LDAA NX
    ADDA 0,X
    STAA NX
    LDAA NY
    ADDA 1,X
    STAA NY
    JSR enemy_step
    JMP enemy_turn_next
bat_chase:
    JSR enemy_pointer
enemy_chase:
    LDAA 0,X
    STAA NX
    LDAB 1,X
    STAB NY
    CMPA 4,X
    BEQ enemy_try_y
    BCS enemy_go_right
    DEC NX
    BRA enemy_x_ready
enemy_go_right:
    INC NX
enemy_x_ready:
    JSR enemy_step
    TSTA
    BNE enemy_turn_next
    JSR enemy_pointer
    LDAA 0,X
    STAA NX
enemy_try_y:
    LDAA 1,X
    CMPA 5,X
    BEQ enemy_target_reached
    BCS enemy_go_down
    DEC NY
    BRA enemy_y_ready
enemy_go_down:
    INC NY
enemy_y_ready:
    JSR enemy_step
    BRA enemy_turn_next
enemy_target_reached:
    LDAA #255
    STAA 4,X
    STAA 5,X
enemy_turn_next:
    INC E_INDEX
    LDAA E_INDEX
    CMPA #MAX_ENEMIES
    BEQ enemies_done
    JMP enemy_turn_loop
enemies_done:
    RTS

; Try one cardinal step or strike. Does not advance time recursively.
enemy_step:
    JSR walkable
    TSTA
    BNE far_enemy_345
    JMP enemy_step_no
far_enemy_345:
    LDAA NX
    CMPA G_X
    BEQ far_enemy_348
    JMP enemy_empty
far_enemy_348:
    LDAA NY
    CMPA G_Y
    BEQ far_enemy_351
    JMP enemy_empty
far_enemy_351:
enemy_strike:
    JSR enemy_pointer
    LDAB 2,X
    STAB CANDIDATE
    DECB
    LDX #enemy_attack
    ABX
    LDAA 0,X
    ADDA G_DIFFICULTY
    DECA
    SUBA G_DEFENSE
    BHI enemy_damage_positive
    LDAA #1
enemy_damage_positive:
    STAA DAMAGE
    LDAA G_HP
    SUBA DAMAGE
    BHI enemy_damage_alive
    CLRA
enemy_damage_alive:
    STAA G_HP
    LDAA CANDIDATE
    CMPA #10
    BNE strike_not_poison
    LDAA #6
    STAA G_POISON
    CLR G_POISON_TICK
    LDAA #12
    STAA G_MESSAGE
    BRA enemy_step_yes
strike_not_poison:
    CMPA #11
    BNE strike_not_rust
    TST G_DEFENSE
    BEQ enemy_step_yes
    DEC G_DEFENSE
    DEC G_ARMOR
    TST G_DEFENSE
    BNE rust_message
    CLR G_ARMOR
rust_message:
    LDAA #13
    STAA G_MESSAGE
    BRA enemy_step_yes
strike_not_rust:
    CMPA #9
    BNE strike_not_theft
    JSR enemy_pointer
    TST 6,X
    BNE enemy_step_yes
    INC 6,X
    LDD G_GOLD
    SUBD #10
    BCC theft_gold
    CLRA
    CLRB
theft_gold:
    STD G_GOLD
    LDAA #14
    STAA G_MESSAGE
    BRA enemy_step_yes
strike_not_theft:
    CMPA #4
    BNE enemy_step_yes
    LDAA G_FOOD
    SUBA #3
    BCC wraith_food
    CLRA
wraith_food:
    STAA G_FOOD
    BRA enemy_step_yes
enemy_empty:
    JSR find_enemy
    TSTA
    BEQ far_enemy_426
    JMP enemy_step_no
far_enemy_426:
    JSR enemy_pointer
    LDAA NX
    STAA 0,X
    LDAA NY
    STAA 1,X
enemy_step_yes:
    LDAA #1
    RTS
enemy_step_no:
    CLRA
    RTS

; Repeat only through a known straight corridor, and never past a visible foe.
safe_walk:
    CLR SAFE_FLAG
    TST G_MESSAGE
    BEQ safe_no_message
    RTS
safe_no_message:
    LDAA G_FOOD
    CMPA #40
    BHI long_t302
    JMP safe_done
long_t302:
    LDAA G_MODE
    CMPA #1
    BEQ long_t305
    JMP safe_done
long_t305:
    CLR E_INDEX
safe_enemy_loop:
    JSR enemy_pointer
    TST 3,X
    BEQ safe_enemy_next
    LDAA 0,X
    LDAB 1,X
    JSR visible_cell
    TST 0,X
    BEQ long_t315
    JMP safe_done
long_t315:
safe_enemy_next:
    INC E_INDEX
    LDAA E_INDEX
    CMPA #MAX_ENEMIES
    BNE safe_enemy_loop
    ; Both side cells must be walls; ahead and behind must be ordinary floor.
    LDAA G_X
    STAA NX
    LDAA G_Y
    STAA NY
    CLR NEIGHBORS
    CLR CHECK_X
safe_neighbors:
    LDAA G_X
    LDAB G_Y
    LDX #direction_dx
    PSHB
    LDAB CHECK_X
    ABX
    ADDA 0,X
    STAA NX
    LDX #direction_dy
    ABX
    PULB
    ADDB 0,X
    STAB NY
    JSR walkable
    TSTA
    BEQ safe_neighbor_next
    INC NEIGHBORS
safe_neighbor_next:
    INC CHECK_X
    LDAA CHECK_X
    CMPA #4
    BNE safe_neighbors
    LDAA NEIGHBORS
    CMPA #2
    BEQ long_t353
    JMP safe_done
long_t353:
    LDAA G_X
    STAA NX
    LDAA G_Y
    STAA NY
    LDAB LAST_DIRECTION
    DECB
    LDX #direction_dx
    ABX
    LDAA NX
    ADDA 0,X
    STAA NX
    LDX #direction_dy
    ABX
    LDAA NY
    ADDA 0,X
    STAA NY
    TAB
    LDAA NX
    JSR cell
    CMPA #129
    BEQ long_t374
    JMP safe_done
long_t374:
    JSR find_item
    TSTA
    BEQ long_t377
    JMP safe_done
long_t377:
    INC SAFE_FLAG
safe_done:
    RTS
