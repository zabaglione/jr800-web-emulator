; SPDX-License-Identifier: MIT
; One 768-byte floor: terrain 384, 32 enemies x 8, 32 items x 3, 32 spare.
; Terrain bit 7 remembers exploration. Descent discards the previous floor.
.section .text, code
select_floor:
    LDX #FLOORS
    STX FP
    RTS

; Full-period 16-bit LCG: state = 25173 * state + 13849 (mod 65536).
; Returning the high byte avoids the short low-bit cycles of power-of-two LCGs.
; Three 8x8 MUL operations compute the low 16 product bits; $AA-$AB scratch.
random:
    LDAA G_RNG + 1
    LDAB #$55
    MUL
    STD $AA
    LDAA G_RNG
    LDAB #$55
    MUL
    ADDB $AA
    STAB $AA
    LDAA G_RNG + 1
    LDAB #$62
    MUL
    ADDB $AA
    STAB $AA
    LDD $AA
    ADDD #$3619
    STD G_RNG
    RTS

; A=x, B=y -> X=terrain address, A=value. All positions are bounded first.
cell:
    STAA CX
    TBA
    LDAB #24
    MUL
    ADDB CX
    ADCA #0
    ADDD FP
    STD CP
    LDX CP
    LDAA 0,X
    RTS

; A=x, B=y -> X=current visibility byte.
visible_cell:
    STAA CX
    TBA
    LDAB #24
    MUL
    ADDB CX
    ADCA #0
    ADDD #VISIBLE
    STD CP
    LDX CP
    RTS

; Clear the floor, generate connected terrain, then populate it.
generate_world:
    LDAA G_FLOOR
    STAA GEN_FLOOR
    JSR select_floor
    LDX FP
    STX P1
    LDD #768
    STD TEMP_WORD
    CLRA
gen_zero:
    LDX P1
    STAA 0,X
    INX
    STX P1
    LDD TEMP_WORD
    SUBD #1
    STD TEMP_WORD
    BNE gen_zero_more
    BRA gen_rooms
gen_zero_more:
    CLRA
    BRA gen_zero
gen_rooms:
    JSR terrain_layout
    ; Guaranteed resources, then varied consumables, gear, and gold.
    LDAA #1
    TST G_DIFFICULTY
    BNE food_count_ready
    INCA
food_count_ready:
    STAA FOOD_COUNT
    CLR PLACE_INDEX
    LDAA #1
    STAA PLACE_TYPE
gen_food:
    JSR place_item
    DEC FOOD_COUNT
    BNE gen_food
    LDAA #2
    STAA PLACE_TYPE
gen_consumable:
    JSR place_item
    INC PLACE_TYPE
    LDAA PLACE_TYPE
    CMPA #7
    BNE gen_consumable
    ; Tier rises on floors 3 and 5, capped at the third tier.
    LDAA GEN_FLOOR
    LSRA
    CMPA #2
    BLS gear_tier
    LDAA #2
gear_tier:
    ADDA #7
    STAA PLACE_TYPE
    JSR place_item
    LDAA PLACE_TYPE
    ADDA #3
    STAA PLACE_TYPE
    JSR place_item
    LDAA #13
    STAA PLACE_TYPE
    JSR place_item
    JSR place_item
    JSR place_item
    LDAA G_DIFFICULTY
    ASLA
    ADDA G_FLOOR
    ADDA #4
    CMPA #MAX_ENEMIES
    BLS population_ready
    LDAA #MAX_ENEMIES
population_ready:
    STAA G_ENEMY_COUNT
    CLR E_INDEX
gen_enemy:
    JSR free_position
    ; Leave an arrival area outside enemy sight, including ranged attackers.
    LDAA NX
    SUBA G_X
    BPL arrival_dx
    NEGA
arrival_dx:
    STAA TEMP
    LDAA NY
    SUBA G_Y
    BPL arrival_dy
    NEGA
arrival_dy:
    ADDA TEMP
    CMPA #6
    BCS gen_enemy
    JSR enemy_pointer
    LDAA NX
    STAA 0,X
    LDAA NY
    STAA 1,X
    LDAA G_FLOOR
    ASLA
    ADDA #4
    CMPA #12
    BLS kind_limit_ready
    LDAA #12
kind_limit_ready:
    STAA KIND_LIMIT
enemy_roll_kind:
    JSR random
    ANDA #15
    CMPA KIND_LIMIT
    BCC enemy_roll_kind
    TAB
    LDX #enemy_distribution
    ABX
    LDAA 0,X
    STAA CANDIDATE
    JSR enemy_pointer
    STAA 2,X
    LDAB CANDIDATE
    DECB
    LDX #enemy_hp
    ABX
    LDAA 0,X
    JSR enemy_pointer
    STAA 3,X
    LDAA #255
    STAA 4,X
    STAA 5,X
    INC E_INDEX
    LDAA E_INDEX
    CMPA G_ENEMY_COUNT
    BNE gen_enemy
    RTS

; A random empty floor cell. Generated
; Room and corridor cells are reserved before bounded populations are placed.
free_position:
    JSR poll_key
    JSR random
    ANDA #31
    CMPA #23
    BCC free_position
    TSTA
    BEQ free_position
    STAA NX
    JSR random
    ANDA #15
    TSTA
    BEQ free_position
    CMPA #15
    BEQ free_position
    STAA NY
    TAB
    LDAA NX
    JSR cell
    ANDA #127
    CMPA #1
    BNE free_position
    LDAA NX
    CMPA G_X
    BNE position_not_entry
    LDAA NY
    CMPA G_Y
    BEQ free_position
position_not_entry:
    JSR find_enemy
    TSTA
    BNE free_position
    JSR find_item
    TSTA
    BNE free_position
    RTS

place_item:
    JSR free_position
    LDAB PLACE_INDEX
    LDAA #3
    MUL
    ADDD FP
    ADDD #ITEM_START
    STD ITEM_PTR
    LDX ITEM_PTR
    LDAA NX
    STAA 0,X
    LDAA NY
    STAA 1,X
    LDAA PLACE_TYPE
    STAA 2,X
    INC PLACE_INDEX
    RTS

; Enemy lookup at NX,NY. Returns A=kind/0 and E_PTR if found.
find_enemy:
    LDD FP
    ADDD #384
    STD E_PTR
    LDAB #MAX_ENEMIES
find_enemy_loop:
    LDX E_PTR
    TST 3,X
    BEQ find_enemy_next
    LDAA 0,X
    CMPA NX
    BNE find_enemy_next
    LDAA 1,X
    CMPA NY
    BNE find_enemy_next
    LDAA 2,X
    RTS
find_enemy_next:
    LDD E_PTR
    ADDD #8
    STD E_PTR
    ; B was clobbered by pointer arithmetic. Use end address instead.
    SUBD FP
    SUBD #ITEM_START
    BNE find_enemy_loop
    CLRA
    RTS

find_item:
    LDD FP
    ADDD #ITEM_START
    STD ITEM_PTR
find_item_loop:
    LDX ITEM_PTR
    TST 2,X
    BEQ find_item_next
    LDAA 0,X
    CMPA NX
    BNE find_item_next
    LDAA 1,X
    CMPA NY
    BNE find_item_next
    LDAA 2,X
    RTS
find_item_next:
    LDD ITEM_PTR
    ADDD #3
    STD ITEM_PTR
    SUBD FP
    SUBD #FLOOR_DATA_END
    BNE find_item_loop
    CLRA
    RTS

enemy_pointer:
    PSHA
    LDAA E_INDEX
    LDAB #8
    MUL
    ADDD FP
    ADDD #384
    STD E_PTR
    LDX E_PTR
    PULA
    RTS

; Manhattan-radius 5 line of sight with integer Bresenham rays. The target
; wall is visible; cells behind it are not. No per-turn random calls.
update_visibility:
    CLR NEW_SIGHT
    LDX #VISIBLE
    CLRA
vis_clear:
    STAA 0,X
    INX
    CPX #VISIBLE + 384
    BNE vis_clear
    LDAA G_X
    SUBA #5
    BCC vis_start_x
    CLRA
vis_start_x:
    STAA LOS_START_X
    LDAA G_X
    ADDA #5
    CMPA #23
    BLS vis_end_x
    LDAA #23
vis_end_x:
    STAA LOS_END_X
    LDAA G_Y
    ADDA #5
    CMPA #15
    BLS vis_end_y
    LDAA #15
vis_end_y:
    STAA LOS_END_Y
    LDAA G_Y
    SUBA #5
    BCC vis_start_y
    CLRA
vis_start_y:
    STAA LOS_TY
vis_y:
    JSR poll_key
    LDAA LOS_START_X
    STAA LOS_TX
vis_x:
    LDAA LOS_TX
    SUBA G_X
    BPL vis_dx
    NEGA
vis_dx:
    STAA LOS_DX
    LDAA LOS_TY
    SUBA G_Y
    BPL vis_dy
    NEGA
vis_dy:
    ADDA LOS_DX
    CMPA #5
    BHI vis_next
    JSR line_visible
    TSTA
    BEQ vis_next
    LDAA LOS_TX
    LDAB LOS_TY
    JSR visible_cell
    LDAA #1
    STAA 0,X
    LDAA LOS_TX
    LDAB LOS_TY
    JSR cell
    BITA #128
    BNE vis_remember
    INC NEW_SIGHT
vis_remember:
    ORAA #128
    STAA 0,X
vis_next:
    INC LOS_TX
    LDAA LOS_TX
    CMPA LOS_END_X
    BLS vis_x
    INC LOS_TY
    LDAA LOS_TY
    CMPA LOS_END_Y
    BLS vis_y
    RTS

line_visible:
    LDAA G_X
    STAA LOS_X
    LDAA G_Y
    STAA LOS_Y
    LDAA #1
    STAA LOS_SX
    STAA LOS_SY
    LDAA LOS_TX
    SUBA G_X
    BPL line_dx
    NEGA
    NEG LOS_SX
line_dx:
    STAA LOS_DX
    LDAA LOS_TY
    SUBA G_Y
    BPL line_dy
    NEGA
    NEG LOS_SY
line_dy:
    STAA LOS_DY
    LDAA LOS_DX
    SUBA LOS_DY
    STAA LOS_ERROR
line_loop:
    LDAA LOS_X
    CMPA LOS_TX
    BNE line_block
    LDAA LOS_Y
    CMPA LOS_TY
    BEQ line_yes
line_block:
    LDAA LOS_X
    LDAB LOS_Y
    JSR cell
    ANDA #127
    BEQ line_no
    LDAA LOS_ERROR
    ASLA
    STAA LOS_TWICE
    LDAA LOS_DY
    NEGA
    CMPA LOS_TWICE
    BGE line_skip_x
    LDAA LOS_ERROR
    SUBA LOS_DY
    STAA LOS_ERROR
    LDAA LOS_X
    ADDA LOS_SX
    STAA LOS_X
line_skip_x:
    LDAA LOS_TWICE
    CMPA LOS_DX
    BGE line_loop
    LDAA LOS_ERROR
    ADDA LOS_DX
    STAA LOS_ERROR
    LDAA LOS_Y
    ADDA LOS_SY
    STAA LOS_Y
    BRA line_loop
line_yes:
    LDAA #1
    RTS
line_no:
    CLRA
    RTS
