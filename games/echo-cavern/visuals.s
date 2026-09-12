; SPDX-License-Identifier: MIT
; echo-cavern: sonar, play origin 0
.equ VIEW_X,0
.equ HUD_FIELDS,7
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB stage
    CLRA
    ADDD #1
    LDX #hud_field_0
    JSR hud_number
    LDAB echo_air
    CLRA
    LDX #hud_field_1
    JSR hud_number
    LDAB echo_air
    CLRA
    LDX #hud_field_2
    JSR hud_number
    LDAB echo_left
    CLRA
    LDX #hud_field_3
    JSR hud_number
    LDAB echo_mapped
    CLRA
    LDX #hud_field_4
    JSR hud_number
    LDD echo_score
    LDX #hud_field_5
    JSR hud_number
    LDD #0
    TST echo_left
    BNE hud_expr_6_done
    INCB
hud_expr_6_done:
    LDX #hud_field_6
    JSR hud_choice
    RTS
hud_select_background:
    LDX #hud_span_table
    RTS
.section .data, data
view_origin: .byte 0
hud_field_0:
    .byte 176,0,3,0,128,0
    .word hud_cache + 0,0
hud_field_1:
    .byte 151,2,3,1,128,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 133,3,54,4,128,99
    .word hud_cache + 6,0
hud_field_3:
    .byte 175,4,3,0,128,0
    .word hud_cache + 9,0
hud_field_4:
    .byte 175,5,3,0,128,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 171,6,4,0,128,0
    .word hud_cache + 15,0
hud_field_6:
    .byte 142,7,11,0,128,2
    .word hud_cache + 18,hud_choices_6
hud_choices_6:
    .byte $45,$58,$49,$54,$20,$4C,$4F,$43,$4B,$45,$44,$45,$58,$49,$54,$20,$4F,$50,$45,$4E,$20,$20

hud_span_table:
    .word hud_pixels_0
    .byte 0,0,64
    .word hud_pixels_0
    .byte 0,64,64
    .word hud_pixels_1
    .byte 0,128,64
    .word hud_pixels_2
    .byte 1,128,64
    .word hud_pixels_3
    .byte 2,128,64
    .word hud_pixels_4
    .byte 3,128,64
    .word hud_pixels_5
    .byte 4,128,64
    .word hud_pixels_6
    .byte 5,128,64
    .word hud_pixels_7
    .byte 6,128,64
    .word hud_pixels_8
    .byte 7,128,64
    .word 0
    .byte 0,0,0
hud_pixels_0:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_1:
    .byte $FF,$FF,$DB,$D5,$ED,$FF,$E3,$DD,$E3,$FF,$C1,$E3,$C1,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$CB,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_2:
    .byte $00,$BD,$BD,$FF,$FF,$C3,$F5,$C3,$FF,$DD,$C1,$DD,$FF,$C1,$F5,$CB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_3:
    .byte $00,$F7,$F7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_4:
    .byte $00,$DE,$DE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_5:
    .byte $00,$7B,$7B,$FF,$FF,$E3,$DD,$C5,$FF,$C1,$D5,$DD,$FF,$C1,$F3,$C1,$FF,$DB,$D5,$ED,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_6:
    .byte $00,$EF,$EF,$FF,$FF,$C1,$F3,$C1,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$FB,$FF,$C1,$F5,$FB,$FF,$C1,$D5,$DD
    .byte $FF,$C1,$DD,$E3,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_7:
    .byte $00,$BD,$BD,$FF,$FF,$DB,$D5,$ED,$FF,$E3,$DD,$DD,$FF,$E3,$DD,$E3,$FF,$C1,$F5,$CB,$FF,$C1,$D5,$DD
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_8:
    .byte $00,$F7,$F7,$FF,$FF,$01,$00,$7C,$6C,$10,$01,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

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
