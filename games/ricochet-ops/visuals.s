; SPDX-License-Identifier: MIT
; ricochet-ops: vector, play origin 32
.equ VIEW_X,32
.equ HUD_FIELDS,8
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB stage
    CLRA
    ADDD #1
    LDX #hud_field_0
    JSR hud_number
    LDAB rico_left
    CLRA
    LDX #hud_field_1
    JSR hud_number
    LDAB rico_ammo
    CLRA
    LDX #hud_field_2
    JSR hud_number
    LDAB rico_aim
    CLRA
    LDX #hud_field_3
    JSR hud_choice
    LDD challenge_moves
    LDX #hud_field_4
    JSR hud_number
    LDD challenge_par
    LDX #hud_field_5
    JSR hud_number
    LDAB challenge_bonus
    CLRA
    BITB #1
    BEQ hud_expr_6_one
    INCA
hud_expr_6_one:
    BITB #2
    BEQ hud_expr_6_two
    INCA
hud_expr_6_two:
    TAB
    CLRA
    LDX #hud_field_6
    JSR hud_number
    CLRB
    TST rico_active
    BEQ hud_expr_7_done
    INCB
hud_expr_7_done:
    CLRA
    LDX #hud_field_7
    JSR hud_choice
    RTS
hud_select_background:
    LDX #hud_span_table
    RTS
.section .data, data
view_origin: .byte 32
hud_field_0:
    .byte 178,0,3,0,128,0
    .word hud_cache + 0,0
hud_field_1:
    .byte 7,2,3,2,128,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 167,2,3,2,128,0
    .word hud_cache + 6,0
hud_field_3:
    .byte 3,5,5,0,128,8
    .word hud_cache + 9,hud_choices_3
hud_choices_3:
    .byte $55,$50,$20,$20,$20,$55,$50,$2D,$52,$20,$52,$49,$47,$48,$54,$44,$4E,$2D,$52,$20,$44,$4F,$57,$4E
    .byte $20,$44,$4E,$2D,$4C,$20,$4C,$45,$46,$54,$20,$55,$50,$2D,$4C,$20

hud_field_4:
    .byte 164,5,4,1,128,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 46,0,4,0,128,0
    .word hud_cache + 15,0
hud_field_6:
    .byte 88,0,1,0,128,0
    .word hud_cache + 18,0
hud_field_7:
    .byte 2,7,7,0,128,2
    .word hud_cache + 21,hud_choices_7
hud_choices_7:
    .byte $46,$49,$52,$45,$20,$20,$20,$46,$4C,$49,$47,$48,$54,$20

hud_span_table:
    .word hud_pixels_0
    .byte 0,0,64
    .word hud_pixels_1
    .byte 0,64,64
    .word hud_pixels_2
    .byte 0,128,64
    .word hud_pixels_3
    .byte 1,0,32
    .word hud_pixels_4
    .byte 1,160,32
    .word hud_pixels_5
    .byte 2,0,32
    .word hud_pixels_5
    .byte 2,160,32
    .word hud_pixels_6
    .byte 3,0,32
    .word hud_pixels_6
    .byte 3,160,32
    .word hud_pixels_7
    .byte 4,0,32
    .word hud_pixels_8
    .byte 4,160,32
    .word hud_pixels_9
    .byte 5,0,32
    .word hud_pixels_9
    .byte 5,160,32
    .word hud_pixels_10
    .byte 6,0,32
    .word hud_pixels_10
    .byte 6,160,32
    .word hud_pixels_5
    .byte 7,0,32
    .word hud_pixels_11
    .byte 7,160,32
    .word 0
    .byte 0,0,0
hud_pixels_0:
    .byte $FF,$FF,$C1,$D5,$EB,$FF,$C3,$F5,$C3,$FF,$C1,$E3,$C1,$FF,$C1,$F7,$C9,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1,$F5,$FB,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$CB,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_1:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$C1,$D5,$EB,$FF,$E3,$DD,$E3,$FF,$C1,$E3,$C1,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_2:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_3:
    .byte $00,$BD,$81,$F5,$FD,$FF,$E3,$DD,$E3,$FF,$C1,$D5,$DD,$FF,$DB,$D5,$ED,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_4:
    .byte $00,$BD,$81,$F5,$C3,$FF,$C1,$F3,$C1,$FF,$C1,$F3,$C1,$FF,$E3,$DD,$E3,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_5:
    .byte $00,$F7,$F7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_6:
    .byte $00,$DE,$DE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_7:
    .byte $00,$7B,$43,$F5,$C3,$FF,$DD,$C1,$DD,$FF,$C1,$F3,$C1,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_8:
    .byte $00,$7B,$41,$DF,$C1,$FF,$DB,$D5,$ED,$FF,$C1,$D5,$DD,$FF,$C1,$DD,$E3,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_9:
    .byte $00,$EF,$EF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_10:
    .byte $00,$BD,$BD,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_11:
    .byte $00,$F7,$F7,$C1,$F5,$CB,$FF,$C1,$D5,$DD,$FF,$FD,$C1,$FD,$FF,$C1,$DF,$C1,$FF,$C1,$F5,$CB,$FF,$C1
    .byte $E3,$C1,$FF,$FF,$FF,$FF,$FF,$00

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
