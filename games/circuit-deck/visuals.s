; SPDX-License-Identifier: MIT
; circuit-deck: card-table, play origin 0
.equ VIEW_X,0
.equ HUD_FIELDS,20
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB battle
    CLRA
    ADDD #1
    LDX #hud_field_0
    JSR hud_number
    LDAB hp
    CLRA
    LDX #hud_field_1
    JSR hud_number
    LDAB block
    CLRA
    LDX #hud_field_2
    JSR hud_number
    LDAB energy
    CLRA
    LDX #hud_field_3
    JSR hud_number
    LDAB enemy_hp
    CLRA
    LDX #hud_field_4
    JSR hud_number
    LDAB enemy_block
    CLRA
    LDX #hud_field_5
    JSR hud_number
    LDAB #6
    LDAA intent
    CMPA #1
    BEQ hud_expr_6_done
    LDAB battle
    ADDB #4
    ADDB stage
    TSTA
    BEQ hud_expr_6_done
    ADDB #3
hud_expr_6_done:
    CLRA
    LDX #hud_field_6
    JSR hud_number
    LDAB intent
    CMPB #1
    BEQ hud_expr_7_shield
    CLRB
    BRA hud_expr_7_mode
hud_expr_7_shield:
    LDAB #1
hud_expr_7_mode:
    TST battle_mode
    BEQ hud_expr_7_help
    LDAB #2
hud_expr_7_help:
    TST card_help
    BEQ hud_expr_7_done
    LDAB #3
hud_expr_7_done:
    CLRA
    LDX #hud_field_7
    JSR hud_choice
    LDAB hand + 0
    TST battle_mode
    BEQ hud_expr_8_hand
    LDAB rewards + 0
hud_expr_8_hand:
    CMPB #255
    BNE hud_expr_8_valid
    LDAB #12
hud_expr_8_valid:
    CLRA
    CMPB #12
    BEQ hud_expr_8_empty
    ASLB
    ASLB
    ASLB
    LDX #card_stats
    ABX
    LDAB 0,X
    CLRA
    BRA hud_expr_8_costdone
hud_expr_8_empty:
    LDD #0
hud_expr_8_costdone:
    LDX #hud_field_8
    JSR hud_number
    LDAB hand + 0
    TST battle_mode
    BEQ hud_expr_9_hand
    LDAB rewards + 0
hud_expr_9_hand:
    CMPB #255
    BNE hud_expr_9_valid
    LDAB #12
hud_expr_9_valid:
    CLRA
    LDX #hud_field_9
    JSR hud_choice
    LDAB hand + 0
    TST battle_mode
    BEQ hud_expr_10_hand
    LDAB rewards + 0
hud_expr_10_hand:
    CMPB #255
    BNE hud_expr_10_valid
    LDAB #12
hud_expr_10_valid:
    CLRA
    LDX #hud_field_10
    JSR hud_choice
    LDAB selected
    CLRA
    CMPB #0
    BEQ hud_expr_11_yes
    LDD #0
    BRA hud_expr_11_done
hud_expr_11_yes:
    LDD #1
hud_expr_11_done:
    LDX #hud_field_11
    JSR hud_choice
    LDAB hand + 1
    TST battle_mode
    BEQ hud_expr_12_hand
    LDAB rewards + 1
hud_expr_12_hand:
    CMPB #255
    BNE hud_expr_12_valid
    LDAB #12
hud_expr_12_valid:
    CLRA
    CMPB #12
    BEQ hud_expr_12_empty
    ASLB
    ASLB
    ASLB
    LDX #card_stats
    ABX
    LDAB 0,X
    CLRA
    BRA hud_expr_12_costdone
hud_expr_12_empty:
    LDD #0
hud_expr_12_costdone:
    LDX #hud_field_12
    JSR hud_number
    LDAB hand + 1
    TST battle_mode
    BEQ hud_expr_13_hand
    LDAB rewards + 1
hud_expr_13_hand:
    CMPB #255
    BNE hud_expr_13_valid
    LDAB #12
hud_expr_13_valid:
    CLRA
    LDX #hud_field_13
    JSR hud_choice
    LDAB hand + 1
    TST battle_mode
    BEQ hud_expr_14_hand
    LDAB rewards + 1
hud_expr_14_hand:
    CMPB #255
    BNE hud_expr_14_valid
    LDAB #12
hud_expr_14_valid:
    CLRA
    LDX #hud_field_14
    JSR hud_choice
    LDAB selected
    CLRA
    CMPB #1
    BEQ hud_expr_15_yes
    LDD #0
    BRA hud_expr_15_done
hud_expr_15_yes:
    LDD #1
hud_expr_15_done:
    LDX #hud_field_15
    JSR hud_choice
    LDAB hand + 2
    TST battle_mode
    BEQ hud_expr_16_hand
    LDAB rewards + 2
hud_expr_16_hand:
    CMPB #255
    BNE hud_expr_16_valid
    LDAB #12
hud_expr_16_valid:
    CLRA
    CMPB #12
    BEQ hud_expr_16_empty
    ASLB
    ASLB
    ASLB
    LDX #card_stats
    ABX
    LDAB 0,X
    CLRA
    BRA hud_expr_16_costdone
hud_expr_16_empty:
    LDD #0
hud_expr_16_costdone:
    LDX #hud_field_16
    JSR hud_number
    LDAB hand + 2
    TST battle_mode
    BEQ hud_expr_17_hand
    LDAB rewards + 2
hud_expr_17_hand:
    CMPB #255
    BNE hud_expr_17_valid
    LDAB #12
hud_expr_17_valid:
    CLRA
    LDX #hud_field_17
    JSR hud_choice
    LDAB hand + 2
    TST battle_mode
    BEQ hud_expr_18_hand
    LDAB rewards + 2
hud_expr_18_hand:
    CMPB #255
    BNE hud_expr_18_valid
    LDAB #12
hud_expr_18_valid:
    CLRA
    LDX #hud_field_18
    JSR hud_choice
    LDAB selected
    CLRA
    CMPB #2
    BEQ hud_expr_19_yes
    LDD #0
    BRA hud_expr_19_done
hud_expr_19_yes:
    LDD #1
hud_expr_19_done:
    LDX #hud_field_19
    JSR hud_choice
    RTS
hud_select_background:
    LDX #hud_span_table
    RTS
.section .data, data
view_origin: .byte 0
hud_field_0:
    .byte 26,0,3,1,128,0
    .word hud_cache + 0,0
hud_field_1:
    .byte 62,0,3,1,128,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 115,0,3,1,128,0
    .word hud_cache + 6,0
hud_field_3:
    .byte 164,0,3,1,128,0
    .word hud_cache + 9,0
hud_field_4:
    .byte 37,1,3,1,0,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 105,1,3,1,0,0
    .word hud_cache + 15,0
hud_field_6:
    .byte 164,1,3,1,0,0
    .word hud_cache + 18,0
hud_field_7:
    .byte 6,2,44,0,128,4
    .word hud_cache + 21,hud_choices_7
hud_choices_7:
    .byte $4E,$45,$58,$54,$3A,$20,$45,$4E,$45,$4D,$59,$20,$41,$54,$54,$41,$43,$4B,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$4E,$45,$58,$54
    .byte $3A,$20,$45,$4E,$45,$4D,$59,$20,$53,$48,$49,$45,$4C,$44,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$52,$45,$57,$41,$52,$44,$3A,$20
    .byte $43,$48,$4F,$4F,$53,$45,$20,$4F,$4E,$45,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$52,$45,$54,$55,$52,$4E,$20,$4D,$45,$4E,$55,$3A
    .byte $20,$45,$4E,$44,$20,$54,$55,$52,$4E,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20

hud_field_8:
    .byte 44,3,2,1,128,0
    .word hud_cache + 24,0
hud_field_9:
    .byte 5,4,8,5,0,13
    .word hud_cache + 27,hud_choices_9
hud_choices_9:
    .byte $53,$54,$52,$49,$4B,$45,$20,$20,$47,$55,$41,$52,$44,$20,$20,$20,$53,$50,$41,$52,$4B,$20,$20,$20
    .byte $50,$49,$45,$52,$43,$45,$20,$20,$57,$41,$4C,$4C,$20,$20,$20,$20,$48,$45,$41,$4C,$20,$20,$20,$20
    .byte $56,$45,$4E,$4F,$4D,$20,$20,$20,$43,$48,$41,$52,$47,$45,$20,$20,$44,$52,$41,$49,$4E,$20,$20,$20
    .byte $4E,$4F,$56,$41,$20,$20,$20,$20,$46,$4F,$43,$55,$53,$20,$20,$20,$45,$43,$48,$4F,$20,$20,$20,$20
    .byte $2D,$2D,$20,$20,$20,$20,$20,$20

hud_field_10:
    .byte 5,5,8,5,0,13
    .word hud_cache + 30,hud_choices_10
hud_choices_10:
    .byte $48,$49,$54,$20,$36,$20,$20,$20,$42,$4C,$4F,$43,$4B,$20,$36,$20,$48,$49,$54,$20,$32,$20,$20,$20
    .byte $48,$49,$54,$20,$31,$32,$20,$20,$42,$4C,$4B,$20,$31,$32,$20,$20,$48,$45,$41,$4C,$20,$35,$20,$20
    .byte $54,$4F,$58,$49,$4E,$20,$33,$20,$45,$4E,$45,$52,$47,$59,$31,$20,$48,$49,$54,$2B,$48,$50,$20,$20
    .byte $48,$49,$54,$20,$32,$30,$20,$20,$50,$4F,$57,$45,$52,$20,$32,$20,$48,$49,$54,$2B,$41,$52,$4D,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20

hud_field_11:
    .byte 4,7,14,0,128,2
    .word hud_cache + 33,hud_choices_11
hud_choices_11:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$3E,$20,$53,$45,$4C,$45,$43,$54,$20,$3C
    .byte $20,$20,$20,$20

hud_field_12:
    .byte 108,3,2,1,128,0
    .word hud_cache + 36,0
hud_field_13:
    .byte 69,4,8,5,0,13
    .word hud_cache + 39,hud_choices_13
hud_choices_13:
    .byte $53,$54,$52,$49,$4B,$45,$20,$20,$47,$55,$41,$52,$44,$20,$20,$20,$53,$50,$41,$52,$4B,$20,$20,$20
    .byte $50,$49,$45,$52,$43,$45,$20,$20,$57,$41,$4C,$4C,$20,$20,$20,$20,$48,$45,$41,$4C,$20,$20,$20,$20
    .byte $56,$45,$4E,$4F,$4D,$20,$20,$20,$43,$48,$41,$52,$47,$45,$20,$20,$44,$52,$41,$49,$4E,$20,$20,$20
    .byte $4E,$4F,$56,$41,$20,$20,$20,$20,$46,$4F,$43,$55,$53,$20,$20,$20,$45,$43,$48,$4F,$20,$20,$20,$20
    .byte $2D,$2D,$20,$20,$20,$20,$20,$20

hud_field_14:
    .byte 69,5,8,5,0,13
    .word hud_cache + 42,hud_choices_14
hud_choices_14:
    .byte $48,$49,$54,$20,$36,$20,$20,$20,$42,$4C,$4F,$43,$4B,$20,$36,$20,$48,$49,$54,$20,$32,$20,$20,$20
    .byte $48,$49,$54,$20,$31,$32,$20,$20,$42,$4C,$4B,$20,$31,$32,$20,$20,$48,$45,$41,$4C,$20,$35,$20,$20
    .byte $54,$4F,$58,$49,$4E,$20,$33,$20,$45,$4E,$45,$52,$47,$59,$31,$20,$48,$49,$54,$2B,$48,$50,$20,$20
    .byte $48,$49,$54,$20,$32,$30,$20,$20,$50,$4F,$57,$45,$52,$20,$32,$20,$48,$49,$54,$2B,$41,$52,$4D,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20

hud_field_15:
    .byte 68,7,14,0,128,2
    .word hud_cache + 45,hud_choices_15
hud_choices_15:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$3E,$20,$53,$45,$4C,$45,$43,$54,$20,$3C
    .byte $20,$20,$20,$20

hud_field_16:
    .byte 172,3,2,1,128,0
    .word hud_cache + 48,0
hud_field_17:
    .byte 133,4,8,5,0,13
    .word hud_cache + 51,hud_choices_17
hud_choices_17:
    .byte $53,$54,$52,$49,$4B,$45,$20,$20,$47,$55,$41,$52,$44,$20,$20,$20,$53,$50,$41,$52,$4B,$20,$20,$20
    .byte $50,$49,$45,$52,$43,$45,$20,$20,$57,$41,$4C,$4C,$20,$20,$20,$20,$48,$45,$41,$4C,$20,$20,$20,$20
    .byte $56,$45,$4E,$4F,$4D,$20,$20,$20,$43,$48,$41,$52,$47,$45,$20,$20,$44,$52,$41,$49,$4E,$20,$20,$20
    .byte $4E,$4F,$56,$41,$20,$20,$20,$20,$46,$4F,$43,$55,$53,$20,$20,$20,$45,$43,$48,$4F,$20,$20,$20,$20
    .byte $2D,$2D,$20,$20,$20,$20,$20,$20

hud_field_18:
    .byte 133,5,8,5,0,13
    .word hud_cache + 54,hud_choices_18
hud_choices_18:
    .byte $48,$49,$54,$20,$36,$20,$20,$20,$42,$4C,$4F,$43,$4B,$20,$36,$20,$48,$49,$54,$20,$32,$20,$20,$20
    .byte $48,$49,$54,$20,$31,$32,$20,$20,$42,$4C,$4B,$20,$31,$32,$20,$20,$48,$45,$41,$4C,$20,$35,$20,$20
    .byte $54,$4F,$58,$49,$4E,$20,$33,$20,$45,$4E,$45,$52,$47,$59,$31,$20,$48,$49,$54,$2B,$48,$50,$20,$20
    .byte $48,$49,$54,$20,$32,$30,$20,$20,$50,$4F,$57,$45,$52,$20,$32,$20,$48,$49,$54,$2B,$41,$52,$4D,$20
    .byte $20,$20,$20,$20,$20,$20,$20,$20

hud_field_19:
    .byte 132,7,14,0,128,2
    .word hud_cache + 57,hud_choices_19
hud_choices_19:
    .byte $20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$3E,$20,$53,$45,$4C,$45,$43,$54,$20,$3C
    .byte $20,$20,$20,$20

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
    .word hud_pixels_6
    .byte 2,64,64
    .word hud_pixels_6
    .byte 2,128,64
    .word hud_pixels_7
    .byte 3,0,64
    .word hud_pixels_7
    .byte 3,64,64
    .word hud_pixels_7
    .byte 3,128,64
    .word hud_pixels_8
    .byte 4,0,64
    .word hud_pixels_8
    .byte 4,64,64
    .word hud_pixels_8
    .byte 4,128,64
    .word hud_pixels_8
    .byte 5,0,64
    .word hud_pixels_8
    .byte 5,64,64
    .word hud_pixels_8
    .byte 5,128,64
    .word hud_pixels_9
    .byte 6,0,64
    .word hud_pixels_10
    .byte 6,64,64
    .word hud_pixels_11
    .byte 6,128,64
    .word hud_pixels_6
    .byte 7,0,64
    .word hud_pixels_6
    .byte 7,64,64
    .word hud_pixels_6
    .byte 7,128,64
    .word 0
    .byte 0,0,0
hud_pixels_0:
    .byte $FF,$C1,$F5,$FD,$FF,$DD,$C1,$DD,$FF,$E3,$DD,$C5,$FF,$C1,$F7,$C1,$FF,$FD,$C1,$FD,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $C1,$F7,$C1,$FF,$C1,$F5,$FB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_1:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$CB,$FF,$C1,$F3,$C1,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_2:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1,$D5,$DD
    .byte $FF,$C1,$E3,$C1,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_3:
    .byte $00,$00,$3E,$2A,$22,$00,$3E,$1C,$3E,$00,$3E,$2A,$22,$00,$3E,$0C,$3E,$00,$06,$38,$06,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_4:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$3C,$0A,$3C,$00,$3E,$0A,$34,$00,$3E,$0C,$3E,$00
    .byte $1C,$22,$1C,$00,$3E,$0A,$34,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_5:
    .byte $00,$00,$00,$00,$00,$22,$3E,$22,$00,$3E,$1C,$3E,$00,$02,$3E,$02,$00,$3E,$2A,$22,$00,$3E,$1C,$3E
    .byte $00,$02,$3E,$02,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_6:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_7:
    .byte $FF,$FF,$FF,$FF,$E3,$DD,$DD,$FF,$E3,$DD,$E3,$FF,$DB,$D5,$ED,$FF,$FD,$C1,$FD,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_8:
    .byte $FF,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF

hud_pixels_9:
    .byte $FF,$00,$00,$00,$00,$00,$08,$14,$22,$00,$00,$00,$00,$00,$24,$2A,$12,$00,$3E,$2A,$22,$00,$3E,$20
    .byte $20,$00,$3E,$2A,$22,$00,$1C,$22,$22,$00,$02,$3E,$02,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF

hud_pixels_10:
    .byte $FF,$00,$00,$00,$00,$00,$24,$2A,$12,$00,$3E,$0A,$04,$00,$3C,$0A,$3C,$00,$1C,$22,$22,$00,$3E,$2A
    .byte $22,$00,$00,$14,$00,$00,$00,$00,$00,$00,$3E,$0A,$04,$00,$3E,$20,$20,$00,$3C,$0A,$3C,$00,$06,$38
    .byte $06,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF

hud_pixels_11:
    .byte $FF,$00,$00,$00,$00,$00,$24,$2A,$12,$00,$3E,$2A,$22,$00,$3E,$20,$20,$00,$3E,$2A,$22,$00,$1C,$22
    .byte $22,$00,$02,$3E,$02,$00,$00,$00,$00,$00,$22,$14,$08,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF

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
    .byte $36,$41,$41,$41,$36,$00,$00,$00,$00,$00,$36,$00,$30,$49,$49,$49,$06,$00,$00,$49,$49,$49,$36,$00
    .byte $06,$08,$08,$08,$36,$00,$06,$49,$49,$49,$30,$00,$36,$49,$49,$49,$30,$00,$00,$01,$01,$01,$36,$00
    .byte $36,$49,$49,$49,$36,$00,$06,$49,$49,$49,$36,$00,$00,$00,$00,$00,$00,$00

hud_font_tall:
    .byte $3C,$03,$03,$03,$3C,$00,$0F,$30,$30,$30,$0F,$00,$00,$00,$00,$00,$3C,$00,$00,$00,$00,$00,$0F,$00
    .byte $00,$C3,$C3,$C3,$3C,$00,$0F,$30,$30,$30,$00,$00,$00,$C3,$C3,$C3,$3C,$00,$00,$30,$30,$30,$0F,$00
    .byte $3C,$C0,$C0,$C0,$3C,$00,$00,$00,$00,$00,$0F,$00,$3C,$C3,$C3,$C3,$00,$00,$00,$30,$30,$30,$0F,$00
    .byte $3C,$C3,$C3,$C3,$00,$00,$0F,$30,$30,$30,$0F,$00,$00,$03,$03,$03,$3C,$00,$00,$00,$00,$00,$0F,$00
    .byte $3C,$C3,$C3,$C3,$3C,$00,$0F,$30,$30,$30,$0F,$00,$3C,$C3,$C3,$C3,$3C,$00,$00,$30,$30,$30,$0F,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_font_wide:
    .byte $3C,$3C,$03,$03,$03,$03,$03,$03,$3C,$3C,$00,$0F,$0F,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$3C,$3C,$00,$00,$00,$00,$00,$00,$00,$00,$00,$0F,$0F,$00,$00,$00,$C3,$C3
    .byte $C3,$C3,$C3,$C3,$3C,$3C,$00,$0F,$0F,$30,$30,$30,$30,$30,$30,$00,$00,$00,$00,$00,$C3,$C3,$C3,$C3
    .byte $C3,$C3,$3C,$3C,$00,$00,$00,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$3C,$3C,$C0,$C0,$C0,$C0,$C0,$C0
    .byte $3C,$3C,$00,$00,$00,$00,$00,$00,$00,$00,$00,$0F,$0F,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$00,$00
    .byte $00,$00,$00,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$00,$00,$00,$0F
    .byte $0F,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$00,$00,$03,$03,$03,$03,$03,$03,$3C,$3C,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$0F,$0F,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$3C,$3C,$00,$0F,$0F,$30,$30,$30
    .byte $30,$30,$30,$0F,$0F,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$3C,$3C,$00,$00,$00,$30,$30,$30,$30,$30
    .byte $30,$0F,$0F,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00
