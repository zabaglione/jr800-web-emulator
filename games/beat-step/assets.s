; SPDX-License-Identifier: MIT
.equ STAGES,3
.equ CELLS,112
.equ TILE_STRIDE,1
.section .data, data
game_name:
    .byte $42,$45,$41,$54,$20,$53,$54,$45,$50,$00
aux1_label:
    .byte $53,$4F,$55,$4E,$44,$00
aux2_label:
    .byte $52,$45,$53,$45,$54,$00
grid_stat_label:
    .byte $4C,$45,$46,$54,$00
grid_action_label:
    .byte $53,$50,$41,$43,$45,$00
title_art:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$1F,$DF,$DF,$DF,$5F,$5F,$5F,$5F
    .byte $5F,$5F,$5F,$5F,$5F,$5F,$5F,$1F,$FF,$FF,$FF,$FF,$FF,$1F,$DF,$DF,$DF,$5F,$5F,$5F,$5F,$5F,$5F,$5F
    .byte $5F,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$1F,$FF,$FF,$FF,$FF,$FF,$1F,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$5F
    .byte $5F,$1F,$FF,$FF,$FF,$FF,$FF,$1F,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$DF,$DF,$DF,$DF,$5F,$5F,$5F,$5F,$5F
    .byte $5F,$5F,$1F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$7F,$7F,$7F,$7F,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$C0,$BF,$BF,$BF,$BF
    .byte $BF,$BF,$BF,$BF,$BF,$BF,$BF,$3F,$C0,$DE,$DE,$C0,$FF,$00,$FF,$FF,$C0,$BF,$BF,$BF,$BF,$BF,$BF,$BF
    .byte $BF,$BF,$BF,$BF,$3F,$FF,$FF,$FF,$FF,$FF,$00,$FE,$FE,$C0,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$BF
    .byte $BF,$BF,$C0,$FE,$FE,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$33,$33,$33,$33,$33,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$81,$7E,$7E,$7E,$7E
    .byte $7E,$7E,$7E,$7E,$7E,$7E,$7E,$7E,$81,$BD,$BD,$81,$FF,$00,$FF,$FF,$81,$7E,$7E,$7E,$7E,$7E,$7E,$7E
    .byte $7E,$7E,$7E,$7E,$7E,$7F,$7F,$7F,$7F,$FF,$00,$FF,$FF,$01,$FE,$FE,$FE,$FE,$FE,$FE,$FE,$FE,$FE,$FE
    .byte $FE,$FE,$01,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$67,$67,$67,$67,$67,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$7F,$7F,$7F,$7F,$7F,$FF,$FF,$FF,$33,$33,$33,$33,$33,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FC,$FD,$FD,$FD,$FD,$FD,$FD,$FD
    .byte $FD,$FD,$FD,$FD,$FD,$FD,$FD,$FC,$7F,$3F,$3F,$3F,$3F,$3C,$3D,$3D,$3D,$3D,$3D,$3D,$3D,$3D,$3D,$3D
    .byte $BD,$7D,$3D,$3D,$3D,$3D,$3D,$3D,$3C,$3F,$3C,$3D,$3D,$3C,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$BF,$7F,$3F
    .byte $3F,$3F,$3C,$3D,$3D,$3C,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$3C,$3D,$3D,$BC,$7F,$3F,$3F,$3F,$3F
    .byte $3F,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$3F,$BF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$33,$33,$33
    .byte $33,$33,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$33,$33,$33,$33,$33,$FF,$FF,$FF,$33,$33,$33,$33,$33
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$8F,$01,$01,$01,$01,$46,$78,$78,$78,$78,$78,$78,$78,$78,$78,$78,$F8,$F8,$F8,$F8,$F8
    .byte $F8,$FE,$F8,$F8,$F8,$F8,$F8,$F8,$08,$00,$00,$00,$00,$C0,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$08,$00,$00
    .byte $00,$00,$40,$78,$78,$78,$78,$78,$78,$78,$78,$78,$78,$F8,$F8,$F8,$F8,$F8,$08,$00,$00,$00,$00,$40
    .byte $78,$78,$78,$78,$78,$78,$78,$78,$78,$78,$88,$00,$00,$01,$01,$C7,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$33,$33,$33
    .byte $33,$33,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$33,$33,$33,$33,$33,$FF,$FF,$FF,$33,$33,$33,$33,$33
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FE,$FE,$FC,$F4,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$30,$00,$01,$03,$03,$0F,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$3F,$01,$00,$00,$00,$00,$F8,$FF,$FF,$FF,$FF,$FF,$FF,$3F,$01,$00,$00,$00
    .byte $00,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F9,$FF,$FF,$3F,$01,$00,$00,$00,$00,$F0,$F0
    .byte $F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0,$F9,$FE,$FE,$FE,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$33,$33,$33
    .byte $33,$33,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$66,$66,$66,$66,$66,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$E6,$E6,$E6,$E6,$E6,$FF,$FF,$FF,$F3,$F3,$F3,$F3,$F3,$FF,$FF,$FF,$F3,$F3,$F3,$F3,$F3
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$DF,$CF,$C7,$03,$01,$00,$01,$03,$C7,$CF,$DF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$F8,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E3,$FC,$FC,$FC,$FC,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$F8,$E0,$E0,$E0,$E0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F8,$E0,$E0,$E0,$E0
    .byte $E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E0,$E3,$F8,$E0,$E0,$E0,$E0,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$DF,$CF,$C7,$03,$01,$00,$01,$03,$C7,$CF,$DF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F3,$F3,$F3
    .byte $F3,$F3,$FF,$FF,$FF,$E6,$E6,$E6,$E6,$E6,$FF,$FF,$FF,$E6,$E6,$E6,$E6,$E6,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F8,$F8,$F8,$F8,$F8,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$B7,$AB,$DB,$FF,$83,$EB,$F7,$FF,$87,$EB,$87,$FF,$C7,$BB,$BB,$FF,$83,$AB,$BB,$FF
    .byte $FF,$FF,$FF,$FF,$FB,$83,$FB,$FF,$C7,$BB,$C7,$FF,$FF,$FF,$FF,$FF,$B7,$AB,$DB,$FF,$FB,$83,$FB,$FF
    .byte $87,$EB,$87,$FF,$83,$EB,$97,$FF,$FB,$83,$FB,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$F8,$F8,$F8,$F8,$F8,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
tiles:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$08,$00,$00,$00,$00,$FF,$00,$00,$08,$00,$00,$00,$00
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
beat_charts:
    .byte $28,$08,$20,$00,$01,$02,$03,$02,$01,$00,$03,$00,$01,$02,$03,$02,$01,$00,$03,$00,$01,$02,$03,$02
    .byte $01,$00,$03,$00,$01,$02,$03,$02,$01,$00,$03,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$1E,$06,$30,$00,$02
    .byte $01,$03,$00,$03,$01,$02,$02,$00,$03,$01,$00,$02,$01,$03,$00,$03,$01,$02,$02,$00,$03,$01,$00,$02
    .byte $01,$03,$00,$03,$01,$02,$02,$00,$03,$01,$00,$02,$01,$03,$00,$03,$01,$02,$02,$00,$03,$01,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$18,$05,$40,$00,$03,$01,$02,$03,$00,$02
    .byte $01,$00,$02,$03,$01,$03,$01,$00,$02,$00,$03,$01,$02,$03,$00,$02,$01,$00,$02,$03,$01,$03,$01,$00
    .byte $02,$00,$03,$01,$02,$03,$00,$02,$01,$00,$02,$03,$01,$03,$01,$00,$02,$00,$03,$01,$02,$03,$00,$02
    .byte $01,$00,$02,$03,$01,$03,$01,$00,$02
beat_arrows:
    .byte $08,$1C,$3E,$7F,$1C,$1C,$1C,$00,$08,$18,$3F,$7F,$3F,$18,$08,$00,$08,$0C,$7E,$7F,$7E,$0C,$08,$00
    .byte $1C,$1C,$1C,$7F,$3E,$1C,$08,$00
