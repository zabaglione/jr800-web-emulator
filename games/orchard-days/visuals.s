; SPDX-License-Identifier: MIT
; orchard-days: seed-packet, play origin 0
.equ VIEW_X,0
.equ HUD_FIELDS,13
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB orchard_day
    CLRA
    ADDD #1
    LDX #hud_field_0
    JSR hud_number
    LDAB orchard_limit
    CLRA
    LDX #hud_field_1
    JSR hud_number
    LDAB orchard_rain
    CLRA
    LDX #hud_field_2
    JSR hud_choice
    LDAB orchard_actions
    CLRA
    LDX #hud_field_3
    JSR hud_number
    LDAB stage
    CLRA
    ADDD #1
    LDX #hud_field_4
    JSR hud_number
    LDD orchard_coins
    LDX #hud_field_5
    JSR hud_number
    LDAB orchard_goal
    CLRA
    LDX #hud_field_6
    JSR hud_number
    LDAB orchard_seed
    DECB
    CLRA
    LDX #hud_field_7
    JSR hud_choice
    LDAB orchard_seed
    LDX #orchard_costs
    ABX
    LDAB 0,X
    CLRA
    LDX #hud_field_8
    JSR hud_number
    LDAB orchard_seed
    LDX #orchard_durations
    ABX
    LDAB 0,X
    CLRA
    LDX #hud_field_9
    JSR hud_number
    LDAB orchard_seed
    LDX #orchard_prices
    ABX
    LDAB 0,X
    CLRA
    LDX #hud_field_10
    JSR hud_number
    CLRB
    LDAA orchard_button
    CMPA #1
    BNE hud_expr_11_done
    TST cursor_blink_mask
    BEQ hud_expr_11_done
    INCB
hud_expr_11_done:
    CLRA
    LDX #hud_field_11
    JSR hud_choice
    CLRB
    LDAA orchard_button
    CMPA #2
    BNE hud_expr_12_done
    TST cursor_blink_mask
    BEQ hud_expr_12_done
    INCB
hud_expr_12_done:
    CLRA
    LDX #hud_field_12
    JSR hud_choice
    RTS
hud_select_background:
    LDX #hud_span_table
    RTS
.section .data, data
view_origin: .byte 0
hud_field_0:
    .byte 17,0,2,0,128,0
    .word hud_cache + 0,0
hud_field_1:
    .byte 34,0,2,0,128,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 52,0,4,0,128,2
    .word hud_cache + 6,hud_choices_2
hud_choices_2:
    .byte $53,$55,$4E,$20,$52,$41,$49,$4E

hud_field_3:
    .byte 104,0,1,0,128,0
    .word hud_cache + 9,0
hud_field_4:
    .byte 178,0,3,0,128,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 164,1,4,1,0,0
    .word hud_cache + 15,0
hud_field_6:
    .byte 170,2,3,1,0,0
    .word hud_cache + 18,0
hud_field_7:
    .byte 163,3,5,0,0,2
    .word hud_cache + 21,hud_choices_7
hud_choices_7:
    .byte $42,$45,$41,$4E,$20,$42,$45,$52,$52,$59

hud_field_8:
    .byte 174,4,2,0,0,0
    .word hud_cache + 24,0
hud_field_9:
    .byte 174,5,2,0,0,0
    .word hud_cache + 27,0
hud_field_10:
    .byte 174,6,2,0,0,0
    .word hud_cache + 30,0
hud_field_11:
    .byte 4,7,1,0,128,2
    .word hud_cache + 33,hud_choices_11
hud_choices_11:
    .byte $20,$3E

hud_field_12:
    .byte 68,7,1,0,128,2
    .word hud_cache + 36,hud_choices_12
hud_choices_12:
    .byte $20,$3E

hud_span_table:
    .word hud_pixels_0
    .byte 0,0,64
    .word hud_pixels_1
    .byte 0,64,64
    .word hud_pixels_2
    .byte 0,128,64
    .word hud_pixels_3
    .byte 7,0,64
    .word hud_pixels_4
    .byte 7,64,64
    .word hud_pixels_5
    .byte 7,128,64
    .word hud_pixels_6
    .byte 1,128,64
    .word hud_pixels_7
    .byte 2,128,64
    .word hud_pixels_8
    .byte 3,128,64
    .word hud_pixels_9
    .byte 4,128,64
    .word hud_pixels_10
    .byte 5,128,64
    .word hud_pixels_11
    .byte 6,128,64
    .word 0
    .byte 0,0,0
hud_pixels_0:
    .byte $FF,$FF,$C1,$DD,$E3,$FF,$C3,$F5,$C3,$FF,$F9,$C7,$F9,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$CF,$F7,$F9,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_1:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$C3,$F5,$C3,$FF,$E3,$DD,$DD,$FF,$FD,$C1,$FD,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_2:
    .byte $FF,$FF,$FF,$E3,$DD,$E3,$FF,$C1,$F5,$CB,$FF,$E3,$DD,$DD,$FF,$C1,$F7,$C1,$FF,$C3,$F5,$C3,$FF,$C1
    .byte $F5,$CB,$FF,$C1,$DD,$E3,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_3:
    .byte $FF,$C1,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1,$E3,$C1,$FF,$C1,$D5,$DD,$FF,$C9,$F7,$C9,$FF
    .byte $FD,$C1,$FD,$FF,$FF,$FF,$FF,$FF,$C1,$DD,$E3,$FF,$C3,$F5,$C3,$FF,$F9,$C7,$F9,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$BF,$FF

hud_pixels_4:
    .byte $FF,$C1,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$DB,$D5,$ED,$FF,$C1,$E7,$C1,$FF,$C3,$F5,$C3,$FF
    .byte $C1,$F5,$FB,$FF,$FF,$FF,$FF,$FF,$DB,$D5,$ED,$FF,$C1,$D5,$DD,$FF,$C1,$D5,$DD,$FF,$C1,$DD,$E3,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$BF,$FF

hud_pixels_5:
    .byte $00,$00,$00,$00,$3F,$20,$20,$20,$32,$35,$29,$20,$3F,$35,$31,$20,$3F,$35,$31,$20,$3F,$31,$2E,$20
    .byte $20,$20,$20,$20,$21,$3F,$21,$20,$2E,$31,$2E,$20,$20,$20,$20,$20,$3F,$25,$22,$20,$3F,$30,$30,$20
    .byte $3E,$25,$3E,$20,$3F,$2E,$3F,$20,$21,$3F,$21,$20,$20,$20,$3F,$00

hud_pixels_6:
    .byte $00,$00,$00,$00,$00,$38,$44,$44,$00,$38,$44,$38,$00,$44,$7C,$44,$00,$7C,$38,$7C,$00,$48,$54,$24
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_7:
    .byte $00,$00,$00,$00,$00,$38,$44,$74,$00,$38,$44,$38,$00,$78,$14,$78,$00,$7C,$40,$40,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_pixels_8:
    .byte $00,$00,$00,$00,$FC,$03,$81,$C9,$D5,$A5,$81,$FD,$D5,$C5,$81,$FD,$D5,$C5,$81,$FD,$C5,$B9,$81,$81
    .byte $81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81
    .byte $81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$81,$82,$04,$F8,$00

hud_pixels_9:
    .byte $00,$00,$00,$00,$FF,$00,$00,$00,$00,$38,$44,$44,$00,$38,$44,$38,$00,$48,$54,$24,$00,$04,$7C,$04
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00

hud_pixels_10:
    .byte $00,$00,$00,$00,$FF,$00,$00,$00,$00,$38,$44,$74,$00,$7C,$14,$68,$00,$38,$44,$38,$00,$7C,$30,$7C
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$7C,$44,$38,$00,$00,$00,$FF,$00

hud_pixels_11:
    .byte $00,$00,$00,$00,$FF,$00,$00,$00,$00,$48,$54,$24,$00,$7C,$54,$44,$00,$7C,$40,$40,$00,$7C,$40,$40
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00

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
    .byte $3E,$41,$41,$41,$3E,$00,$00,$42,$7F,$40,$00,$00,$62,$51,$49,$49,$46,$00,$41,$49,$49,$49,$36,$00
    .byte $0F,$08,$08,$7F,$08,$00,$4F,$49,$49,$49,$31,$00,$3E,$49,$49,$49,$30,$00,$01,$01,$79,$05,$03,$00
    .byte $36,$49,$49,$49,$36,$00,$06,$49,$49,$49,$3E,$00,$00,$00,$00,$00,$00,$00

hud_font_tall:
    .byte $FC,$03,$03,$03,$FC,$00,$0F,$30,$30,$30,$0F,$00,$00,$0C,$FF,$00,$00,$00,$00,$30,$3F,$30,$00,$00
    .byte $0C,$03,$C3,$C3,$3C,$00,$3C,$33,$30,$30,$30,$00,$03,$C3,$C3,$C3,$3C,$00,$30,$30,$30,$30,$0F,$00
    .byte $FF,$C0,$C0,$FF,$C0,$00,$00,$00,$00,$3F,$00,$00,$FF,$C3,$C3,$C3,$03,$00,$30,$30,$30,$30,$0F,$00
    .byte $FC,$C3,$C3,$C3,$00,$00,$0F,$30,$30,$30,$0F,$00,$03,$03,$C3,$33,$0F,$00,$00,$00,$3F,$00,$00,$00
    .byte $3C,$C3,$C3,$C3,$3C,$00,$0F,$30,$30,$30,$0F,$00,$3C,$C3,$C3,$C3,$FC,$00,$00,$30,$30,$30,$0F,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

hud_font_wide:
    .byte $FC,$FC,$03,$03,$03,$03,$03,$03,$FC,$FC,$00,$0F,$0F,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$00,$00
    .byte $0C,$0C,$FF,$FF,$00,$00,$00,$00,$00,$00,$00,$30,$30,$3F,$3F,$30,$30,$00,$00,$00,$0C,$0C,$03,$03
    .byte $C3,$C3,$C3,$C3,$3C,$3C,$00,$3C,$3C,$33,$33,$30,$30,$30,$30,$30,$30,$00,$03,$03,$C3,$C3,$C3,$C3
    .byte $C3,$C3,$3C,$3C,$00,$30,$30,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$FF,$FF,$C0,$C0,$C0,$C0,$FF,$FF
    .byte $C0,$C0,$00,$00,$00,$00,$00,$00,$00,$3F,$3F,$00,$00,$00,$FF,$FF,$C3,$C3,$C3,$C3,$C3,$C3,$03,$03
    .byte $00,$30,$30,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$FC,$FC,$C3,$C3,$C3,$C3,$C3,$C3,$00,$00,$00,$0F
    .byte $0F,$30,$30,$30,$30,$30,$30,$0F,$0F,$00,$03,$03,$03,$03,$C3,$C3,$33,$33,$0F,$0F,$00,$00,$00,$00
    .byte $00,$3F,$3F,$00,$00,$00,$00,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$3C,$3C,$00,$0F,$0F,$30,$30,$30
    .byte $30,$30,$30,$0F,$0F,$00,$3C,$3C,$C3,$C3,$C3,$C3,$C3,$C3,$FC,$FC,$00,$00,$00,$30,$30,$30,$30,$30
    .byte $30,$0F,$0F,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00
