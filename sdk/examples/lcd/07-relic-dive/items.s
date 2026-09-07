; SPDX-License-Identifier: MIT
.section .text, code
pickup:
    LDAA G_X
    STAA NX
    LDAA G_Y
    STAA NY
    TAB
    LDAA NX
    JSR cell
    ANDA #127
    CMPA #4
    BNE pickup_item
    LDAA #1
    STAA G_TREASURE
    LDAA #9
    STAA G_MODE
    CLR G_SUSPEND
    LDAA #129
    STAA 0,X
    LDAA #6
    STAA G_MESSAGE
pickup_item:
    JSR find_item
    TSTA
    BEQ pickup_done
    CMPA #13
    BEQ pickup_gold
    STAA ITEM_TYPE
    LDX #G_BAG
pickup_slot:
    TST 0,X
    BEQ pickup_store
    INX
    CPX #G_BAG + 6
    BNE pickup_slot
    LDAA #8
    STAA G_MESSAGE
    RTS
pickup_store:
    LDAA ITEM_TYPE
    STAA 0,X
    LDAA #7
    STAA G_MESSAGE
    BRA pickup_remove
pickup_gold:
    LDAA #7
    STAA G_MESSAGE
    LDD G_GOLD
    ADDD #10
    BCC pickup_gold_store
    LDD #$FFFF
pickup_gold_store:
    STD G_GOLD
pickup_remove:
    LDX ITEM_PTR
    CLR 2,X
pickup_done:
    RTS

; Select an empty item record at the player position, rejecting overlap/full.
; A=1 on success; no mutation until the caller commits the drop.
drop_location:
    LDAA G_X
    STAA NX
    LDAA G_Y
    STAA NY
    JSR find_item
    TSTA
    BNE drop_unavailable
    LDD FP
    ADDD #ITEM_START
    STD ITEM_PTR
drop_find_slot:
    LDX ITEM_PTR
    TST 2,X
    BEQ drop_available
    LDD ITEM_PTR
    ADDD #3
    STD ITEM_PTR
    SUBD FP
    SUBD #FLOOR_DATA_END
    BNE drop_find_slot
drop_unavailable:
    CLRA
    RTS
drop_available:
    LDAA #1
    RTS

write_drop:
    LDX ITEM_PTR
    LDAA G_X
    STAA 0,X
    LDAA G_Y
    STAA 1,X
    LDAA CANDIDATE
    STAA 2,X
    RTS

bag_item:
    LDX #G_BAG
    LDAB G_SLOT
    ABX
    LDAA 0,X
    STAA ITEM_TYPE
    RTS

use_item:
    JSR bag_item
    CMPA #7
    BCS long_i106
    JMP equip_item
long_i106:
    CMPA #1
    BNE use_magic
    LDX #difficulty_restore
    LDAB G_DIFFICULTY
    ABX
    LDAA G_FOOD
    ADDA 0,X
    BCC food_restored
    LDAA #255
food_restored:
    STAA G_FOOD
    BRA consume_item
use_magic:
    SUBA #2
    TAB
    LDX #bits
    ABX
    LDAA G_KNOWN
    ORAA 0,X
    STAA G_KNOWN
    LDX #G_IDENTITIES
    ABX
    LDAA 0,X
    STAA ITEM_EFFECT
    BEQ heal_item
    CMPA #1
    BEQ poison_item
    CMPA #2
    BEQ strength_item
    CMPA #3
    BEQ map_item
    JSR teleport_item
    BRA consume_item
heal_item:
    CLR G_POISON
    CLR G_POISON_TICK
    LDX #difficulty_heal
    LDAB G_DIFFICULTY
    ABX
    LDAA G_HP
    ADDA 0,X
    CMPA G_MAX_HP
    BLS healing_ready
    LDAA G_MAX_HP
healing_ready:
    STAA G_HP
    BRA consume_item
poison_item:
    LDAA G_HP
    SUBA #3
    BHI poisoned_alive
    CLRA
poisoned_alive:
    STAA G_HP
    BRA consume_item
strength_item:
    ; This action consumes the first tick, leaving 12 future player actions.
    LDAA #13
    STAA G_BUFF
    BRA consume_item
map_item:
    LDX FP
    STX P1
map_next:
    LDX P1
    LDAA 0,X
    ORAA #128
    STAA 0,X
    INX
    STX P1
    LDD P1
    SUBD FP
    SUBD #384
    BNE map_next
consume_item:
    JSR bag_item
    CLR 0,X
    LDAA G_MENU
    CMPA #1
    BEQ consumed_discard
    LDAA ITEM_TYPE
    CMPA #7
    BCC consumed_equipment
    CMPA #1
    BEQ consumed_food
    LDAA ITEM_EFFECT
    ADDA #17
    BRA consumed_message
consumed_food:
    LDAA #9
    BRA consumed_message
consumed_equipment:
    LDAA #22
    BRA consumed_message
consumed_discard:
    LDAA #23
consumed_message:
    STAA G_MESSAGE
    LDAA #1
    STAA G_MODE
    JMP commit_turn

equip_item:
    CMPA #10
    BCC equip_armor
    LDAA G_WEAPON
    BRA equip_prepare
equip_armor:
    LDAA G_ARMOR
equip_prepare:
    STAA CANDIDATE
    TSTA
    BEQ equip_ready
    JSR drop_location
    TSTA
    BNE equip_drop
    LDAA #10
    STAA G_MESSAGE
    RTS
equip_drop:
    JSR write_drop
equip_ready:
    LDAA ITEM_TYPE
    CMPA #10
    BCC armor_ready
    STAA G_WEAPON
    SUBA #3
    STAA G_ATTACK
    BRA consume_item
armor_ready:
    STAA G_ARMOR
    SUBA #9
    STAA G_DEFENSE
    BRA consume_item

discard_item:
    JSR bag_item
    STAA CANDIDATE
    JSR drop_location
    TSTA
    BNE discard_ready
    LDAA #10
    STAA G_MESSAGE
    RTS
discard_ready:
    JSR write_drop
    JMP consume_item

; Deterministic bounded scan. Teleport selects an
; explored ordinary floor with no entity/item and no adjacent enemy.
teleport_item:
    CLR CHECK_X
    CLR CHECK_Y
teleport_scan:
    LDAA CHECK_X
    STAA NX
    LDAB CHECK_Y
    STAB NY
    JSR cell
    CMPA #129
    BNE teleport_next
    JSR find_item
    TSTA
    BNE teleport_next
    JSR find_enemy
    TSTA
    BNE teleport_next
    CLR E_INDEX
teleport_enemy:
    JSR enemy_pointer
    TST 3,X
    BEQ teleport_enemy_next
    LDAA 0,X
    SUBA NX
    BPL teleport_dx
    NEGA
teleport_dx:
    STAA TEMP
    LDAA 1,X
    SUBA NY
    BPL teleport_dy
    NEGA
teleport_dy:
    ADDA TEMP
    CMPA #1
    BLS teleport_next
teleport_enemy_next:
    INC E_INDEX
    LDAA E_INDEX
    CMPA #MAX_ENEMIES
    BNE teleport_enemy
    LDAA NX
    STAA G_X
    LDAA NY
    STAA G_Y
    RTS
teleport_next:
    INC CHECK_X
    LDAA CHECK_X
    CMPA #24
    BNE teleport_scan
    CLR CHECK_X
    INC CHECK_Y
    LDAA CHECK_Y
    CMPA #16
    BNE teleport_scan
    RTS

stairs_action:
    LDAA G_X
    LDAB G_Y
    JSR cell
    ANDA #127
    CMPA #3
    BNE stairs_done
    INC G_FLOOR
    JSR generate_world
    LDAA #1
    STAA G_MODE
    CLR G_MESSAGE
    JSR select_floor
    JMP commit_turn
stairs_done:
    RTS
