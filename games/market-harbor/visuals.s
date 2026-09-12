; SPDX-License-Identifier: MIT
; market-harbor: harbor-ledger, play origin 0
.equ VIEW_X,0
.equ HUD_FIELDS,44
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB market_port
    CLRA
    LDX #hud_field_0
    JSR hud_choice
    LDAB market_day
    CLRA
    ADDD #1
    LDX #hud_field_1
    JSR hud_number
    LDAB market_limit
    CLRA
    LDX #hud_field_2
    JSR hud_number
    LDAB stage
    CLRA
    ADDD #1
    LDX #hud_field_3
    JSR hud_number
    TST selection_active
    BNE hud_skip_4
    LDD market_cash
    LDX #hud_field_4
    JSR hud_number
hud_skip_4:
    TST selection_active
    BNE hud_skip_5
    LDD market_goal
    LDX #hud_field_5
    JSR hud_number
hud_skip_5:
    TST selection_active
    BNE hud_skip_6
    LDAB market_mode
    CLRA
    LDX #hud_field_6
    JSR hud_choice
hud_skip_6:
    TST selection_active
    BNE hud_skip_7
    LDAB market_good
    CLRA
    CMPB #0
    BEQ hud_expr_7_yes
    LDD #0
    BRA hud_expr_7_done
hud_expr_7_yes:
    LDD #1
hud_expr_7_done:
    LDX #hud_field_7
    JSR hud_choice
hud_skip_7:
    TST selection_active
    BNE hud_skip_8
    LDAB market_prices + 0
    CLRA
    LDX #hud_field_8
    JSR hud_number
hud_skip_8:
    TST selection_active
    BNE hud_skip_9
    LDAB market_cargo + 0
    CLRA
    LDX #hud_field_9
    JSR hud_number
hud_skip_9:
    TST selection_active
    BNE hud_skip_10
    LDAB market_good
    CLRA
    CMPB #0
    BEQ hud_expr_10_yes
    LDD #0
    BRA hud_expr_10_done
hud_expr_10_yes:
    LDD #1
hud_expr_10_done:
    LDX #hud_field_10
    JSR hud_choice
hud_skip_10:
    TST selection_active
    BNE hud_skip_11
    LDAB market_good
    CLRA
    CMPB #1
    BEQ hud_expr_11_yes
    LDD #0
    BRA hud_expr_11_done
hud_expr_11_yes:
    LDD #1
hud_expr_11_done:
    LDX #hud_field_11
    JSR hud_choice
hud_skip_11:
    TST selection_active
    BNE hud_skip_12
    LDAB market_prices + 1
    CLRA
    LDX #hud_field_12
    JSR hud_number
hud_skip_12:
    TST selection_active
    BNE hud_skip_13
    LDAB market_cargo + 1
    CLRA
    LDX #hud_field_13
    JSR hud_number
hud_skip_13:
    TST selection_active
    BNE hud_skip_14
    LDAB market_good
    CLRA
    CMPB #1
    BEQ hud_expr_14_yes
    LDD #0
    BRA hud_expr_14_done
hud_expr_14_yes:
    LDD #1
hud_expr_14_done:
    LDX #hud_field_14
    JSR hud_choice
hud_skip_14:
    TST selection_active
    BNE hud_skip_15
    LDAB market_good
    CLRA
    CMPB #2
    BEQ hud_expr_15_yes
    LDD #0
    BRA hud_expr_15_done
hud_expr_15_yes:
    LDD #1
hud_expr_15_done:
    LDX #hud_field_15
    JSR hud_choice
hud_skip_15:
    TST selection_active
    BNE hud_skip_16
    LDAB market_prices + 2
    CLRA
    LDX #hud_field_16
    JSR hud_number
hud_skip_16:
    TST selection_active
    BNE hud_skip_17
    LDAB market_cargo + 2
    CLRA
    LDX #hud_field_17
    JSR hud_number
hud_skip_17:
    TST selection_active
    BNE hud_skip_18
    LDAB market_good
    CLRA
    CMPB #2
    BEQ hud_expr_18_yes
    LDD #0
    BRA hud_expr_18_done
hud_expr_18_yes:
    LDD #1
hud_expr_18_done:
    LDX #hud_field_18
    JSR hud_choice
hud_skip_18:
    TST selection_active
    BNE hud_skip_19
    LDAB market_load
    CLRA
    LDX #hud_field_19
    JSR hud_number
hud_skip_19:
    TST selection_active
    BNE hud_skip_20
    LDAB market_capacity
    CLRA
    LDX #hud_field_20
    JSR hud_number
hud_skip_20:
    TST selection_active
    BNE hud_skip_21
    LDAB market_good
    CLRA
    CMPB #3
    BEQ hud_expr_21_yes
    LDD #0
    BRA hud_expr_21_done
hud_expr_21_yes:
    LDD #1
hud_expr_21_done:
    LDX #hud_field_21
    JSR hud_choice
hud_skip_21:
    TST selection_active
    BNE hud_skip_22
    LDAB market_good
    CLRA
    CMPB #3
    BEQ hud_expr_22_yes
    LDD #0
    BRA hud_expr_22_done
hud_expr_22_yes:
    LDD #1
hud_expr_22_done:
    LDX #hud_field_22
    JSR hud_choice
hud_skip_22:
    TST selection_active
    BNE hud_skip_23
    LDAB market_reason
    CLRA
    LDX #hud_field_23
    JSR hud_choice
hud_skip_23:
    TST selection_active
    BEQ hud_skip_24
    LDAB market_trip_days
    CLRA
    LDX #hud_field_24
    JSR hud_number
hud_skip_24:
    TST selection_active
    BEQ hud_skip_25
    LDAB market_trip_days
    CLRA
    ASLB
    LDX #hud_field_25
    JSR hud_number
hud_skip_25:
    TST selection_active
    BEQ hud_skip_26
    LDD market_cash
    LDX #hud_field_26
    JSR hud_number
hud_skip_26:
    TST selection_active
    BEQ hud_skip_27
    LDAB market_destination
    CLRA
    CMPB #0
    BEQ hud_expr_27_yes
    LDD #0
    BRA hud_expr_27_done
hud_expr_27_yes:
    LDD #1
hud_expr_27_done:
    LDX #hud_field_27
    JSR hud_choice
hud_skip_27:
    TST selection_active
    BEQ hud_skip_28
    LDAB market_quotes + 0
    CLRA
    LDX #hud_field_28
    JSR hud_number
hud_skip_28:
    TST selection_active
    BEQ hud_skip_29
    LDAB market_quotes + 1
    CLRA
    LDX #hud_field_29
    JSR hud_number
hud_skip_29:
    TST selection_active
    BEQ hud_skip_30
    LDAB market_quotes + 2
    CLRA
    LDX #hud_field_30
    JSR hud_number
hud_skip_30:
    TST selection_active
    BEQ hud_skip_31
    LDAB market_destination
    CLRA
    CMPB #1
    BEQ hud_expr_31_yes
    LDD #0
    BRA hud_expr_31_done
hud_expr_31_yes:
    LDD #1
hud_expr_31_done:
    LDX #hud_field_31
    JSR hud_choice
hud_skip_31:
    TST selection_active
    BEQ hud_skip_32
    LDAB market_quotes + 3
    CLRA
    LDX #hud_field_32
    JSR hud_number
hud_skip_32:
    TST selection_active
    BEQ hud_skip_33
    LDAB market_quotes + 4
    CLRA
    LDX #hud_field_33
    JSR hud_number
hud_skip_33:
    TST selection_active
    BEQ hud_skip_34
    LDAB market_quotes + 5
    CLRA
    LDX #hud_field_34
    JSR hud_number
hud_skip_34:
    TST selection_active
    BEQ hud_skip_35
    LDAB market_destination
    CLRA
    CMPB #2
    BEQ hud_expr_35_yes
    LDD #0
    BRA hud_expr_35_done
hud_expr_35_yes:
    LDD #1
hud_expr_35_done:
    LDX #hud_field_35
    JSR hud_choice
hud_skip_35:
    TST selection_active
    BEQ hud_skip_36
    LDAB market_quotes + 6
    CLRA
    LDX #hud_field_36
    JSR hud_number
hud_skip_36:
    TST selection_active
    BEQ hud_skip_37
    LDAB market_quotes + 7
    CLRA
    LDX #hud_field_37
    JSR hud_number
hud_skip_37:
    TST selection_active
    BEQ hud_skip_38
    LDAB market_quotes + 8
    CLRA
    LDX #hud_field_38
    JSR hud_number
hud_skip_38:
    TST selection_active
    BEQ hud_skip_39
    LDAB market_destination
    CLRA
    CMPB #3
    BEQ hud_expr_39_yes
    LDD #0
    BRA hud_expr_39_done
hud_expr_39_yes:
    LDD #1
hud_expr_39_done:
    LDX #hud_field_39
    JSR hud_choice
hud_skip_39:
    TST selection_active
    BEQ hud_skip_40
    LDAB market_quotes + 9
    CLRA
    LDX #hud_field_40
    JSR hud_number
hud_skip_40:
    TST selection_active
    BEQ hud_skip_41
    LDAB market_quotes + 10
    CLRA
    LDX #hud_field_41
    JSR hud_number
hud_skip_41:
    TST selection_active
    BEQ hud_skip_42
    LDAB market_quotes + 11
    CLRA
    LDX #hud_field_42
    JSR hud_number
hud_skip_42:
    TST selection_active
    BEQ hud_skip_43
    CLRB
    TST market_message
    BEQ hud_expr_43_done
    INCB
hud_expr_43_done:
    CLRA
    LDX #hud_field_43
    JSR hud_choice
hud_skip_43:
    RTS
hud_select_background:
    LDX #hud_span_table
    TST selection_active
    BEQ hud_background_selected
    LDX #hud_span_alternate
hud_background_selected:
    RTS
.section .data, data
view_origin: .byte 0
hud_field_0:
    .byte 48,0,5,5,128,4
    .word hud_cache + 0,hud_choices_0
hud_choices_0:
    .byte $4E,$4F,$52,$54,$48,$45,$41,$53,$54,$20,$53,$4F,$55,$54,$48,$57,$45,$53,$54,$20

hud_field_1:
    .byte 115,0,2,0,128,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 139,0,2,0,128,0
    .word hud_cache + 6,0
hud_field_3:
    .byte 179,0,2,0,128,0
    .word hud_cache + 9,0
hud_field_4:
    .byte 23,1,5,1,0,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 107,1,5,1,0,0
    .word hud_cache + 15,0
hud_field_6:
    .byte 158,1,4,5,0,2
    .word hud_cache + 18,hud_choices_6
hud_choices_6:
    .byte $42,$55,$59,$20,$53,$45,$4C,$4C

hud_field_7:
    .byte 4,3,1,0,0,2
    .word hud_cache + 21,hud_choices_7
hud_choices_7:
    .byte $20,$3E

hud_field_8:
    .byte 72,3,3,1,0,0
    .word hud_cache + 24,0
hud_field_9:
    .byte 114,3,3,1,0,0
    .word hud_cache + 27,0
hud_field_10:
    .byte 147,3,9,0,0,2
    .word hud_cache + 30,hud_choices_10
hud_choices_10:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$53,$50,$41,$43,$45,$20,$20,$20,$20

hud_field_11:
    .byte 4,4,1,0,0,2
    .word hud_cache + 33,hud_choices_11
hud_choices_11:
    .byte $20,$3E

hud_field_12:
    .byte 72,4,3,1,0,0
    .word hud_cache + 36,0
hud_field_13:
    .byte 114,4,3,1,0,0
    .word hud_cache + 39,0
hud_field_14:
    .byte 147,4,9,0,0,2
    .word hud_cache + 42,hud_choices_14
hud_choices_14:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$53,$50,$41,$43,$45,$20,$20,$20,$20

hud_field_15:
    .byte 4,5,1,0,0,2
    .word hud_cache + 45,hud_choices_15
hud_choices_15:
    .byte $20,$3E

hud_field_16:
    .byte 72,5,3,1,0,0
    .word hud_cache + 48,0
hud_field_17:
    .byte 114,5,3,1,0,0
    .word hud_cache + 51,0
hud_field_18:
    .byte 147,5,9,0,0,2
    .word hud_cache + 54,hud_choices_18
hud_choices_18:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$53,$50,$41,$43,$45,$20,$20,$20,$20

hud_field_19:
    .byte 56,6,2,0,0,0
    .word hud_cache + 57,0
hud_field_20:
    .byte 82,6,2,0,0,0
    .word hud_cache + 60,0
hud_field_21:
    .byte 110,6,1,0,0,2
    .word hud_cache + 63,hud_choices_21
hud_choices_21:
    .byte $20,$3E

hud_field_22:
    .byte 147,6,9,0,0,2
    .word hud_cache + 66,hud_choices_22
hud_choices_22:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$53,$50,$41,$43,$45,$20,$20,$20,$20

hud_field_23:
    .byte 4,7,46,0,128,3
    .word hud_cache + 69,hud_choices_23
hud_choices_23:
    .byte $41,$2F,$44,$3A,$20,$42,$55,$59,$2F,$53,$45,$4C,$4C,$20,$20,$53,$50,$41,$43,$45,$3A,$20,$54,$52
    .byte $41,$44,$45,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$54,$49
    .byte $4D,$45,$20,$4C,$49,$4D,$49,$54,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$4F,$55,$54,$20
    .byte $4F,$46,$20,$43,$41,$53,$48,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20

hud_field_24:
    .byte 27,1,2,1,0,0
    .word hud_cache + 72,0
hud_field_25:
    .byte 85,1,2,1,0,0
    .word hud_cache + 75,0
hud_field_26:
    .byte 145,1,5,0,0,0
    .word hud_cache + 78,0
hud_field_27:
    .byte 4,3,1,0,0,2
    .word hud_cache + 81,hud_choices_27
hud_choices_27:
    .byte $20,$3E

hud_field_28:
    .byte 72,3,3,1,0,0
    .word hud_cache + 84,0
hud_field_29:
    .byte 114,3,3,1,0,0
    .word hud_cache + 87,0
hud_field_30:
    .byte 156,3,3,1,0,0
    .word hud_cache + 90,0
hud_field_31:
    .byte 4,4,1,0,0,2
    .word hud_cache + 93,hud_choices_31
hud_choices_31:
    .byte $20,$3E

hud_field_32:
    .byte 72,4,3,1,0,0
    .word hud_cache + 96,0
hud_field_33:
    .byte 114,4,3,1,0,0
    .word hud_cache + 99,0
hud_field_34:
    .byte 156,4,3,1,0,0
    .word hud_cache + 102,0
hud_field_35:
    .byte 4,5,1,0,0,2
    .word hud_cache + 105,hud_choices_35
hud_choices_35:
    .byte $20,$3E

hud_field_36:
    .byte 72,5,3,1,0,0
    .word hud_cache + 108,0
hud_field_37:
    .byte 114,5,3,1,0,0
    .word hud_cache + 111,0
hud_field_38:
    .byte 156,5,3,1,0,0
    .word hud_cache + 114,0
hud_field_39:
    .byte 4,6,1,0,0,2
    .word hud_cache + 117,hud_choices_39
hud_choices_39:
    .byte $20,$3E

hud_field_40:
    .byte 72,6,3,1,0,0
    .word hud_cache + 120,0
hud_field_41:
    .byte 114,6,3,1,0,0
    .word hud_cache + 123,0
hud_field_42:
    .byte 156,6,3,1,0,0
    .word hud_cache + 126,0
hud_field_43:
    .byte 4,7,46,0,128,2
    .word hud_cache + 129,hud_choices_43
hud_choices_43:
    .byte $53,$50,$41,$43,$45,$3A,$20,$53,$41,$49,$4C,$20,$20,$20,$52,$45,$54,$55,$52,$4E,$3A,$20,$42,$41
    .byte $43,$4B,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$4E,$4F
    .byte $54,$20,$45,$4E,$4F,$55,$47,$48,$20,$43,$41,$53,$48,$20,$46,$4F,$52,$20,$46,$41,$52,$45,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20

hud_span_table:
    .word hud_pixels_0
    .byte 0,0,64
    .word hud_pixels_1
    .byte 0,64,64
    .word hud_pixels_2
    .byte 0,128,64
    .word hud_pixels_3
    .byte 1,0,64
    .word hud_pixels_4
    .byte 1,64,64
    .word hud_pixels_5
    .byte 1,128,64
    .word hud_pixels_6
    .byte 2,0,64
    .word hud_pixels_7
    .byte 2,64,64
    .word hud_pixels_8
    .byte 2,128,64
    .word hud_pixels_9
    .byte 3,0,64
    .word hud_pixels_10
    .byte 3,64,64
    .word hud_pixels_11
    .byte 3,128,64
    .word hud_pixels_12
    .byte 4,0,64
    .word hud_pixels_10
    .byte 4,64,64
    .word hud_pixels_11
    .byte 4,128,64
    .word hud_pixels_13
    .byte 5,0,64
    .word hud_pixels_10
    .byte 5,64,64
    .word hud_pixels_14
    .byte 5,128,64
    .word hud_pixels_15
    .byte 6,0,64
    .word hud_pixels_16
    .byte 6,64,64
    .word hud_pixels_17
    .byte 6,128,64
    .word hud_pixels_18
    .byte 7,0,64
    .word hud_pixels_18
    .byte 7,64,64
    .word hud_pixels_18
    .byte 7,128,64
    .word 0
    .byte 0,0,0
hud_span_alternate:
    .word hud_pixels_19
    .byte 0,0,64
    .word hud_pixels_1
    .byte 0,64,64
    .word hud_pixels_2
    .byte 0,128,64
    .word hud_pixels_20
    .byte 1,0,64
    .word hud_pixels_21
    .byte 1,64,64
    .word hud_pixels_22
    .byte 1,128,64
    .word hud_pixels_23
    .byte 2,0,64
    .word hud_pixels_24
    .byte 2,64,64
    .word hud_pixels_25
    .byte 2,128,64
    .word hud_pixels_26
    .byte 3,0,64
    .word hud_pixels_10
    .byte 3,64,64
    .word hud_pixels_11
    .byte 3,128,64
    .word hud_pixels_27
    .byte 4,0,64
    .word hud_pixels_10
    .byte 4,64,64
    .word hud_pixels_11
    .byte 4,128,64
    .word hud_pixels_28
    .byte 5,0,64
    .word hud_pixels_10
    .byte 5,64,64
    .word hud_pixels_11
    .byte 5,128,64
    .word hud_pixels_29
    .byte 6,0,64
    .word hud_pixels_10
    .byte 6,64,64
    .word hud_pixels_11
    .byte 6,128,64
    .word hud_pixels_18
    .byte 7,0,64
    .word hud_pixels_18
    .byte 7,64,64
    .word hud_pixels_18
    .byte 7,128,64
    .word 0
    .byte 0,0,0
hud_pixels_0:
    .byte $FF,$FF,$FF,$C1,$F7,$C1,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$CB,$FF,$C1,$D5,$EB,$FF,$E3,$DD,$E3,$FF,$C1
    .byte $F5,$CB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_1:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1,$DD,$E3,$FF,$C3,$F5,$C3,$FF,$F9,$C7,$F9,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_2:
    .byte $FF,$FF,$FF,$FF,$CF,$F7,$F9,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$EF,$DF,$E1,$FF,$E3,$DD,$E3,$FF,$C1,$D5,$EB,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_3:
    .byte $00,$00,$00,$1C,$22,$22,$00,$3C,$0A,$3C,$00,$24,$2A,$12,$00,$3E,$08,$3E,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_4:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$1C,$22,$3A,$00,$1C,$22,$1C
    .byte $00,$3C,$0A,$3C,$00,$3E,$20,$20,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_5:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_6:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$E3,$DD,$C5,$FF,$E3,$DD,$E3,$FF,$E3,$DD,$E3,$FF,$C1
    .byte $DD,$E3,$FF,$DB,$D5,$ED,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_7:
    .byte $FF,$FF,$FF,$FF,$FF,$C1,$F5,$FB,$FF,$C1,$F5,$CB,$FF,$DD,$C1,$DD,$FF,$E3,$DD,$DD,$FF,$C1,$D5,$DD
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1
    .byte $F7,$C1,$FF,$E3,$DD,$E3,$FF,$C1,$DF,$DF,$FF,$C1,$DD,$E3,$FF,$FF

hud_pixels_8:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C3
    .byte $F5,$C3,$FF,$E3,$DD,$DD,$FF,$FD,$C1,$FD,$FF,$DD,$C1,$DD,$FF,$E3,$DD,$E3,$FF,$C1,$E3,$C1,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_9:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$3E,$0A,$34,$00,$22,$3E,$22,$00,$1C,$22,$22,$00
    .byte $3E,$2A,$22,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_10:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_11:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00

hud_pixels_12:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$1C,$22,$1C,$00,$3E,$0A,$34,$00,$3E,$2A,$22,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_13:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$24,$2A,$12,$00,$3E,$0A,$04,$00,$22,$3E,$22,$00
    .byte $1C,$22,$22,$00,$3E,$2A,$22,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_14:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$7F,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00

hud_pixels_15:
    .byte $00,$FF,$00,$00,$3E,$20,$20,$00,$1C,$22,$1C,$00,$3C,$0A,$3C,$00,$3E,$22,$1C,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_16:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$30,$08,$06,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$24,$2A,$12,$00,$3C,$0A,$3C,$00,$22,$3E,$22

hud_pixels_17:
    .byte $00,$3E,$20,$20,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00

hud_pixels_18:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_19:
    .byte $FF,$FF,$FF,$DB,$D5,$ED,$FF,$C3,$F5,$C3,$FF,$DD,$C1,$DD,$FF,$C1,$DF,$DF,$FF,$FF,$FF,$FF,$FF,$C1
    .byte $F5,$FD,$FF,$C1,$F5,$CB,$FF,$E3,$DD,$E3,$FF,$C1,$F3,$C1,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_20:
    .byte $00,$00,$00,$3E,$22,$1C,$00,$3C,$0A,$3C,$00,$06,$38,$06,$00,$24,$2A,$12,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$3E,$0A,$02

hud_pixels_21:
    .byte $00,$3C,$0A,$3C,$00,$3E,$0A,$34,$00,$3E,$2A,$22,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$1C,$22,$22,$00,$3C,$0A,$3C

hud_pixels_22:
    .byte $00,$24,$2A,$12,$00,$3E,$08,$3E,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_23:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1,$F5,$FB,$FF,$E3,$DD,$E3,$FF,$C1,$F5,$CB,$FF,$FD
    .byte $C1,$FD,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_24:
    .byte $FF,$FF,$FF,$FF,$FF,$C1,$F5,$CB,$FF,$DD,$C1,$DD,$FF,$E3,$DD,$DD,$FF,$C1,$D5,$DD,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$E3
    .byte $DD,$E3,$FF,$C1,$F5,$CB,$FF,$C1,$D5,$DD,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_25:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$DB
    .byte $D5,$ED,$FF,$C1,$F5,$FB,$FF,$DD,$C1,$DD,$FF,$E3,$DD,$DD,$FF,$C1,$D5,$DD,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_26:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$3E,$1C,$3E,$00,$1C,$22,$1C,$00,$3E,$0A,$34,$00
    .byte $02,$3E,$02,$00,$3E,$08,$3E,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_27:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$3E,$2A,$22,$00,$3C,$0A,$3C,$00,$24,$2A,$12,$00
    .byte $02,$3E,$02,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_28:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$24,$2A,$12,$00,$1C,$22,$1C,$00,$3E,$20,$3E,$00
    .byte $02,$3E,$02,$00,$3E,$08,$3E,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

hud_pixels_29:
    .byte $00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$3E,$18,$3E,$00,$3E,$2A,$22,$00,$24,$2A,$12,$00
    .byte $02,$3E,$02,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00

.section .ui_fonts, data
hud_font_tiny:
    .byte $00,$00,$00,$00,$00,$17,$00,$00,$01,$15,$02,$00,$01,$15,$02,$00,$01,$15,$02,$00,$01,$15,$02,$00
    .byte $01,$15,$02,$00,$01,$15,$02,$00,$01,$15,$02,$00,$01,$15,$02,$00,$01,$15,$02,$00,$04,$0E,$04,$00
    .byte $01,$15,$02,$00,$04,$04,$04,$00,$00,$10,$00,$00,$18,$04,$03,$00,$1F,$11,$1F,$00,$12,$1F,$10,$00
    .byte $19,$15,$12,$00,$11,$15,$0A,$00,$07,$04,$1F,$00,$17,$15,$09,$00,$1E,$15,$1D,$00,$01,$1D,$03,$00
    .byte $1F,$15,$1F,$00,$17,$15,$0F,$00,$00,$0A,$00,$00,$01,$15,$02,$00,$04,$0A,$11,$00,$01,$15,$02,$00
    .byte $11,$0A,$04,$00,$01,$15,$02,$00,$01,$15,$02,$00,$1E,$05,$1E,$00,$1F,$15,$0A,$00,$0E,$11,$11,$00
    .byte $1F,$11,$0E,$00,$1F,$15,$11,$00,$1F,$05,$01,$00,$0E,$11,$1D,$00,$1F,$04,$1F,$00,$11,$1F,$11,$00
    .byte $08,$10,$0F,$00,$1F,$04,$1B,$00,$1F,$10,$10,$00,$1F,$06,$1F,$00,$1F,$0E,$1F,$00,$0E,$11,$0E,$00
    .byte $1F,$05,$02,$00,$0E,$19,$1E,$00,$1F,$05,$1A,$00,$12,$15,$09,$00,$01,$1F,$01,$00,$1F,$10,$1F,$00
    .byte $0F,$10,$0F,$00,$1F,$0C,$1F,$00,$1B,$04,$1B,$00,$03,$1C,$03,$00,$19,$15,$13,$00

hud_font_normal:
    .byte $3E,$51,$49,$45,$3E,$00,$00,$42,$7F,$40,$00,$00,$42,$61,$51,$49,$46,$00,$41,$49,$49,$49,$36,$00
    .byte $18,$14,$12,$7F,$10,$00,$4F,$49,$49,$49,$31,$00,$3E,$49,$49,$49,$30,$00,$01,$71,$09,$05,$03,$00
    .byte $36,$49,$49,$49,$36,$00,$06,$49,$49,$49,$3E,$00,$00,$00,$00,$00,$00,$00

hud_font_tall:
    .byte $FC,$03,$C3,$33,$FC,$00,$0F,$33,$30,$30,$0F,$00,$00,$0C,$FF,$00,$00,$00,$00,$30,$3F,$30,$00,$00
    .byte $0C,$03,$03,$C3,$3C,$00,$30,$3C,$33,$30,$30,$00,$03,$C3,$C3,$C3,$3C,$00,$30,$30,$30,$30,$0F,$00
    .byte $C0,$30,$0C,$FF,$00,$00,$03,$03,$03,$3F,$03,$00,$FF,$C3,$C3,$C3,$03,$00,$30,$30,$30,$30,$0F,$00
    .byte $FC,$C3,$C3,$C3,$00,$00,$0F,$30,$30,$30,$0F,$00,$03,$03,$C3,$33,$0F,$00,$00,$3F,$00,$00,$00,$00
    .byte $3C,$C3,$C3,$C3,$3C,$00,$0F,$30,$30,$30,$0F,$00,$3C,$C3,$C3,$C3,$FC,$00,$00,$30,$30,$30,$0F,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_font_wide:
    .byte $FC,$FC,$03,$03,$C3,$C3,$33,$33,$FC,$FC,$00,$0F,$0F,$33,$33,$30,$30,$30,$30,$0F,$0F,$00,$00,$00
    .byte $0C,$0C,$FF,$FF,$00,$00,$00,$00,$00,$00,$00,$30,$30,$3F,$3F,$30,$30,$00,$00,$00,$0C,$0C,$03,$03
    .byte $03,$03,$C3,$C3,$3C,$3C,$00,$30,$30,$3C,$3C,$33,$33,$30,$30,$30,$30,$00,$03,$03,$C3,$C3,$C3,$C3
    .byte $C3,$C3,$3C,$3C,$00,$30,$30,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$C0,$C0,$30,$30,$0C,$0C,$FF,$FF
    .byte $00,$00,$00,$03,$03,$03,$03,$03,$03,$3F,$3F,$03,$03,$00,$FF,$FF,$C3,$C3,$C3,$C3,$C3,$C3,$03,$03
    .byte $00,$30,$30,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$FC,$FC,$C3,$C3,$C3,$C3,$C3,$C3,$00,$00,$00,$0F
    .byte $0F,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$03,$03,$03,$03,$C3,$C3,$33,$33,$0F,$0F,$00,$00,$00,$3F
    .byte $3F,$00,$00,$00,$00,$00,$00,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$3C,$3C,$00,$0F,$0F,$30,$30,$30
    .byte $30,$30,$30,$0F,$0F,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$FC,$FC,$00,$00,$00,$30,$30,$30,$30,$30
    .byte $30,$0F,$0F,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00
