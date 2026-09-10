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
    JMP draw_hand
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
    LDAA input_event
    BITA #16
    BNE card_activate
    BITA #5
    BEQ card_right
    TST selected
    BEQ card_wrap_left
    DEC selected
    BRA card_changed
card_wrap_left:
    LDAA #2
    STAA selected
    BRA card_changed
card_right:
    BITA #10
    BEQ card_idle
    INC selected
    LDAA selected
    CMPA #3
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
    JMP card_changed
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
    CMPA #1
    BEQ end_turn
    LDAA card_help
    EORA #1
    STAA card_help
    RTS
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
    LDX #battle_label
    CLRA
    CLRB
    JSR paint_text
    LDAA #36
    STAA paint_x
    CLR paint_band
    LDAA battle
    INCA
    JSR paint_number
    LDX #hp_label
    LDAA #66
    CLRB
    JSR paint_text
    LDAA #84
    STAA paint_x
    CLR paint_band
    LDAA hp
    JSR paint_number
    LDX #energy_label
    LDAA #126
    CLRB
    JSR paint_text
    LDAA #144
    STAA paint_x
    CLR paint_band
    LDAA energy
    JSR paint_number
    TST battle_mode
    BEQ render_enemy
    LDX #reward_label
    CLRA
    LDAB #1
    JSR paint_text
    LDX #reward_help
    CLRA
    LDAB #2
    JSR paint_text
    JMP render_cards
render_enemy:
    LDX #enemy_label
    CLRA
    LDAB #1
    JSR paint_text
    LDAA #42
    STAA paint_x
    LDAA #1
    STAA paint_band
    LDAA enemy_hp
    JSR paint_number
    LDX #shield_label
    LDAA #72
    LDAB #1
    JSR paint_text
    LDAA #84
    STAA paint_x
    LDAA #1
    STAA paint_band
    LDAA enemy_block
    JSR paint_number
    LDX #block_label
    LDAA #126
    LDAB #1
    JSR paint_text
    LDAA #156
    STAA paint_x
    LDAA #1
    STAA paint_band
    LDAA block
    JSR paint_number
    LDX #intent_attack
    LDAA intent
    CMPA #1
    BNE render_intent
    LDX #intent_defend
render_intent:
    CLRA
    LDAB #2
    JSR paint_text
    LDAA #108
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA #6
    LDAB intent
    CMPB #1
    BEQ render_intent_number
    LDAA battle
    ADDA #4
    ADDA stage
    TST intent
    BEQ render_intent_number
    ADDA #3
render_intent_number:
    JSR paint_number
render_cards:
    CLR render_slot
render_card_loop:
    LDAB render_slot
    LDX #hand
    TST battle_mode
    BEQ render_card_load
    LDX #rewards
render_card_load:
    ABX
    LDAA 0,X
    STAA card_id
    LDAA render_slot
    LDAB #64
    MUL
    STAB card_x
    LDX #card_border
    TST card_id
    LDAA card_x
    LDAB #3
    JSR paint_text
    LDAA card_id
    CMPA #255
    BNE render_card_name
    LDX #empty_card
    BRA render_name
render_card_name:
    LDAB #8
    MUL
    ADDD #card_names
    XGDX
render_name:
    LDAA card_x
    LDAB #4
    JSR paint_text
    LDX #card_cost_label
    LDAA card_x
    LDAB #5
    JSR paint_text
    LDAA card_x
    ADDA #36
    STAA paint_x
    LDAA #5
    STAA paint_band
    CLR card_attack
    CLR card_cost
    LDAA card_id
    CMPA #255
    BEQ render_card_numbers
    LDAB #8
    MUL
    ADDD #card_stats
    XGDX
    LDAA 0,X
    STAA card_cost
    LDAA 7,X
    STAA card_attack
render_card_numbers:
    LDAA card_cost
    JSR paint_number
    LDAA card_id
    CMPA #255
    BNE render_effect_name
    LDX #empty_card
    BRA render_effect
render_effect_name:
    LDAB #8
    MUL
    ADDD #effect_names
    XGDX
render_effect:
    LDAA card_x
    LDAB #6
    JSR paint_text
    LDX #card_border
    LDAA render_slot
    CMPA selected
    BNE render_selection
    LDX #card_selected
render_selection:
    LDAA card_x
    LDAB #7
    JSR paint_text
    INC render_slot
    LDAA render_slot
    CMPA #3
    BEQ render_help
    JMP render_card_loop
render_help:
    TST card_help
    BEQ render_done
    LDX #deck_help
    CLRA
    LDAB #2
    JMP paint_text
render_done:
    RTS
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
