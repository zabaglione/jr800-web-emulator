; SPDX-License-Identifier: MIT
.equ STAGES,12
.equ CELLS,112
.equ TILE_STRIDE,1
.section .data, data
game_name:
    .byte $4F,$52,$42,$49,$54,$20,$47,$55,$41,$52,$44,$00
aux1_label:
    .byte $50,$55,$4C,$53,$45,$00
aux2_label:
    .byte $52,$45,$53,$45,$54,$00
grid_stat_label:
    .byte $4C,$45,$46,$54,$00
grid_action_label:
    .byte $53,$50,$41,$43,$45,$00
title_art:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$1F,$5F,$5F,$5F,$5F,$5F,$5F
    .byte $5F,$1F,$FF,$FF,$FF,$FF,$1F,$DF,$DF,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$1F,$FF,$FF,$FF,$FF,$1F,$DF
    .byte $DF,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$5F,$1F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$1F,$5F,$5F,$DF,$DF,$DF,$5F
    .byte $5F,$1F,$FF,$FF,$FF,$FF,$1F,$5F,$5F,$5F,$5F,$5F,$DF,$DF,$DF,$5F,$5F,$5F,$5F,$5F,$1F,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $7F,$BF,$DF,$CF,$EF,$F7,$F7,$FB,$FB,$FB,$FD,$FD,$FD,$FD,$FD,$FD,$FC,$FD,$FD,$FD,$FD,$FD,$FD,$FB
    .byte $FB,$FB,$F7,$F7,$EF,$CF,$DF,$BF,$7F,$FF,$FF,$FF,$EF,$EF,$EF,$E7,$00,$CF,$EF,$EF,$EF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FE,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$00,$FE,$00,$FF,$00,$FF,$C0,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$3F,$C0,$DE,$C0,$FF,$00,$FF
    .byte $C0,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$BF,$3F,$C0,$DE,$C0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$00,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FB,$FB,$FB,$F7,$F7,$F7,$EF,$EF,$EF,$DF,$DF,$DF,$BF,$BF,$3F,$1F,$67,$73,$FD,$FE
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$7F,$3F,$1F,$0F,$0F,$07,$07,$07,$07,$07,$03,$07,$07,$07,$07,$07,$0F,$0F
    .byte $1F,$3F,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FD,$F3,$E7,$9F,$7F,$FF,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$80,$BF,$80,$7F,$7F,$7F,$7F,$7F,$7F,$7F
    .byte $7F,$7F,$80,$BF,$80,$FF,$00,$FF,$01,$FE,$FE,$FE,$F1,$F7,$F1,$8E,$AE,$8E,$7F,$7F,$7F,$FF,$00,$FF
    .byte $81,$7E,$7E,$7E,$7E,$7E,$7E,$7E,$7E,$7E,$81,$BD,$81,$FF,$FF,$FF,$FF,$7F,$7F,$7F,$80,$FF,$80,$7F
    .byte $7F,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$03,$FC,$FF,$FF,$FF,$FE,$FE
    .byte $FE,$FD,$FD,$05,$01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$01,$07,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FC,$03,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$3F,$3F,$3F,$3F,$3F,$3F,$FF,$3F,$3C,$3D,$FD,$FD,$FD,$3D,$3D
    .byte $3D,$FC,$FF,$FF,$FF,$3F,$3C,$3D,$FC,$FF,$FF,$FF,$3F,$3F,$3F,$3F,$3F,$3F,$FC,$FD,$FC,$FF,$3C,$3D
    .byte $3D,$3D,$3D,$3D,$FD,$FD,$FD,$FD,$FD,$FC,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FC,$FD,$FD,$FD,$FD,$FD,$FD
    .byte $FD,$FC,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FC,$FD,$FC,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$80,$7F,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FE,$C0,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$C0,$FA,$F7,$F7,$F7,$EF,$EF,$EF,$DF,$5F,$80,$BE,$BF,$BF,$7F,$7F,$7F,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$01,$01,$01,$FE,$FE,$FE,$0E,$0E,$0E,$FF,$00,$00,$00,$FF,$FF,$FF,$00,$00
    .byte $00,$FF,$01,$01,$01,$8E,$8E,$8E,$01,$01,$01,$FF,$00,$00,$00,$8E,$8E,$8E,$71,$71,$71,$FF,$00,$00
    .byte $00,$FE,$FE,$FE,$01,$01,$01,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FC,$F3,$CF,$9F,$7F,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FE,$FC,$F8,$F0,$E0,$E0,$C0,$C0,$C0,$C0,$C0,$80,$C0,$C0,$C0,$C0,$C0,$E0,$E0
    .byte $F0,$F8,$FC,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$9F,$CF,$F3,$FC,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FE
    .byte $FE,$FD,$FD,$FD,$FB,$FF,$FC,$FC,$FC,$E3,$E3,$E3,$E0,$E0,$E0,$FF,$E0,$E0,$E0,$E3,$E3,$E3,$E0,$E0
    .byte $E0,$FF,$E0,$E0,$E0,$FF,$FF,$FF,$E0,$E0,$E0,$FF,$E0,$E0,$E0,$FF,$FF,$FF,$E0,$E0,$E0,$FF,$E0,$E0
    .byte $E0,$E3,$E3,$E3,$FC,$FC,$FC,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE
    .byte $FD,$FB,$F7,$E7,$EF,$DF,$DF,$BF,$BF,$BF,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$BF
    .byte $BF,$BF,$DF,$DF,$EF,$E7,$F7,$FB,$FD,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$ED,$EA,$F6,$FF,$E0,$FA,$FD,$FF
    .byte $E1,$FA,$E1,$FF,$F1,$EE,$EE,$FF,$E0,$EA,$EE,$FF,$FF,$FF,$FF,$FF,$FE,$E0,$FE,$FF,$F1,$EE,$F1,$FF
    .byte $FF,$FF,$FF,$FF,$ED,$EA,$F6,$FF,$FE,$E0,$FE,$FF,$E1,$FA,$E1,$FF,$E0,$FA,$E5,$FF,$FE,$E0,$FE,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
tiles:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$C0,$20,$10,$10,$00
    .byte $10,$10,$00,$10,$10,$00,$10,$10,$00,$10,$10,$00,$10,$10,$00,$10,$10,$00,$10,$10,$00,$10,$10,$00
    .byte $10,$10,$00,$10,$10,$40,$00,$00,$80,$50,$0C,$00,$00,$00,$00,$00,$10,$00,$10,$10,$00,$20,$80,$00
    .byte $00,$00,$00,$00,$00,$01,$0C,$30,$80,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$A0,$18
    .byte $01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$40,$30,$08,$01,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$10,$10,$00,$00,$10,$10,$00,$10,$00,$40,$80,$00,$00,$00,$00,$00,$00,$00,$02
    .byte $08,$20,$80,$00,$00,$00,$00,$00,$02,$18,$60,$00,$00,$00,$00,$00,$00,$00,$00,$00,$30,$C3,$00,$00
    .byte $00,$00,$00,$00,$10,$4C,$02,$00,$00,$00,$00,$00,$00,$20,$40,$00,$00,$82,$44,$20,$10,$00,$00,$00
    .byte $00,$00,$80,$42,$08,$00,$00,$00,$00,$00,$00,$45,$30,$00,$00,$00,$00,$00,$00,$00,$00,$00,$0A,$60
    .byte $00,$00,$00,$00,$00,$00,$01,$04,$10,$40,$00,$00,$00,$00,$00,$00,$01,$02,$00,$08,$10,$10,$00,$10
    .byte $10,$10,$00,$10,$10,$08,$04,$02,$00,$00,$00,$00,$00,$00,$80,$60,$10,$02,$01,$00,$00,$00,$00,$00
    .byte $80,$60,$06,$01,$00,$00,$00,$00,$01,$14,$C0,$00,$00,$00,$00,$00,$00,$00,$01,$04,$10,$00,$10,$10
    .byte $10,$10,$00,$10,$18,$04,$00,$00,$00,$00,$00,$00,$00,$00,$C0,$0C,$02,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$03,$08,$10,$10,$00,$10,$10,$00,$10,$18,$05,$00,$00,$00,$7E,$7E,$7E,$7E,$7E,$7E,$00
    .byte $00,$7E,$66,$5A,$5A,$66,$7E,$00,$00,$7E,$42,$5E,$42,$42,$7E,$00,$08,$08,$1C,$FF,$1C,$08,$08,$08
    .byte $FF,$FF,$C3,$C3,$C1,$C3,$FF,$FF,$FF,$FF,$C3,$C3,$C3,$C3,$FB,$FD,$FF,$FF,$C3,$C3,$C3,$C3,$EF,$EF
    .byte $FF,$FF,$C3,$C3,$C3,$C3,$BF,$7F,$FF,$FF,$C3,$C3,$03,$C3,$FF,$FF,$FF,$7F,$83,$C3,$C3,$C3,$FF,$FF
    .byte $FF,$EF,$C3,$C3,$C3,$C3,$FF,$FF,$FF,$FD,$C3,$C3,$C3,$C3,$FF,$FF
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
.equ ORBIT_WARNING,42
.equ ORBIT_ENEMY,40
.equ ORBIT_BEAM,43
.equ ORBIT_PLAYER,44
orbit_x:
    .byte $38,$40,$48,$40,$38,$30,$28,$30,$38,$50,$58,$50,$38,$20,$18,$20,$38,$60,$68,$60,$38,$10,$08,$10
orbit_y:
    .byte $18,$18,$20,$28,$28,$28,$20,$18,$10,$10,$20,$30,$30,$30,$20,$10,$08,$08,$20,$38,$38,$38,$20,$08
orbit_background:
    .byte $00,$00,$01,$02,$03,$04,$02,$03,$04,$02,$03,$04,$05,$00,$00,$00,$00,$00,$06,$00,$01,$02,$03,$04
    .byte $02,$03,$07,$00,$08,$09,$00,$00,$00,$0A,$0B,$0C,$0D,$00,$0E,$02,$0F,$00,$10,$11,$00,$12,$00,$00
    .byte $00,$13,$00,$14,$00,$15,$00,$00,$00,$16,$00,$17,$00,$18,$00,$00,$00,$19,$09,$1A,$1B,$00,$1C,$04
    .byte $1D,$0B,$1E,$1F,$00,$20,$00,$00,$00,$00,$21,$00,$22,$03,$04,$02,$03,$04,$23,$00,$24,$25,$00,$00
    .byte $00,$00,$26,$02,$03,$04,$02,$03,$04,$02,$03,$04,$27,$00,$00,$00
orbit_slots:
    .byte $FF,$FF,$17,$FF,$FF,$FF,$FF,$10,$FF,$FF,$FF,$FF,$11,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$0F,$FF,$FF,$08
    .byte $FF,$FF,$09,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$07,$00,$01,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$16,$FF,$0E,$FF,$06,$FF,$FF,$FF,$02,$FF,$0A,$FF,$12,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$05,$04
    .byte $03,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$0D,$FF,$FF,$0C,$FF,$FF,$0B,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$15,$FF,$FF,$FF,$FF,$14,$FF,$FF,$FF,$FF,$13,$FF,$FF,$FF
orbit_levels:
    .byte $04,$02,$06,$01,$02,$03,$01,$00,$03,$07,$03,$04,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $00,$03,$02,$05,$01,$03,$06,$00,$06,$05,$01,$06,$01,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $01,$00,$04,$01,$06,$01,$07,$06,$01,$00,$05,$04,$01,$02,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $06,$07,$04,$06,$05,$04,$02,$01,$03,$04,$03,$02,$04,$00,$03,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $0B,$07,$03,$0A,$06,$00,$0C,$07,$04,$08,$06,$04,$0D,$03,$06,$0B,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $08,$05,$03,$08,$06,$01,$08,$07,$04,$09,$00,$06,$0F,$01,$00,$0D,$04,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $0F,$06,$01,$08,$01,$04,$08,$05,$02,$0D,$07,$00,$0B,$05,$00,$0D,$01,$02,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $0E,$03,$06,$0F,$01,$00,$0A,$04,$05,$0F,$00,$07,$0C,$05,$06,$0A,$07,$02,$0D,$FF,$FF,$FF,$FF,$FF
    .byte $08,$03,$02,$0E,$04,$01,$0D,$03,$07,$0E,$04,$00,$0F,$06,$03,$0C,$06,$02,$0F,$04,$FF,$FF,$FF,$FF
    .byte $0A,$01,$04,$0B,$06,$04,$0A,$05,$00,$0F,$05,$02,$09,$06,$02,$0C,$01,$05,$08,$06,$03,$FF,$FF,$FF
    .byte $0D,$00,$05,$0E,$01,$03,$08,$05,$02,$09,$04,$03,$0F,$05,$06,$0B,$02,$07,$0D,$02,$06,$0B,$FF,$FF
    .byte $0F,$06,$05,$0F,$02,$00,$0C,$03,$04,$0D,$06,$00,$0C,$06,$02,$0E,$00,$07,$08,$01,$04,$0D,$02,$FF
