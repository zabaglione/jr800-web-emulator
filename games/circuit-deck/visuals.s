; SPDX-License-Identifier: MIT
; circuit-deck: boss-battle, play origin 0
.equ VIEW_X,0
.equ HUD_FIELDS,25
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB battle
    CLRA
    ADDD #1
    LDX #hud_field_0
    JSR hud_number
    LDAB deck_hp
    CLRA
    LDX #hud_field_1
    JSR hud_number
    LDAB deck_block
    CLRA
    LDX #hud_field_2
    JSR hud_number
    LDAB energy
    CLRA
    LDX #hud_field_3
    JSR hud_number
    LDAB deck_enemy_hp
    CLRA
    LDX #hud_field_4
    JSR hud_number
    LDAB deck_enemy_block
    CLRA
    LDX #hud_field_5
    JSR hud_number
    LDAB battle
    CLRA
    LDX #hud_field_6
    JSR hud_choice
    TST deck_reward_view
    BNE hud_skip_7
    LDAA intent
    CMPA #1
    BEQ hud_expr_7_shield
    LDAB battle
    ADDB #4
    ADDB stage
    TSTA
    BEQ hud_expr_7_amount
    ADDB #3
hud_expr_7_amount:
    CLRA
    BRA hud_expr_7_done
hud_expr_7_shield:
    LDD #6
hud_expr_7_done:
    LDX #hud_field_7
    JSR hud_number
hud_skip_7:
    LDD #0
    TST deck_reward_view
    BEQ hud_expr_8_combat
    LDAB #2
    BRA hud_expr_8_done
hud_expr_8_combat:
    LDAA intent
    CMPA #1
    BNE hud_expr_8_done
    INCB
hud_expr_8_done:
    CLRA
    LDX #hud_field_8
    JSR hud_choice
    LDAB deck_message
    CLRA
    LDX #hud_field_9
    JSR hud_choice
    LDAB hand + 0
    TST deck_reward_view
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
    LDAB hand + 0
    TST deck_reward_view
    BEQ hud_expr_11_hand
    LDAB rewards + 0
hud_expr_11_hand:
    CMPB #255
    BNE hud_expr_11_valid
    LDAB #12
hud_expr_11_valid:
    CLRA
    LDX #hud_field_11
    JSR hud_choice
    LDAB hand + 0
    TST deck_reward_view
    BEQ hud_expr_12_hand
    LDAB rewards + 0
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
    BRA hud_expr_12_done
hud_expr_12_empty:
    LDD #0
hud_expr_12_done:
    LDX #hud_field_12
    JSR hud_number
    LDAB selected
    CMPB #0
    BEQ hud_expr_13_yes
    LDD #0
    BRA hud_expr_13_done
hud_expr_13_yes:
    LDD #1
hud_expr_13_done:
    LDX #hud_field_13
    JSR hud_choice
    LDAB hand + 1
    TST deck_reward_view
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
    LDAB hand + 1
    TST deck_reward_view
    BEQ hud_expr_15_hand
    LDAB rewards + 1
hud_expr_15_hand:
    CMPB #255
    BNE hud_expr_15_valid
    LDAB #12
hud_expr_15_valid:
    CLRA
    LDX #hud_field_15
    JSR hud_choice
    LDAB hand + 1
    TST deck_reward_view
    BEQ hud_expr_16_hand
    LDAB rewards + 1
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
    BRA hud_expr_16_done
hud_expr_16_empty:
    LDD #0
hud_expr_16_done:
    LDX #hud_field_16
    JSR hud_number
    LDAB selected
    CMPB #1
    BEQ hud_expr_17_yes
    LDD #0
    BRA hud_expr_17_done
hud_expr_17_yes:
    LDD #1
hud_expr_17_done:
    LDX #hud_field_17
    JSR hud_choice
    LDAB hand + 2
    TST deck_reward_view
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
    LDAB hand + 2
    TST deck_reward_view
    BEQ hud_expr_19_hand
    LDAB rewards + 2
hud_expr_19_hand:
    CMPB #255
    BNE hud_expr_19_valid
    LDAB #12
hud_expr_19_valid:
    CLRA
    LDX #hud_field_19
    JSR hud_choice
    LDAB hand + 2
    TST deck_reward_view
    BEQ hud_expr_20_hand
    LDAB rewards + 2
hud_expr_20_hand:
    CMPB #255
    BNE hud_expr_20_valid
    LDAB #12
hud_expr_20_valid:
    CLRA
    CMPB #12
    BEQ hud_expr_20_empty
    ASLB
    ASLB
    ASLB
    LDX #card_stats
    ABX
    LDAB 0,X
    CLRA
    BRA hud_expr_20_done
hud_expr_20_empty:
    LDD #0
hud_expr_20_done:
    LDX #hud_field_20
    JSR hud_number
    LDAB selected
    CMPB #2
    BEQ hud_expr_21_yes
    LDD #0
    BRA hud_expr_21_done
hud_expr_21_yes:
    LDD #1
hud_expr_21_done:
    LDX #hud_field_21
    JSR hud_choice
    LDD #0
    TST deck_reward_view
    BNE hud_expr_22_reward
    LDAB selected
    CMPB #3
    BNE hud_expr_22_plain
    LDD #1
    BRA hud_expr_22_done
hud_expr_22_plain:
    LDD #0
    BRA hud_expr_22_done
hud_expr_22_reward:
    LDD #2
hud_expr_22_done:
    LDX #hud_field_22
    JSR hud_choice
    LDAB deck_reward_view
    CLRA
    LDX #hud_field_23
    JSR hud_choice
    LDAB deck_reward_view
    CLRA
    LDX #hud_field_24
    JSR hud_choice
    RTS
hud_select_background:
    LDX #hud_span_table
    RTS
.section .data, data
view_origin: .byte 0
hud_field_0:
    .byte 135,0,1,0,128,0
    .word hud_cache + 0,0
hud_field_1:
    .byte 169,1,2,1,0,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 144,2,2,0,0,0
    .word hud_cache + 6,0
hud_field_3:
    .byte 175,0,2,0,128,0
    .word hud_cache + 9,0
hud_field_4:
    .byte 68,2,2,1,0,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 108,2,2,0,0,0
    .word hud_cache + 15,0
hud_field_6:
    .byte 56,1,10,0,128,9
    .word hud_cache + 18,hud_choices_6
hud_choices_6:
    .byte $53,$50,$41,$52,$4B,$20,$49,$4D,$50,$20,$57,$49,$52,$45,$20,$57,$4F,$4C,$46,$20,$49,$52,$4F,$4E
    .byte $20,$4D,$4F,$54,$48,$20,$43,$4F,$49,$4C,$20,$43,$52,$41,$42,$20,$54,$57,$49,$4E,$20,$46,$41,$4E
    .byte $47,$20,$4E,$4F,$56,$41,$20,$45,$59,$45,$20,$20,$47,$45,$41,$52,$20,$4B,$49,$4E,$47,$20,$56,$4F
    .byte $49,$44,$20,$57,$41,$52,$44,$20,$43,$4F,$52,$45,$20,$5A,$45,$52,$4F,$20

hud_field_7:
    .byte 173,3,2,0,128,0
    .word hud_cache + 21,0
hud_field_8:
    .byte 56,3,26,0,128,3
    .word hud_cache + 24,hud_choices_8
hud_choices_8:
    .byte $4E,$45,$58,$54,$3A,$20,$41,$54,$54,$41,$43,$4B,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$4E,$45,$58,$54,$3A,$20,$53,$48,$49,$45,$4C,$44,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$42,$4F,$53,$53,$20,$44,$45,$46,$45,$41,$54,$45,$44,$20,$20,$20,$20,$20,$20,$20
    .byte $20,$20,$20,$20,$20,$20

hud_field_9:
    .byte 2,7,22,0,128,23
    .word hud_cache + 27,hud_choices_9
hud_choices_9:
    .byte $44,$4F,$57,$4E,$3A,$45,$4E,$44,$20,$20,$53,$50,$41,$43,$45,$3A,$50,$4C,$41,$59,$20,$20,$42,$4C
    .byte $4F,$43,$4B,$45,$44,$20,$42,$59,$20,$53,$48,$49,$45,$4C,$44,$20,$20,$20,$20,$20,$53,$48,$49,$45
    .byte $4C,$44,$20,$54,$4F,$4F,$4B,$20,$54,$48,$45,$20,$48,$49,$54,$20,$20,$20,$45,$4E,$45,$4D,$59,$20
    .byte $41,$54,$54,$41,$43,$4B,$20,$20,$20,$20,$20,$20,$20,$20,$20,$20,$59,$4F,$55,$52,$20,$53,$48,$49
    .byte $45,$4C,$44,$20,$48,$45,$4C,$44,$20,$20,$20,$20,$20,$20,$45,$4E,$45,$4D,$59,$20,$53,$48,$49,$45
    .byte $4C,$44,$20,$2B,$36,$20,$20,$20,$20,$20,$20,$20,$50,$4F,$49,$53,$4F,$4E,$20,$44,$41,$4D,$41,$47
    .byte $45,$20,$20,$20,$20,$20,$20,$20,$20,$20,$50,$49,$43,$4B,$20,$43,$41,$52,$44,$20,$2F,$20,$4E,$45
    .byte $58,$54,$20,$42,$4F,$53,$53,$20,$4E,$4F,$54,$20,$45,$4E,$4F,$55,$47,$48,$20,$45,$4E,$45,$52,$47
    .byte $59,$20,$20,$20,$20,$20,$41,$4C,$4C,$20,$4E,$49,$4E,$45,$20,$42,$4F,$53,$53,$45,$53,$20,$44,$45
    .byte $46,$45,$41,$54,$53,$54,$52,$49,$4B,$45,$20,$2D,$20,$45,$4E,$45,$4D,$59,$20,$48,$49,$54,$20,$20
    .byte $20,$20,$47,$55,$41,$52,$44,$20,$2D,$20,$53,$48,$49,$45,$4C,$44,$20,$55,$50,$20,$20,$20,$20,$20
    .byte $53,$50,$41,$52,$4B,$20,$2D,$20,$45,$4E,$45,$4D,$59,$20,$48,$49,$54,$20,$20,$20,$20,$20,$50,$49
    .byte $45,$52,$43,$45,$20,$2D,$20,$48,$45,$41,$56,$59,$20,$48,$49,$54,$20,$20,$20,$20,$57,$41,$4C,$4C
    .byte $20,$2D,$20,$53,$48,$49,$45,$4C,$44,$20,$55,$50,$20,$20,$20,$20,$20,$20,$48,$45,$41,$4C,$20,$2D
    .byte $20,$52,$45,$43,$4F,$56,$45,$52,$20,$48,$50,$20,$20,$20,$20,$20,$56,$45,$4E,$4F,$4D,$20,$2D,$20
    .byte $50,$4F,$49,$53,$4F,$4E,$20,$41,$44,$44,$45,$44,$20,$20,$43,$48,$41,$52,$47,$45,$20,$2D,$20,$45
    .byte $4E,$45,$52,$47,$59,$20,$55,$50,$20,$20,$20,$20,$44,$52,$41,$49,$4E,$20,$2D,$20,$48,$49,$54,$20
    .byte $41,$4E,$44,$20,$48,$45,$41,$4C,$20,$20,$4E,$4F,$56,$41,$20,$2D,$20,$4D,$41,$53,$53,$49,$56,$45
    .byte $20,$48,$49,$54,$20,$20,$20,$20,$46,$4F,$43,$55,$53,$20,$2D,$20,$41,$54,$54,$41,$43,$4B,$20,$50
    .byte $4F,$57,$45,$52,$20,$55,$45,$43,$48,$4F,$20,$2D,$20,$48,$49,$54,$20,$41,$4E,$44,$20,$53,$48,$49
    .byte $45,$4C,$44,$20,$45,$4E,$20,$50,$41,$59,$53,$20,$43,$4F,$53,$54,$20,$2F,$20,$44,$4F,$57,$4E,$20
    .byte $45,$4E

hud_field_10:
    .byte 3,4,7,0,128,13
    .word hud_cache + 30,hud_choices_10
hud_choices_10:
    .byte $53,$54,$52,$49,$4B,$45,$20,$47,$55,$41,$52,$44,$20,$20,$53,$50,$41,$52,$4B,$20,$20,$50,$49,$45
    .byte $52,$43,$45,$20,$57,$41,$4C,$4C,$20,$20,$20,$48,$45,$41,$4C,$20,$20,$20,$56,$45,$4E,$4F,$4D,$20
    .byte $20,$43,$48,$41,$52,$47,$45,$20,$44,$52,$41,$49,$4E,$20,$20,$4E,$4F,$56,$41,$20,$20,$20,$46,$4F
    .byte $43,$55,$53,$20,$20,$45,$43,$48,$4F,$20,$20,$20,$2D,$2D,$20,$20,$20,$20,$20

hud_field_11:
    .byte 3,5,9,0,0,13
    .word hud_cache + 33,hud_choices_11
hud_choices_11:
    .byte $48,$49,$54,$20,$36,$20,$20,$20,$20,$41,$52,$4D,$20,$2B,$36,$20,$20,$20,$48,$49,$54,$20,$32,$20
    .byte $20,$20,$20,$48,$49,$54,$20,$31,$32,$20,$20,$20,$41,$52,$4D,$20,$2B,$31,$32,$20,$20,$48,$50,$20
    .byte $2B,$35,$20,$20,$20,$20,$50,$4F,$49,$53,$4F,$4E,$33,$20,$20,$45,$4E,$20,$2B,$31,$20,$20,$20,$20
    .byte $48,$49,$54,$37,$20,$48,$50,$34,$20,$48,$49,$54,$20,$32,$30,$20,$20,$20,$50,$4F,$57,$45,$52,$2B
    .byte $32,$20,$20,$48,$49,$54,$34,$20,$41,$52,$4D,$34,$20,$20,$20,$20,$20,$20,$20,$20,$20

hud_field_12:
    .byte 37,6,1,0,0,0
    .word hud_cache + 36,0
hud_field_13:
    .byte 3,6,5,0,0,2
    .word hud_cache + 39,hud_choices_13
hud_choices_13:
    .byte $20,$20,$20,$20,$20,$50,$4C,$41,$59,$20

hud_field_14:
    .byte 51,4,7,0,128,13
    .word hud_cache + 42,hud_choices_10

hud_field_15:
    .byte 51,5,9,0,0,13
    .word hud_cache + 45,hud_choices_11

hud_field_16:
    .byte 85,6,1,0,0,0
    .word hud_cache + 48,0
hud_field_17:
    .byte 51,6,5,0,0,2
    .word hud_cache + 51,hud_choices_13

hud_field_18:
    .byte 99,4,7,0,128,13
    .word hud_cache + 54,hud_choices_10

hud_field_19:
    .byte 99,5,9,0,0,13
    .word hud_cache + 57,hud_choices_11

hud_field_20:
    .byte 133,6,1,0,0,0
    .word hud_cache + 60,0
hud_field_21:
    .byte 99,6,5,0,0,2
    .word hud_cache + 63,hud_choices_13

hud_field_22:
    .byte 149,5,10,0,128,3
    .word hud_cache + 66,hud_choices_22
hud_choices_22:
    .byte $20,$54,$55,$52,$4E,$20,$45,$4E,$44,$20,$3E,$54,$55,$52,$4E,$20,$45,$4E,$44,$3C,$50,$49,$43,$4B
    .byte $20,$43,$41,$52,$44,$20

hud_field_23:
    .byte 157,4,6,0,128,2
    .word hud_cache + 69,hud_choices_23
hud_choices_23:
    .byte $41,$43,$54,$49,$4F,$4E,$52,$45,$57,$41,$52,$44

hud_field_24:
    .byte 157,6,6,0,0,2
    .word hud_cache + 72,hud_choices_24
hud_choices_24:
    .byte $20,$53,$50,$41,$43,$45,$48,$50,$20,$2B,$31,$32

hud_span_table:
    .word 0
    .byte 0,0,0
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


.section .data, data
deck_battle_art:
    .byte $0D,$FF,$FF,$E3,$DD,$DD,$FF,$DD,$C1,$DD,$FF,$C1,$F5,$CB,$84,$0C,$03,$C1,$DF,$C1,$84,$10,$04,$FD
    .byte $C1,$FD,$FF,$83,$01,$07,$C1,$DD,$E3,$FF,$C1,$D5,$DD,$85,$1C,$02,$F7,$C9,$84,$14,$08,$FF,$FF,$DB
    .byte $DB,$DB,$81,$89,$81,$82,$06,$A2,$01,$85,$29,$86,$39,$03,$C1,$D5,$EB,$82,$4B,$05,$E3,$FF,$DB,$D5
    .byte $ED,$84,$04,$8E,$01,$07,$CF,$F7,$F9,$FF,$D1,$D5,$E1,$8A,$12,$83,$7C,$03,$C1,$E3,$C1,$8F,$29,$86
    .byte $01,$01,$00,$B2,$01,$02,$FE,$00,$96,$4D,$AB,$01,$14,$01,$02,$04,$08,$FE,$00,$FC,$00,$00,$06,$38
    .byte $06,$00,$1C,$22,$1C,$00,$3E,$20,$3E,$84,$5E,$07,$3E,$08,$3E,$00,$3E,$0A,$04,$9E,$84,$01,$24,$83
    .byte $01,$02,$7E,$7E,$B3,$BF,$04,$00,$FF,$40,$FF,$A4,$66,$03,$24,$2A,$12,$85,$28,$82,$9E,$94,$01,$01
    .byte $3F,$82,$02,$04,$00,$3C,$0A,$3C,$82,$4A,$04,$34,$00,$3E,$0C,$85,$C0,$89,$01,$01,$10,$98,$01,$06
    .byte $00,$10,$38,$7C,$38,$10,$B4,$BD,$83,$77,$02,$01,$FF,$FF,$01,$88,$01,$02,$FC,$02,$A2,$25,$05,$F7
    .byte $E3,$C1,$E3,$F7,$82,$08,$03,$02,$FC,$00,$DF,$30,$82,$01,$A4,$33,$85,$01,$03,$02,$FC,$FF,$83,$32
    .byte $A5,$01,$82,$5D,$01,$FF,$82,$02,$DD,$30,$82,$01,$82,$35,$A7,$01,$05,$00,$FF,$3F,$40,$80,$95,$01
    .byte $08,$D0,$C8,$C4,$C0,$CE,$D1,$D1,$C0,$89,$12,$05,$97,$80,$40,$3F,$00,$DF,$30,$82,$01,$98,$33,$91
    .byte $01,$02,$40,$3F,$A8,$EB,$BB,$01,$06,$DB,$DB,$DB,$81,$89,$81,$82,$06,$CA,$01,$85,$51,$01,$FF,$00
deck_arrival_art:
    .byte $08,$FF,$FF,$DB,$DB,$DB,$81,$89,$81,$82,$06,$9E,$01,$85,$25,$01,$FF,$87,$01,$0D,$80,$B6,$B6,$B6
    .byte $C9,$FF,$C1,$BE,$BE,$BE,$C1,$FF,$B9,$82,$0C,$01,$CE,$86,$06,$86,$01,$03,$BE,$80,$BE,$82,$24,$04
    .byte $FD,$FB,$F7,$80,$84,$24,$01,$BE,$86,$2A,$05,$80,$FD,$F3,$FD,$80,$8E,$1E,$03,$B6,$B6,$86,$88,$56
    .byte $B0,$8F,$01,$00,$E1,$01,$04,$FC,$02,$F1,$01,$82,$01,$17,$49,$55,$25,$01,$45,$7D,$45,$01,$39,$45
    .byte $75,$01,$7D,$39,$7D,$01,$79,$15,$79,$01,$7D,$41,$41,$83,$1B,$0C,$01,$7D,$45,$39,$01,$7D,$55,$45
    .byte $01,$05,$7D,$05,$84,$08,$02,$39,$45,$89,$0C,$83,$1C,$97,$01,$02,$02,$FC,$E2,$C0,$01,$FF,$82,$02
    .byte $D9,$5C,$E7,$C0,$06,$24,$24,$24,$7E,$76,$7E,$82,$06,$C1,$01,$85,$48,$EE,$C0,$D4,$59,$FF,$C0,$FF
    .byte $C0,$A4,$01,$04,$3F,$40,$8F,$80,$D3,$01,$0D,$87,$85,$87,$40,$3F,$FF,$FF,$DB,$DB,$DB,$81,$89,$81
    .byte $82,$06,$CD,$01,$85,$54,$01,$FF,$91,$01,$1B,$FD,$C1,$FD,$FF,$C1,$F5,$CB,$FF,$C3,$F5,$C3,$FF,$C1
    .byte $E3,$C1,$FF,$DB,$D5,$ED,$FF,$C1,$F3,$C1,$FF,$DD,$C1,$DD,$84,$0C,$83,$04,$83,$0C,$03,$E3,$DD,$E3
    .byte $84,$20,$9D,$01,$00
deck_result_art:
    .byte $08,$FF,$FF,$DB,$DB,$DB,$81,$89,$81,$82,$06,$A2,$01,$85,$29,$01,$FF,$86,$01,$02,$C1,$BE,$82,$01
    .byte $04,$FF,$FF,$BE,$80,$82,$05,$05,$80,$F6,$E6,$D6,$B9,$86,$12,$05,$C0,$BF,$BF,$BF,$C0,$86,$18,$05
    .byte $FE,$FE,$80,$FE,$FE,$86,$30,$01,$80,$82,$1D,$07,$C1,$FF,$80,$B6,$B6,$B6,$BE,$86,$2A,$04,$80,$F7
    .byte $EB,$DD,$82,$25,$86,$01,$AB,$8A,$87,$8B,$03,$00,$00,$08,$95,$01,$05,$10,$20,$20,$40,$00,$82,$01
    .byte $02,$80,$40,$FA,$01,$01,$80,$83,$82,$04,$40,$20,$20,$10,$96,$A5,$83,$1F,$06,$00,$00,$F8,$08,$08
    .byte $C8,$85,$10,$01,$88,$83,$01,$02,$F8,$80,$88,$01,$02,$00,$00,$82,$E1,$FB,$01,$82,$7F,$89,$8E,$01
    .byte $F8,$84,$9F,$85,$AA,$82,$B1,$01,$F8,$8A,$25,$01,$01,$83,$08,$01,$80,$83,$0A,$01,$1C,$8D,$3E,$FF
    .byte $C0,$8D,$82,$82,$9D,$01,$FF,$82,$A7,$82,$01,$82,$B1,$88,$19,$84,$09,$05,$04,$0E,$1F,$3F,$1F,$84
    .byte $0A,$07,$40,$E0,$F0,$F8,$F0,$E0,$40,$FF,$C0,$8B,$C0,$89,$93,$02,$00,$FF,$82,$A7,$02,$0E,$04,$88
    .byte $B7,$83,$01,$02,$3F,$20,$87,$01,$86,$13,$02,$01,$03,$82,$DF,$85,$22,$03,$3F,$40,$80,$FA,$01,$02
    .byte $40,$3F,$87,$A7,$87,$93,$02,$00,$FF,$88,$AD,$82,$1C,$83,$01,$88,$10,$82,$01,$02,$23,$22,$88,$01
    .byte $05,$12,$0A,$0A,$06,$03,$87,$E2,$FB,$01,$05,$03,$06,$0A,$0A,$12,$89,$98,$01,$23,$8B,$B0,$82,$CF
    .byte $07,$FF,$DB,$DB,$DB,$81,$89,$81,$82,$06,$FF,$01,$AD,$01,$85,$B4,$01,$FF,$00
