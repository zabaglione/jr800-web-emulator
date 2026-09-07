; SPDX-License-Identifier: MIT
; Unequal recursive partitions, 4-6 rooms, optional corridor-only partitions.
.section .generation, code
terrain_layout:
    LDAA #1
    STAA GEN_N
    STAA RECT_X
    STAA RECT_Y
    LDAA #22
    STAA RECT_W
    LDAA #14
    STAA RECT_H
terrain_target:
    JSR random
    ANDA #3
    CMPA #3
    BEQ terrain_target
    ADDA #4
    STAA GEN_TARGET
terrain_partition:
    CLR GEN_BIG
    CLR ROOM
    CLRB
terrain_find_split:
    LDX #RECT_W
    ABX
    LDAA 0,X
    CMPA GEN_BIG
    BLS terrain_find_height
    STAA GEN_BIG
    STAB ROOM
    CLR GEN_AXIS
terrain_find_height:
    LDX #RECT_H
    ABX
    LDAA 0,X
    CMPA GEN_BIG
    BLS terrain_find_next
    STAA GEN_BIG
    STAB ROOM
    LDAA #1
    STAA GEN_AXIS
terrain_find_next:
    INCB
    CMPB GEN_N
    BNE terrain_find_split
    LDAA GEN_BIG
    CMPA #11
    BCS terrain_partitions_done
terrain_cut:
    JSR random
    ANDA #31
    ADDA #5
    STAA GEN_CUT
    ADDA #6
    CMPA GEN_BIG
    BHI terrain_cut
    JSR terrain_load_rect
    TST GEN_AXIS
    BNE terrain_split_height
    LDAA GEN_CUT
    STAA ROOM_W
    LDAB ROOM
    JSR terrain_store_rect
    LDAA ROOM_X
    ADDA GEN_CUT
    INCA
    STAA ROOM_X
    LDAA GEN_BIG
    SUBA GEN_CUT
    DECA
    STAA ROOM_W
    BRA terrain_split_store
terrain_split_height:
    LDAA GEN_CUT
    STAA ROOM_H
    LDAB ROOM
    JSR terrain_store_rect
    LDAA ROOM_Y
    ADDA GEN_CUT
    INCA
    STAA ROOM_Y
    LDAA GEN_BIG
    SUBA GEN_CUT
    DECA
    STAA ROOM_H
terrain_split_store:
    LDAB GEN_N
    JSR terrain_store_rect
    INC GEN_N
    LDAA GEN_N
    CMPA GEN_TARGET
    BEQ terrain_partitions_done
    JMP terrain_partition
terrain_partitions_done:
    CLR GEN_OMITTED
    LDAA GEN_N
    STAA GEN_ROOMS
    SUBA #4
    STAA GEN_CUT
    BEQ terrain_rooms_begin
terrain_omit:
    JSR random
    ANDA #1
    BEQ terrain_omit_next
terrain_omit_roll:
    JSR terrain_room_roll
    LDX #terrain_bits
    ABX
    LDAA 0,X
    BITA GEN_OMITTED
    BNE terrain_omit_roll
    ORAA GEN_OMITTED
    STAA GEN_OMITTED
    DEC GEN_ROOMS
terrain_omit_next:
    DEC GEN_CUT
    BNE terrain_omit
terrain_rooms_begin:
    CLR ROOM
terrain_room:
    JSR terrain_load_rect
    LDAA ROOM_W
    STAA GEN_BIG
terrain_width:
    JSR random
    ANDA #31
    ADDA #4
    CMPA GEN_BIG
    BHI terrain_width
    STAA ROOM_W
terrain_x_roll:
    JSR random
    ANDA #31
    STAA GEN_CUT
    ADDA ROOM_W
    CMPA GEN_BIG
    BHI terrain_x_roll
    LDAA ROOM_X
    ADDA GEN_CUT
    STAA ROOM_X
    LDAA ROOM_H
    STAA GEN_BIG
terrain_height:
    JSR random
    ANDA #15
    ADDA #4
    CMPA GEN_BIG
    BHI terrain_height
    STAA ROOM_H
terrain_y_roll:
    JSR random
    ANDA #15
    STAA GEN_CUT
    ADDA ROOM_H
    CMPA GEN_BIG
    BHI terrain_y_roll
    LDAA ROOM_Y
    ADDA GEN_CUT
    STAA ROOM_Y
    LDAB ROOM
    JSR terrain_store_rect
    LDX #ROOM_CX
    ABX
    LDAA ROOM_W
    LSRA
    ADDA ROOM_X
    STAA 0,X
    LDX #ROOM_CY
    ABX
    LDAA ROOM_H
    LSRA
    ADDA ROOM_Y
    STAA 0,X
    LDX #GEN_LINKS
    ABX
    CLR 0,X
    LDX #terrain_bits
    ABX
    LDAA 0,X
    BITA GEN_OMITTED
    BNE terrain_room_next
    LDAA ROOM_Y
    STAA GY
terrain_row:
    JSR poll_key
    LDAA ROOM_X
    STAA GX
terrain_column:
    JSR terrain_dot
    INC GX
    LDAA GX
    SUBA ROOM_X
    CMPA ROOM_W
    BNE terrain_column
    INC GY
    LDAA GY
    SUBA ROOM_Y
    CMPA ROOM_H
    BNE terrain_row
terrain_room_next:
    INC ROOM
    LDAA ROOM
    CMPA GEN_N
    BEQ terrain_tree_begin
    JMP terrain_room
terrain_load_rect:
    LDAB ROOM
    LDX #RECT_X
    ABX
    LDAA 0,X
    STAA ROOM_X
    LDX #RECT_Y
    ABX
    LDAA 0,X
    STAA ROOM_Y
    LDX #RECT_W
    ABX
    LDAA 0,X
    STAA ROOM_W
    LDX #RECT_H
    ABX
    LDAA 0,X
    STAA ROOM_H
    RTS
terrain_store_rect:
    LDX #RECT_X
    ABX
    LDAA ROOM_X
    STAA 0,X
    LDX #RECT_Y
    ABX
    LDAA ROOM_Y
    STAA 0,X
    LDX #RECT_W
    ABX
    LDAA ROOM_W
    STAA 0,X
    LDX #RECT_H
    ABX
    LDAA ROOM_H
    STAA 0,X
    RTS
terrain_tree_begin:
    JSR terrain_room_roll
    LDX #terrain_bits
    ABX
    LDAA 0,X
    STAA GEN_VISITED
    LDAB GEN_N
    LDX #terrain_full_masks
    ABX
    LDAA 0,X
    STAA GEN_FULL
terrain_tree:
    JSR terrain_edge
    LDAA GEN_VISITED
    ANDA GEN_MASK
    BEQ terrain_tree
    CMPA GEN_MASK
    BEQ terrain_tree
    LDAA GEN_VISITED
    ORAA GEN_MASK
    STAA GEN_VISITED
    JSR terrain_connect
    LDAA GEN_VISITED
    CMPA GEN_FULL
    BNE terrain_tree
    JSR random
    ANDA #1
    BEQ terrain_start
terrain_extra:
    JSR terrain_edge
    LDAB GEN_A
    LDX #GEN_LINKS
    ABX
    LDAA 0,X
    BITA GEN_EDGE_BIT
    BNE terrain_extra
    JSR terrain_connect
terrain_start:
    JSR terrain_room_roll
    LDX #terrain_bits
    ABX
    LDAA 0,X
    BITA GEN_OMITTED
    BNE terrain_start
    STAB GEN_START
    LDX #ROOM_CX
    ABX
    LDAA 0,X
    STAA G_X
    LDX #ROOM_CY
    ABX
    LDAA 0,X
    STAA G_Y
    CLR GEN_DISTANCE
    CLR ROOM
terrain_goal:
    LDAB ROOM
    CMPB GEN_START
    BEQ terrain_goal_next
    LDX #terrain_bits
    ABX
    LDAA 0,X
    BITA GEN_OMITTED
    BNE terrain_goal_next
    LDX #RECT_X
    ABX
    LDAA 0,X
    STAA GX
    LDX #ROOM_CX
    ABX
    LDAA 0,X
    CMPA G_X
    BCS terrain_goal_left
    LDX #RECT_W
    ABX
    LDAA 0,X
    ADDA GX
    DECA
    STAA GX
terrain_goal_left:
    LDX #RECT_Y
    ABX
    LDAA 0,X
    STAA GY
    LDX #ROOM_CY
    ABX
    LDAA 0,X
    CMPA G_Y
    BCS terrain_goal_top
    LDX #RECT_H
    ABX
    LDAA 0,X
    ADDA GY
    DECA
    STAA GY
terrain_goal_top:
    LDAA GX
    SUBA G_X
    BPL terrain_goal_x
    NEGA
terrain_goal_x:
    STAA TEMP
    LDAA GY
    SUBA G_Y
    BPL terrain_goal_y
    NEGA
terrain_goal_y:
    ADDA TEMP
    CMPA GEN_DISTANCE
    BLS terrain_goal_next
    STAA GEN_DISTANCE
    LDAA GY
    STAA ROOM_Y
    LDAA GX
    STAA ROOM_X
terrain_goal_next:
    INC ROOM
    LDAA ROOM
    CMPA GEN_N
    BEQ terrain_goal_done
    JMP terrain_goal
terrain_goal_done:
    LDAA ROOM_X
    LDAB ROOM_Y
    JSR cell
    LDAA #3
    LDAB GEN_FLOOR
    INCB
    CMPB G_DEPTH
    BNE terrain_goal_store
    INCA
terrain_goal_store:
    STAA 0,X
    RTS
terrain_room_roll:
    JSR random
    ANDA #7
    CMPA GEN_N
    BCC terrain_room_roll
    TAB
    RTS
terrain_edge:
    JSR poll_key
    JSR terrain_room_roll
    STAB GEN_A
terrain_edge_other:
    JSR terrain_room_roll
    CMPB GEN_A
    BEQ terrain_edge_other
    STAB GEN_B
    LDX #terrain_bits
    ABX
    LDAA 0,X
    STAA GEN_EDGE_BIT
    STAA GEN_MASK
    LDAB GEN_A
    LDX #terrain_bits
    ABX
    LDAA 0,X
    ORAA GEN_MASK
    STAA GEN_MASK
    RTS
terrain_connect:
    LDAB GEN_A
    LDX #GEN_LINKS
    ABX
    LDAA 0,X
    ORAA GEN_EDGE_BIT
    STAA 0,X
    LDX #terrain_bits
    ABX
    LDAA 0,X
    STAA GEN_MASK
    LDAB GEN_B
    LDX #GEN_LINKS
    ABX
    LDAA 0,X
    ORAA GEN_MASK
    STAA 0,X
    LDAB GEN_A
    LDX #ROOM_CX
    ABX
    LDAA 0,X
    STAA GX
    LDX #ROOM_CY
    ABX
    LDAA 0,X
    STAA GY
    LDAB GEN_B
    LDX #ROOM_CX
    ABX
    LDAA 0,X
    STAA NX
    LDX #ROOM_CY
    ABX
    LDAA 0,X
    STAA NY
    JSR random
    ANDA #1
    BEQ terrain_horizontal_first
    JSR terrain_vertical
    JMP terrain_horizontal
terrain_horizontal_first:
    JSR terrain_horizontal
    JMP terrain_vertical
terrain_horizontal:
    JSR terrain_dot
    LDAA GX
    CMPA NX
    BEQ terrain_axis_done
    BHI terrain_left
    INC GX
    BRA terrain_horizontal
terrain_left:
    DEC GX
    BRA terrain_horizontal
terrain_vertical:
    JSR terrain_dot
    LDAA GY
    CMPA NY
    BEQ terrain_axis_done
    BHI terrain_up
    INC GY
    BRA terrain_vertical
terrain_up:
    DEC GY
    BRA terrain_vertical
terrain_axis_done:
    RTS
terrain_dot:
    LDAA GX
    LDAB GY
    JSR cell
    LDAA #1
    STAA 0,X
    RTS
terrain_bits: .byte 1,2,4,8,16,32
terrain_full_masks: .byte 0,1,3,7,15,31,63
