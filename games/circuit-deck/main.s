; SPDX-License-Identifier: MIT
.global hp
.global energy
.global rewards
.global hand
.global deck
.global deck_size
.global draw_count
.global discard_count
.global battle
.global battle_mode
.global enemy_hp
.global enemy_block
.global block
.global poison
.global strength
.global selected
.global rounds
.global shuffles
.global card_id
.global intent
.section .text, code
game_start:
    CLR battle
    CLR rounds
    CLR shuffles
    CLR card_help
    LDAA #40
    STAA hp
    LDAA #8
    STAA deck_size
    LDX #deck
    CLRA
    STAA 0,X
    STAA 1,X
    STAA 2,X
    INCA
    STAA 3,X
    STAA 4,X
    STAA 5,X
    INCA
    STAA 6,X
    LDAA #5
    STAA 7,X
    JMP battle_start
battle_start:
    CLR battle_mode
    CLR selected
    CLR poison
    CLR strength
    CLR block
    CLR enemy_block
    CLR intent
    CLR discard_count
    LDAB battle
    LDX #enemy_health
    ABX
    LDAA stage
    LDAB #3
    MUL
    ADDB 0,X
    STAB enemy_hp
    LDAA deck_size
    STAA draw_count
    CLR card_index
battle_copy:
    LDAB card_index
    LDX #deck
    ABX
    LDAA 0,X
    LDX #draw_pile
    ABX
    STAA 0,X
    INC card_index
    LDAA card_index
    CMPA deck_size
    BNE battle_copy
    JSR shuffle
    JSR draw_hand
    JMP deck_intro_begin
; Fisher-Yates over the active pile, rejection-free bounded modulo.
shuffle:
    INC shuffles
    LDAA draw_count
    DECA
    STAA shuffle_i
shuffle_loop:
    TST shuffle_i
    BEQ shuffle_done
    LDAA shuffle_i
    INCA
    STAA modulus
    JSR random_mod
    TAB
    LDX #draw_pile
    ABX
    STX card_ptr
    LDAA 0,X
    STAA card_temp
    LDAB shuffle_i
    LDX #draw_pile
    ABX
    LDAA 0,X
    LDAB card_temp
    STAB 0,X
    LDX card_ptr
    STAA 0,X
    DEC shuffle_i
    BRA shuffle_loop
shuffle_done:
    RTS
random_mod:
    JSR random
random_reduce:
    CMPA modulus
    BCS random_mod_done
    SUBA modulus
    BRA random_reduce
random_mod_done:
    RTS
draw_hand:
    LDAA #3
    STAA energy
    CLR card_index
draw_hand_loop:
    TST draw_count
    BNE draw_one
    TST discard_count
    BNE recycle_begin
    LDAA #255
    BRA draw_store
recycle_begin:
    LDAA discard_count
    STAA draw_count
    CLRB
recycle_copy:
    LDX #discard
    ABX
    LDAA 0,X
    LDX #draw_pile
    ABX
    STAA 0,X
    INCB
    CMPB draw_count
    BNE recycle_copy
    CLR discard_count
    JSR shuffle
draw_one:
    DEC draw_count
    LDAB draw_count
    LDX #draw_pile
    ABX
    LDAA 0,X
draw_store:
    LDAB card_index
    LDX #hand
    ABX
    STAA 0,X
    INC card_index
    LDAA card_index
    CMPA #3
    BNE draw_hand_loop
    RTS
game_update:
    TST deck_active
    BEQ deck_accept_input
    JMP deck_animation_update
deck_accept_input:
    LDAA input_event
    BITA #16
    BNE card_activate
    TST battle_mode
    BNE card_direction
    BITA #2
    BEQ deck_not_down
    LDAA #3
    STAA selected
    JMP card_changed
deck_not_down:
    BITA #1
    BEQ card_direction
    CLR selected
    JMP card_changed
card_direction:
    BITA #5
    BEQ card_right
    TST selected
    BEQ card_wrap_left
    DEC selected
    BRA card_changed
card_wrap_left:
    LDAA #3
    TST battle_mode
    BEQ deck_wrap_left
    DECA
deck_wrap_left:
    STAA selected
    BRA card_changed
card_right:
    BITA #10
    BEQ card_idle
    INC selected
    LDAA selected
    LDAB #4
    TST battle_mode
    BEQ deck_wrap_right
    DECB
deck_wrap_right:
    CBA
    BCS card_changed
    CLR selected
card_changed:
    LDAA #1
    STAA redraw
card_idle:
    RTS
card_activate:
    TST battle_mode
    BEQ card_play
    LDAB selected
    LDX #rewards
    ABX
    LDAA 0,X
    LDAB deck_size
    LDX #deck
    ABX
    STAA 0,X
    INC deck_size
    INC battle
    LDAA hp
    ADDA #12
    CMPA #40
    BCS reward_heal
    LDAA #40
reward_heal:
    STAA hp
    JSR paint_clear
    JSR battle_start
    BRA card_changed
card_play:
    LDAA selected
    CMPA #3
    BNE deck_play_card
    JMP deck_end_action
deck_play_card:
    LDAB selected
    LDX #hand
    ABX
    LDAA 0,X
    CMPA #255
    BEQ card_idle
    STAA card_id
    LDAB #8
    MUL
    ADDD #card_stats
    STD stats_ptr
    LDX stats_ptr
    LDAA energy
    SUBA 0,X
    BCS card_idle
    STAA energy
    JSR deck_capture
    LDAB selected
    JSR discard_hand_card
    LDX stats_ptr
    LDAA 1,X
    BEQ card_block
    ADDA strength
    JSR deal_damage
card_block:
    LDX stats_ptr
    LDAA 2,X
    ADDA block
    STAA block
    LDAA 3,X
    ADDA hp
    CMPA #40
    BCS card_heal_store
    LDAA #40
card_heal_store:
    STAA hp
    LDAA 4,X
    ADDA poison
    CMPA #99
    BCS poison_cap
    LDAA #99
poison_cap:
    STAA poison
    LDAA 5,X
    ADDA strength
    CMPA #99
    BCS strength_cap
    LDAA #99
strength_cap:
    STAA strength
    LDAA 6,X
    ADDA energy
    STAA energy
    JSR check_battle_win
    LDAA #1
    JMP deck_queue
; A damage consumes enemy shield before HP, saturates at zero.
deal_damage:
    SUBA enemy_block
    BCC damage_health
    NEGA
    STAA enemy_block
    RTS
damage_health:
    CLR enemy_block
    STAA card_temp
    LDAA enemy_hp
    SUBA card_temp
    BCC damage_store
    CLRA
damage_store:
    STAA enemy_hp
    RTS
; B hand slot; played and unplayed cards both enter the discard pile.
discard_hand_card:
    LDX #hand
    ABX
    LDAA 0,X
    CMPA #255
    BEQ discard_done
    STAA card_temp
    LDAA #255
    STAA 0,X
    LDAB discard_count
    LDX #discard
    ABX
    LDAA card_temp
    STAA 0,X
    INC discard_count
discard_done:
    RTS
game_aux:
    TST deck_active
    BEQ deck_aux_ready
    RTS
deck_aux_ready:
    CMPA #1
    BEQ deck_end_action
    LDAA card_help
    EORA #1
    STAA card_help
    CLR deck_message
    TSTA
    BEQ deck_help_done
    LDAA #22
    STAA deck_message
deck_help_done:
    RTS
deck_end_action:
    TST battle_mode
    BEQ deck_end_live
    RTS
deck_end_live:
    JSR deck_capture
    LDAA intent
    STAA deck_intent
    JSR end_turn
    LDAA #2
    JMP deck_queue
end_turn:
    TST battle_mode
    BEQ end_turn_combat
    RTS
end_turn_combat:
    INC rounds
    LDAA enemy_hp
    SUBA poison
    BCC poison_store
    CLRA
poison_store:
    STAA enemy_hp
    JSR check_battle_win
    TST battle_mode
    BNE end_turn_done
    LDAA phase
    CMPA #4
    BEQ end_turn_done
    CLR enemy_block
    LDAA intent
    CMPA #1
    BEQ enemy_defend
    LDAA battle
    ADDA #4
    ADDA stage
    TST intent
    BEQ enemy_attack
    ADDA #3
enemy_attack:
    SUBA block
    BCS enemy_next
    STAA card_temp
    LDAA hp
    SUBA card_temp
    BLS deck_dead
    STAA hp
    BRA enemy_next
enemy_defend:
    LDAA #6
    STAA enemy_block
enemy_next:
    CLR block
    INC intent
    LDAA intent
    CMPA #3
    BCS discard_remaining
    CLR intent
discard_remaining:
    CLRB
    JSR discard_hand_card
    LDAB #1
    JSR discard_hand_card
    LDAB #2
    JSR discard_hand_card
    JSR draw_hand
end_turn_done:
    RTS
deck_dead:
    CLR hp
    LDAA #5
    STAA phase
    RTS
check_battle_win:
    TST enemy_hp
    BNE end_turn_done
    LDAA battle
    CMPA #8
    BNE reward_begin
    LDAA #4
    STAA phase
    RTS
reward_begin:
    LDAA #1
    STAA battle_mode
    CLR selected
    LDAA #12
    STAA modulus
    JSR random_mod
    STAA rewards
    ADDA #4
    CMPA #12
    BCS reward_second
    SUBA #12
reward_second:
    STAA rewards + 1
    ADDA #4
    CMPA #12
    BCS reward_third
    SUBA #12
reward_third:
    STAA rewards + 2
    JSR input_gate
    JMP paint_clear
game_tile:
    CLRA
    RTS
game_render:
    JMP deck_render
.section .bss, bss
hp: .space 1
energy: .space 1
block: .space 1
poison: .space 1
strength: .space 1
battle: .space 1
battle_mode: .space 1
enemy_hp: .space 1
enemy_block: .space 1
intent: .space 1
selected: .space 1
rounds: .space 1
shuffles: .space 1
deck_size: .space 1
draw_count: .space 1
discard_count: .space 1
deck: .space 24
draw_pile: .space 24
discard: .space 24
hand: .space 3
rewards: .space 3
card_index: .space 1
card_ptr: .space 2
card_temp: .space 1
shuffle_i: .space 1
modulus: .space 1
card_id: .space 1
stats_ptr: .space 2
render_slot: .space 1
card_x: .space 1
card_attack: .space 1
card_cost: .space 1
card_help: .space 1
