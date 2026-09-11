; SPDX-License-Identifier: MIT
.equ STAGES,12
.equ CELLS,112
.equ TILE_STRIDE,1
.section .data, data
game_name:
    .byte $53,$54,$41,$52,$20,$50,$41,$54,$52,$4F,$4C,$00
aux1_label:
    .byte $50,$41,$55,$53,$45,$00
aux2_label:
    .byte $52,$45,$53,$45,$54,$00
grid_stat_label:
    .byte $4C,$45,$46,$54,$00
grid_action_label:
    .byte $53,$50,$41,$43,$45,$00
title_art:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$EF,$FF,$FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$7F
    .byte $7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$FF,$7F,$7F,$5F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F
    .byte $7F,$7F,$FF,$FF,$FF,$FF,$7F,$7F,$7F,$7D,$7F,$7F,$7F,$7F,$7F,$FF,$FF,$FF,$FF,$7F,$7F,$7F,$7F,$7F
    .byte $7F,$7F,$7F,$7F,$7F,$7F,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $BF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$7F,$1F,$07,$01,$00,$01,$07,$1F,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$EF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FD,$FF,$03,$7B,$03,$FC,$FD
    .byte $FD,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FC,$FF,$FC,$FD,$FD,$FD,$FD,$FD,$03,$FF,$03,$FD,$FD,$FD,$FD
    .byte $FD,$FC,$FF,$03,$FB,$03,$FC,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FC,$03,$FB,$03,$FF,$00,$FF,$03,$FD,$FD
    .byte $FD,$FD,$FD,$FD,$F9,$FD,$FC,$03,$7B,$03,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $3F,$07,$01,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$01,$07,$3F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$DF,$F8,$FA
    .byte $FA,$FA,$FA,$FA,$FA,$FA,$F8,$07,$F7,$07,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$00,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$00,$FF,$07,$FA,$FA,$FA,$FA,$FA,$FA,$FA,$FA,$FA,$07,$FF,$00,$FF,$00,$FF,$07,$FA,$FA
    .byte $FA,$C6,$DE,$C6,$3A,$BA,$38,$FF,$BF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$3F,$0F,$03,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$00,$00,$00,$00,$03,$0F,$3F,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F1,$F5,$F5,$F5,$F5
    .byte $F5,$F5,$F5,$F5,$F5,$F5,$F1,$FE,$FE,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F0,$F7,$F0,$FF,$DF,$FF,$FF
    .byte $FF,$FF,$FF,$F0,$F7,$F0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F0,$F7,$F0,$FF,$F0,$F7,$F0,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FE,$FE,$FE,$F1,$F5,$F1,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$BF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F7
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FF,$7F,$1F,$07,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$80,$9F,$80,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$07,$1F,$7F,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$EF,$00,$00,$00,$38,$38,$38,$C7,$C7,$C7,$FF,$07,$07,$07
    .byte $38,$38,$38,$07,$07,$07,$FF,$F8,$F8,$F8,$00,$00,$00,$F8,$F8,$F8,$FF,$00,$00,$00,$38,$38,$38,$C7
    .byte $C7,$C7,$FF,$07,$07,$07,$F8,$F8,$F8,$07,$07,$07,$FF,$00,$00,$00,$BF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F7
    .byte $FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$9F,$87,$C1,$C0,$E0,$E0,$F0,$F0,$F8,$F8,$F8
    .byte $FC,$FC,$FE,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FE,$FC,$FC,$F8,$F8,$F8,$F0,$F0,$E0,$E0,$C0,$C1
    .byte $87,$9F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$80,$80,$80,$FE,$FE,$FE,$FF,$FF,$FF,$FF,$80,$80,$80
    .byte $FE,$FE,$FE,$80,$80,$80,$FF,$FF,$FF,$FF,$80,$80,$80,$FF,$FF,$FF,$FF,$80,$80,$80,$FE,$FE,$FE,$81
    .byte $81,$81,$FF,$F0,$F0,$F0,$8F,$8F,$8F,$F0,$F0,$F0,$FF,$80,$80,$80,$8F,$8F,$8F,$8F,$8F,$8F,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$BF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$EF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$DF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FB,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$F7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$F7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$DF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FD,$FF,$FF,$DB,$D5,$ED,$FF,$C1,$F5,$FB,$FF,$C3,$F5,$C3,$FF
    .byte $E3,$DD,$DD,$FF,$C1,$D5,$DD,$FF,$FF,$FF,$FF,$FF,$FD,$C1,$FD,$FF,$E3,$DD,$E3,$FF,$FF,$FF,$FF,$FF
    .byte $9B,$D5,$ED,$FF,$FD,$C1,$FD,$FF,$C3,$F5,$C3,$FF,$C1,$F5,$CB,$FF,$FD,$C1,$FD,$FF,$FB,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
tiles:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$60,$1C,$56,$1E,$1E,$56,$1C,$60
    .byte $7E,$1D,$57,$1F,$1F,$57,$1D,$7E,$38,$3C,$3C,$30,$30,$3C,$1C,$38,$38,$3C,$3C,$3C,$3C,$3C,$3C,$38
    .byte $00,$00,$00,$7E,$7E,$00,$00,$00,$00,$00,$11,$2A,$44,$00,$00,$00,$BF,$8F,$8F,$80,$80,$8F,$8F,$BF
view_cells:
    .byte $00,$01,$02,$03,$04,$05,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E,$0F,$10,$11,$12,$13,$14,$15,$16,$17
    .byte $18,$19,$1A,$1B,$1C,$1D,$1E,$1F,$20,$21,$22,$23,$24,$25,$26,$27,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F
    .byte $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$3A,$3B,$3C,$3D,$3E,$3F,$40,$41,$42,$43,$44,$45,$46,$47
    .byte $48,$49,$4A,$4B,$4C,$4D,$4E,$4F,$50,$51,$52,$53,$54,$55,$56,$57,$58,$59,$5A,$5B,$5C,$5D,$5E,$5F
    .byte $60,$61,$62,$63,$64,$65,$66,$67,$68,$69,$6A,$6B,$6C,$6D,$6E,$6F
view_subtiles:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
neighbors:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$01,$02,$03,$04,$05,$06,$07
    .byte $08,$09,$0A,$0B,$0C,$0D,$0E,$0F,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$1A,$1B,$1C,$1D,$1E,$1F
    .byte $20,$21,$22,$23,$24,$25,$26,$27,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F,$30,$31,$32,$33,$34,$35,$36,$37
    .byte $38,$39,$3A,$3B,$3C,$3D,$3E,$3F,$40,$41,$42,$43,$44,$45,$46,$47,$48,$49,$4A,$4B,$4C,$4D,$4E,$4F
    .byte $50,$51,$52,$53,$54,$55,$56,$57,$58,$59,$5A,$5B,$5C,$5D,$5E,$5F,$10,$11,$12,$13,$14,$15,$16,$17
    .byte $18,$19,$1A,$1B,$1C,$1D,$1E,$1F,$20,$21,$22,$23,$24,$25,$26,$27,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F
    .byte $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$3A,$3B,$3C,$3D,$3E,$3F,$40,$41,$42,$43,$44,$45,$46,$47
    .byte $48,$49,$4A,$4B,$4C,$4D,$4E,$4F,$50,$51,$52,$53,$54,$55,$56,$57,$58,$59,$5A,$5B,$5C,$5D,$5E,$5F
    .byte $60,$61,$62,$63,$64,$65,$66,$67,$68,$69,$6A,$6B,$6C,$6D,$6E,$6F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$01,$02,$03,$04,$05,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E
    .byte $FF,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$1A,$1B,$1C,$1D,$1E,$FF,$20,$21,$22,$23,$24,$25,$26
    .byte $27,$28,$29,$2A,$2B,$2C,$2D,$2E,$FF,$30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$3A,$3B,$3C,$3D,$3E
    .byte $FF,$40,$41,$42,$43,$44,$45,$46,$47,$48,$49,$4A,$4B,$4C,$4D,$4E,$FF,$50,$51,$52,$53,$54,$55,$56
    .byte $57,$58,$59,$5A,$5B,$5C,$5D,$5E,$FF,$60,$61,$62,$63,$64,$65,$66,$67,$68,$69,$6A,$6B,$6C,$6D,$6E
    .byte $01,$02,$03,$04,$05,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E,$0F,$FF,$11,$12,$13,$14,$15,$16,$17,$18
    .byte $19,$1A,$1B,$1C,$1D,$1E,$1F,$FF,$21,$22,$23,$24,$25,$26,$27,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F,$FF
    .byte $31,$32,$33,$34,$35,$36,$37,$38,$39,$3A,$3B,$3C,$3D,$3E,$3F,$FF,$41,$42,$43,$44,$45,$46,$47,$48
    .byte $49,$4A,$4B,$4C,$4D,$4E,$4F,$FF,$51,$52,$53,$54,$55,$56,$57,$58,$59,$5A,$5B,$5C,$5D,$5E,$5F,$FF
    .byte $61,$62,$63,$64,$65,$66,$67,$68,$69,$6A,$6B,$6C,$6D,$6E,$6F,$FF
star_levels:
    .byte $01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01
    .byte $01,$01,$00,$01,$01,$01,$01,$01,$01,$00,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$00,$01,$01
    .byte $01,$01,$01,$01,$00,$01,$01,$01,$01,$01,$01,$00,$01,$01,$01,$01,$00,$01,$01,$01,$01,$01,$01,$00
    .byte $01,$01,$01,$01,$01,$01,$00,$01,$02,$01,$01,$01,$02,$01,$01,$01,$02,$01,$01,$01,$02,$01,$01,$01
    .byte $02,$01,$01,$01,$01,$02,$00,$01,$01,$02,$01,$01,$01,$00,$01,$01,$01,$02,$01,$01,$00,$02,$01,$01
    .byte $01,$00,$02,$01,$01,$01,$02,$01,$00,$01,$02,$01,$01,$01,$02,$00,$01,$01,$02,$01,$00,$01,$01,$02
    .byte $01,$01,$01,$00,$01,$01,$01,$02,$01,$01,$00,$02,$01,$01,$01,$02,$02,$01,$01,$01,$02,$01,$01,$01
    .byte $02,$01,$01,$01,$02,$01,$01,$01,$02,$01,$01,$01,$01,$02,$01,$01,$01,$00,$01,$01,$01,$02,$01,$01
    .byte $00,$02,$01,$01,$01,$02,$01,$00,$01,$01,$02,$01,$00,$01,$02,$01,$01,$01,$02,$00,$01,$01,$02,$01
    .byte $01,$01,$00,$01,$01,$01,$01,$00,$01,$01,$01,$02,$01,$01,$00,$02,$01,$01,$01,$02,$01,$00,$01,$02
