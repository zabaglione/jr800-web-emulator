; SPDX-License-Identifier: MIT
; number-rail: ticker, play origin 64
.equ VIEW_X,64
.equ HUD_FIELDS,6
.section .text, code
visual_hud:
    JSR hud_begin
    LDAB stage
    CLRA
    ADDD #1
    LDX #hud_field_0
    JSR hud_number
    LDAB grid_stat
    ASLB
    LDX #rail_values
    ABX
    LDD 0,X
    LDX #hud_field_1
    JSR hud_number
    LDAB stage
    LDX #rail_goals
    ABX
    LDAB 0,X
    CLRA
    ASLB
    LDX #rail_values
    ABX
    LDD 0,X
    LDX #hud_field_2
    JSR hud_number
    LDD challenge_moves
    LDX #hud_field_3
    JSR hud_number
    LDD challenge_par
    LDX #hud_field_4
    JSR hud_number
    LDD #0
    LDX #hud_field_5
    JSR hud_choice
    RTS
hud_select_background:
    LDX #hud_span_table
    RTS
.section .data, data
view_origin: .byte 64
hud_field_0:
    .byte 48,0,3,0,128,0
    .word hud_cache + 0,0
hud_field_1:
    .byte 10,2,4,3,128,0
    .word hud_cache + 3,0
hud_field_2:
    .byte 43,4,4,0,128,0
    .word hud_cache + 6,0
hud_field_3:
    .byte 43,5,4,0,128,0
    .word hud_cache + 9,0
hud_field_4:
    .byte 43,6,4,0,128,0
    .word hud_cache + 12,0
hud_field_5:
    .byte 4,7,14,0,128,1
    .word hud_cache + 15,hud_choices_5
hud_choices_5:
    .byte $53,$4C,$49,$44,$45,$20,$54,$4F,$20,$4D,$45,$52,$47,$45

hud_span_table:
    .word hud_pixels_0
    .byte 0,0,64
    .word hud_pixels_1
    .byte 0,64,64
    .word hud_pixels_1
    .byte 0,128,64
    .word hud_pixels_2
    .byte 1,0,64
    .word hud_pixels_3
    .byte 2,0,64
    .word hud_pixels_3
    .byte 3,0,64
    .word hud_pixels_4
    .byte 4,0,64
    .word hud_pixels_5
    .byte 5,0,64
    .word hud_pixels_6
    .byte 6,0,64
    .word hud_pixels_3
    .byte 7,0,64
    .word 0
    .byte 0,0,0
hud_pixels_0:
    .byte $FF,$FF,$C1,$F3,$C1,$FF,$C1,$D5,$DD,$FF,$C1,$F5,$CB,$FF,$E3,$DD,$C5,$FF,$C1,$D5,$DD,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_1:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

hud_pixels_2:
    .byte $00,$FF,$FF,$FF,$FF,$FD,$C1,$FD,$FF,$E3,$DD,$E3,$FF,$C1,$F5,$FB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_3:
    .byte $00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_4:
    .byte $00,$FF,$FF,$FF,$FF,$E3,$DD,$C5,$FF,$E3,$DD,$E3,$FF,$C3,$F5,$C3,$FF,$C1,$DF,$DF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_5:
    .byte $00,$FF,$FF,$FF,$FF,$C1,$DF,$C1,$FF,$DB,$D5,$ED,$FF,$C1,$D5,$DD,$FF,$C1,$DD,$E3,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00

hud_pixels_6:
    .byte $00,$FF,$FF,$FF,$FF,$C1,$F5,$FB,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$CB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
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
