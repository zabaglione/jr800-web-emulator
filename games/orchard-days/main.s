; SPDX-License-Identifier: MIT
.global orchard_crops
.global orchard_growth
.global orchard_wet
.global orchard_dry
.global orchard_day
.global orchard_limit
.global orchard_goal
.global orchard_coins
.global orchard_actions
.global orchard_seed
.global orchard_rain
.global orchard_work
.global orchard_night
.section .text, code
game_start:
    JSR grid_reset
    LDX #orchard_crops
    LDAB #72
    CLRA
orchard_zero:
    STAA 0,X
    INX
    DECB
    BNE orchard_zero
    CLR orchard_day
    CLR orchard_coins
    LDAA #10
    STAA orchard_coins + 1
    LDAA #3
    STAA orchard_actions
    LDAA #1
    STAA orchard_seed
    LDAA stage
    LDAB #16
    MUL
    ADDD #orchard_levels
    XGDX
    LDAA 0,X
    STAA orchard_limit
    LDAA 1,X
    STAA orchard_goal
    INX
    INX
    STX orchard_weather
    LDAA 0,X
    STAA orchard_rain
    RTS
game_aux:
    CMPA #1
    BEQ orchard_night
    LDAA orchard_seed
    EORA #3
    STAA orchard_seed
    RTS
game_update:
    CLR resume_pending
    JSR grid_move
    CMPB #255
    BEQ orchard_space
    STAB cursor
    LDAA #1
    STAA redraw
orchard_space:
    LDAA input_event
    BITA #16
    BEQ orchard_update_done
    JMP orchard_work
orchard_update_done:
    RTS
orchard_night:
    LDAA orchard_day
    INCA
    CMPA orchard_limit
    BCS orchard_grow_begin
    LDAA #5
    STAA phase
    RTS
orchard_grow_begin:
    CLR orchard_index
orchard_plot:
    LDAB orchard_index
    LDX #orchard_crops
    ABX
    LDAA 0,X
    BEQ orchard_plot_next
    STAA orchard_crop
    TAB
    LDX #orchard_durations
    ABX
    LDAA 0,X
    STAA orchard_duration
    LDAB orchard_index
    LDX #orchard_growth
    ABX
    LDAA 0,X
    CMPA orchard_duration
    BEQ orchard_clear_wet
    LDX #orchard_wet
    ABX
    LDAA 0,X
    ORAA orchard_rain
    BEQ orchard_thirst
    LDX #orchard_growth
    ABX
    INC 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    BRA orchard_clear_wet
orchard_thirst:
    LDX #orchard_dry
    ABX
    INC 0,X
    LDAA 0,X
    CMPA #2
    BCS orchard_clear_wet
    CLR 0,X
    LDX #orchard_crops
    ABX
    CLR 0,X
    LDX #orchard_growth
    ABX
    CLR 0,X
orchard_clear_wet:
    LDX #orchard_wet
    ABX
    CLR 0,X
orchard_plot_next:
    INC orchard_index
    LDAA orchard_index
    CMPA #18
    BNE orchard_plot
    INC orchard_day
    LDAB orchard_day
    LDX orchard_weather
    ABX
    LDAA 0,X
    STAA orchard_rain
    LDAA #3
    STAA orchard_actions
    RTS
orchard_work:
    TST orchard_actions
    BNE orchard_inspect
    RTS
orchard_inspect:
    LDAB cursor
    LDX #orchard_crops
    ABX
    LDAA 0,X
    BNE orchard_water_or_pick
    JMP orchard_plant
orchard_water_or_pick:
    STAA orchard_crop
    TAB
    LDX #orchard_durations
    ABX
    LDAA 0,X
    LDAB cursor
    LDX #orchard_growth
    ABX
    CMPA 0,X
    BEQ orchard_harvest
    LDX #orchard_wet
    ABX
    TST 0,X
    BEQ orchard_water
    RTS
orchard_water:
    INC 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    JMP orchard_spent
orchard_harvest:
    LDAB orchard_crop
    LDX #orchard_prices
    ABX
    LDAB 0,X
    CLRA
    ADDD orchard_coins
    STD orchard_coins
    LDAB cursor
    LDX #orchard_crops
    ABX
    CLR 0,X
    LDX #orchard_growth
    ABX
    CLR 0,X
    LDX #orchard_wet
    ABX
    CLR 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    JSR orchard_spent
    LDAA orchard_coins
    BNE orchard_clear
    LDAA orchard_coins + 1
    CMPA orchard_goal
    BCS orchard_work_done
orchard_clear:
    LDAA #4
    STAA phase
orchard_work_done:
    RTS
orchard_plant:
    LDAB orchard_seed
    LDX #orchard_costs
    ABX
    LDAB 0,X
    CLRA
    STD orchard_amount
    LDD orchard_coins
    SUBD orchard_amount
    BCS orchard_work_done
    STD orchard_coins
    LDAB cursor
    LDX #orchard_crops
    ABX
    LDAA orchard_seed
    STAA 0,X
    LDX #orchard_growth
    ABX
    CLR 0,X
    LDX #orchard_dry
    ABX
    CLR 0,X
    LDX #orchard_wet
    ABX
    LDAA #1
    STAA 0,X
orchard_spent:
    DEC orchard_actions
    LDAA #1
    STAA redraw
    RTS
grid_value:
    STAB orchard_tile_cell
    LDX #orchard_crops
    ABX
    LDAA 0,X
    BEQ orchard_tile_done
    CMPA #2
    BEQ orchard_tile_berry
    LDX #orchard_growth
    ABX
    LDAA 0,X
    BEQ orchard_tile_bean_seed
    CMPA #3
    BEQ orchard_tile_bean_ripe
    LDAA #2
    BRA orchard_tile_water
orchard_tile_bean_seed:
    LDAA #1
    BRA orchard_tile_water
orchard_tile_bean_ripe:
    LDAA #3
    BRA orchard_tile_water
orchard_tile_berry:
    LDX #orchard_growth
    ABX
    LDAA 0,X
    CMPA #2
    BCS orchard_tile_berry_seed
    CMPA #5
    BEQ orchard_tile_berry_ripe
    LDAA #5
    BRA orchard_tile_water
orchard_tile_berry_seed:
    LDAA #4
    BRA orchard_tile_water
orchard_tile_berry_ripe:
    LDAA #6
orchard_tile_water:
    LDX #orchard_wet
    ABX
    TST 0,X
    BEQ orchard_tile_dry
    ADDA #6
    RTS
orchard_tile_dry:
    LDX #orchard_dry
    ABX
    TST 0,X
    BEQ orchard_tile_done
    ADDA #12
orchard_tile_done:
    RTS
game_render:
    JSR paint_board
    CLR orchard_hud_force
    TST resume_pending
    BEQ orchard_values
    INC orchard_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #orchard_slash_label
    LDAA #102
    CLRB
    JSR paint_text
    LDAA #108
    STAA paint_x
    CLR paint_band
    LDAA orchard_limit
    JSR paint_number
    LDX #orchard_coins_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #orchard_actions_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #orchard_seed_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #orchard_goal_label
    LDAA #138
    LDAB #7
    JSR paint_text
    LDAA #168
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDAA orchard_goal
    JSR paint_number
orchard_values:
    TST orchard_hud_force
    BNE orchard_coins_value
    LDD orchard_coins
    SUBD orchard_old_coins
    BEQ orchard_actions_check
orchard_coins_value:
    LDD orchard_coins
    STD orchard_old_coins
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDD orchard_coins
    JSR paint_number16
orchard_actions_check:
    TST orchard_hud_force
    BNE orchard_actions_value
    LDAA orchard_actions
    CMPA orchard_old_actions
    BEQ orchard_day_check
orchard_actions_value:
    LDAA orchard_actions
    STAA orchard_old_actions
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA orchard_actions
    JSR paint_number
orchard_day_check:
    TST orchard_hud_force
    BNE orchard_day_value
    LDAA orchard_day
    CMPA orchard_old_day
    BEQ orchard_seed_check
orchard_day_value:
    LDAA orchard_day
    STAA orchard_old_day
    LDAA #84
    STAA paint_x
    CLR paint_band
    LDAA orchard_day
    INCA
    JSR paint_number
    LDX #orchard_sun_label
    TST orchard_rain
    BEQ orchard_weather_text
    LDX #orchard_rain_label
orchard_weather_text:
    LDAA #132
    CLRB
    JSR paint_text
orchard_seed_check:
    TST orchard_hud_force
    BNE orchard_seed_value
    LDAA orchard_seed
    CMPA orchard_old_seed
    BEQ orchard_render_done
orchard_seed_value:
    LDAA orchard_seed
    STAA orchard_old_seed
    LDX #orchard_bean_label
    CMPA #1
    BEQ orchard_seed_text
    LDX #orchard_berry_label
orchard_seed_text:
    LDAA #138
    LDAB #6
    JMP paint_text
orchard_render_done:
    RTS
.section .bss, bss
orchard_crops: .space 18
orchard_growth: .space 18
orchard_wet: .space 18
orchard_dry: .space 18
orchard_day: .space 1
orchard_limit: .space 1
orchard_goal: .space 1
orchard_coins: .space 2
orchard_actions: .space 1
orchard_seed: .space 1
orchard_rain: .space 1
orchard_weather: .space 2
orchard_index: .space 1
orchard_crop: .space 1
orchard_duration: .space 1
orchard_amount: .space 2
orchard_tile_cell: .space 1
orchard_hud_force: .space 1
orchard_old_coins: .space 2
orchard_old_actions: .space 1
orchard_old_day: .space 1
orchard_old_seed: .space 1
.section .data, data
orchard_durations: .byte 0,3,5
orchard_costs: .byte 0,2,3
orchard_prices: .byte 0,7,12
orchard_coins_label: .byte 67,79,73,78,83,0
orchard_actions_label: .byte 65,67,84,0
orchard_seed_label: .byte 83,69,69,68,0
orchard_goal_label: .byte 71,79,65,76,0
orchard_sun_label: .byte 83,85,78,32,0
orchard_rain_label: .byte 82,65,73,78,0
orchard_bean_label: .byte 66,69,65,78,32,0
orchard_berry_label: .byte 66,69,82,82,89,0
orchard_slash_label: .byte 47,0
