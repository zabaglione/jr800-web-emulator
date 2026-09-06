; SPDX-License-Identifier: MIT
.section .text, code
menu_input:
    LDAA G_KEY
    CMPA #6
    BEQ menu_back
    LDAA G_MODE
    CMPA #6
    BEQ menu_help_close
    LDAA G_KEY
    CMPA #1
    BEQ menu_up
    CMPA #2
    BEQ menu_down
    CMPA #5
    BEQ menu_confirm
    RTS
menu_up:
    TST G_MENU
    BEQ menu_input_done
    DEC G_MENU
menu_input_done:
    RTS
menu_down:
    LDAA G_MODE
    CMPA #3
    BEQ bag_limit
    CMPA #4
    BEQ item_limit
    LDAA G_CONTEXT
    ADDA #4
    BRA menu_limit
bag_limit:
    LDAA #5
    BRA menu_limit
item_limit:
    LDAA #2
menu_limit:
    CMPA G_MENU
    BEQ menu_input_done
    INC G_MENU
    RTS
menu_back:
    LDAA G_MODE
    CMPA #4
    BNE long_u45
    JMP menu_to_bag
long_u45:
    CMPA #3
    BEQ menu_to_main
    CMPA #6
    BEQ menu_to_main
    LDAA #1
    STAA G_MODE
    RTS
menu_to_bag:
    LDAA #3
    STAA G_MODE
    LDAA G_SLOT
    STAA G_MENU
    RTS
menu_help_close:
    LDAA G_KEY
    CMPA #5
    BNE menu_input_done
menu_to_main:
    LDAA #2
    STAA G_MODE
    CLR G_MENU
    RTS
menu_confirm:
    LDAA G_MODE
    CMPA #3
    BEQ bag_confirm
    CMPA #4
    BEQ item_confirm
    LDAA G_MENU
    TST G_CONTEXT
    BEQ main_choice
    TSTA
    BNE main_has_stairs
    JMP stairs_action
main_has_stairs:
    DECA
main_choice:
    TSTA
    BEQ open_bag
    CMPA #1
    BEQ choose_wait
    CMPA #2
    BEQ choose_inspect
    CMPA #3
    BEQ choose_suspend
    LDAA #6
    STAA G_MODE
    RTS
open_bag:
    LDAA #3
    STAA G_MODE
    CLR G_MENU
    RTS
choose_wait:
    LDAA #1
    STAA G_MODE
    CLR G_MESSAGE
    JMP commit_turn
choose_inspect:
    LDAA #5
    STAA G_MODE
    LDAA G_X
    STAA G_INSPECT_X
    LDAA G_Y
    STAA G_INSPECT_Y
    RTS
choose_suspend:
    LDAA #7
    STAA G_MODE
    LDAA #1
    STAA G_SUSPEND
    RTS
bag_confirm:
    LDAA G_MENU
    STAA G_SLOT
    JSR bag_item
    TSTA
    BEQ bag_confirm_done
    LDAA #4
    STAA G_MODE
    CLR G_MENU
bag_confirm_done:
    RTS
item_confirm:
    LDAA G_MENU
    BEQ item_use
    CMPA #1
    BEQ long_u133
    JMP menu_to_bag
long_u133:
    JMP discard_item
item_use:
    JMP use_item

inspect_move:
    LDAA G_KEY
    CMPA #5
    BEQ inspect_close
    CMPA #6
    BEQ inspect_close
    CMPA #11
    BEQ inspect_done
    LDAA G_INSPECT_X
    LDAB G_KEY
    DECB
    LDX #direction_dx
    ABX
    ADDA 0,X
    CMPA #24
    BCC inspect_done
    STAA NX
    LDAA G_INSPECT_Y
    LDX #direction_dy
    ABX
    ADDA 0,X
    CMPA #16
    BCC inspect_done
    STAA NY
    TAB
    LDAA NX
    JSR visible_cell
    TST 0,X
    BEQ inspect_done
    LDAA NX
    STAA G_INSPECT_X
    LDAA NY
    STAA G_INSPECT_Y
inspect_done:
    RTS
inspect_close:
    LDAA #1
    STAA G_MODE
    RTS

render_screen:
    JSR clear
    LDAA G_MODE
    BEQ render_title
    CMPA #8
    BCS long_u179
    JMP render_end
long_u179:
    CMPA #7
    BEQ render_suspend
    CMPA #6
    BEQ render_help
    CMPA #2
    BEQ render_menu_jump
    CMPA #3
    BEQ render_bag_jump
    CMPA #4
    BEQ render_item_jump
    JSR render_hud
    JSR render_map
    LDAA G_MODE
    CMPA #5
    BNE render_message
    JMP render_inspect
render_message:
    LDAB G_MESSAGE
    ASLB
    LDX #messages
    ABX
    JSR load_string
    LDD #framebuffer + 1344
    JSR text
    RTS
render_menu_jump:
    JMP render_menu
render_bag_jump:
    JMP render_bag
render_item_jump:
    JMP render_item
render_title:
    LDX #s_title
    LDD #framebuffer + 192 + 60
    JSR text
    LDX #s_goal
    LDD #framebuffer + 384 + 18
    JSR text
    LDAA G_DIFFICULTY
    STAA G_MENU
    LDX #difficulty_names
    STX UI_PTR
    LDAA #3
    STAA COUNT
    LDAA #3
    STAA UI_ROW
    JSR render_choices
    LDX #s_keys
    LDD #framebuffer + 1344
    JMP text
render_suspend:
    LDX #s_suspended
    LDD #framebuffer + 384 + 60
    JSR text
    LDX #s_resume
    LDD #framebuffer + 768 + 30
    JMP text
render_help:
    LDX #help_lines
    STX UI_PTR
    LDAA #6
    STAA COUNT
    LDAA #1
    STAA UI_ROW
    JMP render_lines
render_end:
    LDX #s_dead
    LDAA G_MODE
    CMPA #8
    BEQ render_end_title
    LDX #s_won
render_end_title:
    LDD #framebuffer + 384 + 42
    JSR text
    LDX #s_end
    LDD #framebuffer + 768 + 36
    JSR text
    LDX #s_gold
    LDD #framebuffer + 1152 + 60
    JSR text
    LDX #framebuffer + 1152 + 90
    JSR render_gold
    JMP render_hud

render_hud:
    LDX #s_floor
    LDD #framebuffer
    JSR text
    LDAA G_FLOOR
    INCA
    LDX #framebuffer + 6
    JSR number
    LDX #s_hp
    LDD #framebuffer + 24
    JSR text
    LDAA G_HP
    LDX #framebuffer + 42
    JSR number
    LDAA #45
    LDX #framebuffer + 60
    JSR glyph
    LDAA G_MAX_HP
    LDX #framebuffer + 66
    JSR number
    LDX #s_food
    LDD #framebuffer + 96
    JSR text
    LDAA G_FOOD
    LDX #framebuffer + 114
    JSR number
    LDX #s_level
    LDD #framebuffer + 150
    JSR text
    LDAA G_LEVEL
    LDX #framebuffer + 162
    JSR number
    TST G_POISON
    BEQ hud_done
    LDAA #80
    LDX #framebuffer + 186
    JSR glyph
hud_done:
    RTS

; A byte printed in three columns. Fixed width avoids shifting HUD fields.
number:
    STAA NUM
    STX NUM_DEST
    LDAA #48
    STAA DIGIT
number_hundreds:
    LDAA NUM
    CMPA #100
    BCS number_hundreds_done
    SUBA #100
    STAA NUM
    INC DIGIT
    BRA number_hundreds
number_hundreds_done:
    LDAA DIGIT
    JSR glyph
    LDX NUM_DEST
    LDAB #6
    ABX
    STX NUM_DEST
    LDAA #48
    STAA DIGIT
number_tens:
    LDAA NUM
    CMPA #10
    BCS number_tens_done
    SUBA #10
    STAA NUM
    INC DIGIT
    BRA number_tens
number_tens_done:
    LDAA DIGIT
    JSR glyph
    LDX NUM_DEST
    LDAB #6
    ABX
    LDAA NUM
    ADDA #48
    JMP glyph

render_map:
    LDAA G_Y
    LDAB G_MODE
    CMPB #5
    BNE view_player
    LDAA G_INSPECT_Y
view_player:
    SUBA #2
    BCC view_top_valid
    CLRA
view_top_valid:
    CMPA #10
    BLS view_bottom_valid
    LDAA #10
view_bottom_valid:
    STAA VIEW_Y
    STAA ROW
    LDX #framebuffer + 192
    STX DRAW
map_row:
    JSR poll_key
    CLR COL
map_column:
    LDAA COL
    LDAB ROW
    JSR cell
    TSTA
    BPL tile_unknown
    ANDA #127
    INCA
    BRA tile_ready
tile_unknown:
    CLRA
tile_ready:
    JSR draw_tile
    INC COL
    LDAA COL
    CMPA #24
    BNE map_column
    INC ROW
    LDAA ROW
    SUBA VIEW_Y
    CMPA #6
    BNE map_row
    LDD FP
    ADDD #ITEM_START
    STD ITEM_PTR
map_items:
    LDX ITEM_PTR
    TST 2,X
    BEQ map_item_next
    LDAA 0,X
    STAA NX
    LDAA 1,X
    STAA NY
    LDAA 2,X
    CMPA #7
    BCS item_consumable_tile
    CMPA #10
    BCS item_weapon_tile
    CMPA #13
    BCS item_armor_tile
    LDAA #12
    BRA map_item_tile
item_weapon_tile:
    LDAA #10
    BRA map_item_tile
item_armor_tile:
    LDAA #11
    BRA map_item_tile
item_consumable_tile:
    CMPA #1
    BEQ item_food_tile
    CMPA #5
    BCS item_potion_tile
    LDAA #9
    BRA map_item_tile
item_food_tile:
    LDAA #7
    BRA map_item_tile
item_potion_tile:
    LDAA #8
map_item_tile:
    STAA TILE
    JSR overlay_visible
map_item_next:
    LDD ITEM_PTR
    ADDD #3
    STD ITEM_PTR
    SUBD FP
    SUBD #FLOOR_DATA_END
    BNE map_items
    CLR E_INDEX
map_enemies:
    JSR enemy_pointer
    TST 3,X
    BEQ map_enemy_next
    LDAA 0,X
    STAA NX
    LDAA 1,X
    STAA NY
    LDAA 2,X
    ADDA #12
    STAA TILE
    JSR overlay_visible
map_enemy_next:
    INC E_INDEX
    LDAA E_INDEX
    CMPA #MAX_ENEMIES
    BNE map_enemies
    LDAA G_X
    STAA NX
    LDAA G_Y
    STAA NY
    JSR tile_destination
    LDAA #6
    JSR draw_tile
    RTS

overlay_visible:
    LDAA NY
    SUBA VIEW_Y
    CMPA #6
    BCC overlay_done
    LDAA NX
    LDAB NY
    JSR visible_cell
    TST 0,X
    BEQ overlay_done
    JSR tile_destination
    LDAA TILE
    JSR draw_tile
overlay_done:
    RTS

tile_destination:
    LDAA NY
    SUBA VIEW_Y
    INCA
    LDAB #192
    MUL
    ADDD #framebuffer
    STD DRAW
    LDAA NX
    LDAB #8
    MUL
    ADDD DRAW
    STD DRAW
    RTS

; Software PCG: eight original vertical column bytes copied per 8x8 cell.
draw_tile:
    LDAB #8
    MUL
    ADDD #tiles
    STD P1
    LDAB #8
pcg_column:
    LDX P1
    LDAA 0,X
    INX
    STX P1
    LDX DRAW
    STAA 0,X
    INX
    STX DRAW
    DECB
    BNE pcg_column
    RTS

; X -> pointer table entry; return X=string, with no extended LDX instruction.
load_string:
    LDD 0,X
    STD P2
    LDX P2
    RTS

render_menu:
    JSR render_stats
    LDAA G_X
    LDAB G_Y
    JSR cell
    ANDA #127
    CLR G_CONTEXT
    CMPA #3
    BNE menu_plain
menu_context:
    INC G_CONTEXT
    LDX #main_names
    LDAA #6
    BRA menu_draw
menu_plain:
    LDX #main_names + 2
    LDAA #5
menu_draw:
    STX UI_PTR
    STAA COUNT
    LDAA #1
    STAA UI_ROW
    JSR render_choices
    LDX #s_menu_keys
    LDD #framebuffer + 1344
    JMP text

render_bag:
    JSR render_stats
    LDAA #1
    STAA UI_ROW
    CLR G_SLOT
bag_draw_loop:
    JSR bag_item
    JSR item_name
    LDAA UI_ROW
    LDAB #192
    MUL
    ADDD #framebuffer + 18
    JSR text
    LDAA G_SLOT
    CMPA G_MENU
    BNE bag_draw_next
    JSR draw_selection
bag_draw_next:
    INC UI_ROW
    INC G_SLOT
    LDAA G_SLOT
    CMPA #6
    BNE bag_draw_loop
    LDX #s_menu_keys
    LDD #framebuffer + 1344
    JMP text

item_name:
    CMPA #2
    BCS item_name_raw
    CMPA #7
    BCC item_name_raw
    STAA ITEM_TYPE
    SUBA #2
    TAB
    LDX #bits
    ABX
    LDAA G_KNOWN
    BITA 0,X
    BEQ item_name_unknown
    LDX #G_IDENTITIES
    ABX
    LDAB 0,X
    ASLB
    LDX #effect_names
    ABX
    JMP load_string
item_name_unknown:
    LDAA ITEM_TYPE
item_name_raw:
    TAB
    ASLB
    LDX #item_names
    ABX
    JMP load_string

render_item:
    JSR bag_item
    JSR item_name
    LDD #framebuffer + 192
    JSR text
    LDAA ITEM_TYPE
    CMPA #7
    BCS item_no_comparison
    CMPA #10
    BCC armor_comparison
    LDX #s_atk_swap
    LDD #framebuffer + 384
    JSR text
    LDAA G_ATTACK
    LDX #framebuffer + 384 + 54
    JSR number
    LDAA ITEM_TYPE
    SUBA #3
    BRA comparison_value
armor_comparison:
    LDX #s_def_swap
    LDD #framebuffer + 384
    JSR text
    LDAA G_DEFENSE
    LDX #framebuffer + 384 + 54
    JSR number
    LDAA ITEM_TYPE
    SUBA #9
comparison_value:
    LDX #framebuffer + 384 + 114
    JSR number
item_no_comparison:
    LDX #item_actions
    STX UI_PTR
    LDAA #3
    STAA COUNT
    LDAA #4
    STAA UI_ROW
    JSR render_choices
    JMP render_message

render_choices:
    CLR CANDIDATE
choices_next:
    LDAA CANDIDATE
    CMPA G_MENU
    BNE choices_unselected
    JSR draw_selection
choices_unselected:
    JSR render_line
    INC CANDIDATE
    DEC COUNT
    BNE choices_next
    RTS
render_lines:
    JSR render_line
    DEC COUNT
    BNE render_lines
    RTS
render_line:
    LDX UI_PTR
    JSR load_string
    LDAA UI_ROW
    LDAB #192
    MUL
    ADDD #framebuffer + 18
    JSR text
    LDX UI_PTR
    INX
    INX
    STX UI_PTR
    INC UI_ROW
    RTS
draw_selection:
    LDAA UI_ROW
    LDAB #192
    MUL
    ADDD #framebuffer + 3
    STD P2
    LDX P2
    LDAA #$3E
    STAA 0,X
    STAA 1,X
    RTS

render_inspect:
    LDAA G_INSPECT_X
    STAA NX
    LDAA G_INSPECT_Y
    STAA NY
    ; Invert the chosen tile to show a cursor without erasing its icon.
    LDAA NY
    SUBA VIEW_Y
    CMPA #6
    BCC inspect_name
    JSR tile_destination
    LDX DRAW
    LDAB #8
inspect_invert:
    COM 0,X
    INX
    DECB
    BNE inspect_invert
inspect_name:
    JSR find_enemy
    TSTA
    BEQ inspect_item
    DECA
    ASLA
    STAA CANDIDATE
    TAB
    LDX #enemy_names
    ABX
    JSR load_string
    LDD #framebuffer + 1344
    JSR text
    LDX E_PTR
    LDAA 3,X
    LDX #framebuffer + 1344 + 96
    JSR number
    LDX #framebuffer
    CLRA
inspect_clear_hud:
    STAA 0,X
    INX
    CPX #framebuffer + 192
    BNE inspect_clear_hud
    LDAB CANDIDATE
    LDX #enemy_traits
    ABX
    JSR load_string
    LDD #framebuffer
    JMP text
inspect_item:
    JSR find_item
    TSTA
    BEQ inspect_terrain
    JSR item_name
    BRA inspect_text
inspect_terrain:
    LDAA NX
    LDAB NY
    JSR cell
    ANDA #127
    ASLA
    TAB
    LDX #terrain_names
    ABX
    JSR load_string
inspect_text:
    LDD #framebuffer + 1344
    JMP text

render_stats:
    LDX #s_stats
    LDD #framebuffer
    JSR text
    LDAA G_ATTACK
    LDX #framebuffer + 24
    JSR number
    LDAA G_DEFENSE
    LDX #framebuffer + 72
    JSR number
    LDX #framebuffer + 138
render_gold:
    STX NUM_DEST
    LDD G_GOLD
    STD $AC
    LDD #10000
    JSR gold_digit
    LDD #1000
    JSR gold_digit
    LDAA #48
    STAA DIGIT
gold_hundreds:
    LDD $AC
    SUBD #100
    BCS gold_remainder
    STD $AC
    INC DIGIT
    BRA gold_hundreds
gold_remainder:
    ADDD #100
    STAB NUM
    JMP number_hundreds_done

; Print one high decimal digit; retain the remainder for the next place.
gold_digit:
    STD $AA
    LDAA #48
    STAA DIGIT
gold_digit_loop:
    LDD $AC
    SUBD $AA
    BCS gold_digit_done
    STD $AC
    INC DIGIT
    BRA gold_digit_loop
gold_digit_done:
    LDX NUM_DEST
    LDAA DIGIT
    JSR glyph
    LDX NUM_DEST
    LDAB #6
    ABX
    STX NUM_DEST
    RTS
