; SPDX-License-Identifier: MIT
.equ STAGES,1
.equ CELLS,36
.equ TILE_STRIDE,2
.section .data, data
game_name:
    .byte $48,$45,$58,$20,$46,$52,$4F,$4E,$54,$00
aux1_label:
    .byte $52,$45,$4C,$41,$59,$00
aux2_label:
    .byte $55,$4E,$44,$4F,$20,$54,$55,$52,$4E,$00
grid_stat_label:
    .byte $4E,$45,$45,$44,$00
grid_action_label:
    .byte $53,$50,$41,$43,$45,$00
title_art:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$BF,$BF,$DF,$DF,$EF,$DF,$DF,$BF,$7F,$7F,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$1F,$1F,$1F,$1F,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$1F,$1F,$1F,$1F,$FF,$1F,$1F,$1F,$1F,$1F,$1F,$1F,$1F,$1F,$1F,$1F,$1F,$1F
    .byte $1F,$1F,$1F,$1F,$1F,$1F,$1F,$FF,$1F,$1F,$1F,$1F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $1F,$1F,$1F,$1F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$7F,$7F,$BF,$BF,$DF,$BF,$BF,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$01,$FE,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FE,$01
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$40,$40,$40,$40,$7F,$7F,$7F,$7F,$7F,$7F
    .byte $7F,$7F,$7F,$7F,$7F,$7F,$40,$40,$40,$40,$FF,$40,$40,$40,$40,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F
    .byte $7F,$7F,$7F,$FF,$FF,$FF,$FF,$FF,$F8,$F8,$F8,$F8,$C7,$C7,$C7,$C7,$7F,$7F,$7F,$7F,$C7,$C7,$C7,$C7
    .byte $F8,$F8,$F8,$F8,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$03,$FD,$FD,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FE,$FD,$FD,$03,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$F0,$EF,$EF,$DF,$DF,$BF,$7F,$7F,$FF,$7F,$7F,$BF,$BF,$DF,$EF,$EF,$F0
    .byte $FF,$7F,$BF,$BF,$DF,$EF,$EF,$F7,$F7,$FB,$F7,$F7,$EF,$DF,$DF,$BF,$BF,$7F,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$00,$00,$00,$FE,$FE,$FE,$FE,$FE,$FE
    .byte $FE,$FE,$FE,$FE,$FE,$FE,$00,$00,$00,$00,$FF,$00,$00,$00,$00,$7E,$7E,$7E,$7E,$7E,$7E,$7E,$7E,$7E
    .byte $7E,$7E,$7E,$7F,$7F,$7F,$7F,$FF,$0F,$0F,$0F,$0F,$F1,$F1,$F1,$F1,$FE,$FE,$FE,$FE,$F1,$F1,$F1,$F1
    .byte $0F,$0F,$0F,$0F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$7F,$BF,$BF,$DF,$EF,$EF,$F7,$F7,$FB,$F7,$F7,$EF,$DF,$DF,$BF,$BF,$7F
    .byte $FF,$E0,$DF,$DF,$BF,$BF,$7F,$FF,$FF,$FF,$FF,$FF,$7F,$7F,$BF,$DF,$DF,$E0,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7C,$FC,$7C,$7C,$7F,$7F,$7F,$7F,$7F,$7F
    .byte $7F,$7F,$7F,$7F,$FF,$FF,$FC,$FC,$FC,$FC,$FF,$7C,$7C,$7C,$7C,$7C,$7C,$7C,$7C,$7C,$FC,$FC,$FC,$FC
    .byte $7C,$7C,$7C,$FC,$FC,$FC,$FC,$FF,$FC,$FC,$FC,$FC,$7F,$7F,$7F,$FF,$7F,$7F,$7F,$7F,$7F,$7F,$7F,$7F
    .byte $7C,$7C,$7C,$7C,$7F,$7F,$7F,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FE,$FD,$FE,$FE,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$3F,$DF,$DF,$EF,$F7,$F7,$FB,$FB,$FD,$FB,$FB,$F7,$EF,$EF,$DF,$DF,$3F
    .byte $FF,$FC,$FB,$FB,$F7,$F7,$EF,$DF,$DF,$BF,$DF,$DF,$EF,$EF,$F7,$FB,$FB,$FC,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $00,$FF,$03,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FC,$FF,$00,$FF,$03,$FD,$FD,$FD,$FD,$FD
    .byte $FD,$FD,$FD,$FC,$03,$7B,$03,$FF,$03,$FB,$03,$FC,$FD,$FD,$FD,$FD,$FD,$FD,$FD,$FC,$03,$FB,$03,$FF
    .byte $00,$FF,$1C,$EB,$EB,$E3,$1F,$5F,$1F,$FF,$FF,$FF,$00,$FF,$00,$FF,$FC,$FD,$FD,$FD,$FD,$FD,$03,$FF
    .byte $03,$FD,$FD,$FD,$FD,$FD,$FC,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FC,$FB,$FB,$F7,$F7,$EF,$DF,$DF,$BF,$DF,$DF,$EF,$EF,$F7,$FB,$FB,$FC
    .byte $FF,$3F,$DF,$DF,$EF,$F7,$F7,$FB,$FB,$FD,$FB,$FB,$F7,$EF,$EF,$DF,$DF,$3F,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $00,$FF,$07,$FA,$FA,$FA,$FA,$FA,$FA,$FA,$FA,$F8,$FF,$FF,$FF,$FF,$00,$FF,$07,$FA,$FA,$FA,$C6,$DE
    .byte $C6,$3A,$BA,$38,$FF,$FF,$FF,$FF,$00,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$00,$FF
    .byte $00,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$F8,$FA,$FA,$07,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF
    .byte $00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FE,$FD,$FD,$FB,$FB,$F7,$EF,$EF,$DF,$EF,$EF,$F7,$F7,$FB,$FD,$FD,$FE
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $F0,$F7,$F0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F0,$F7,$F0,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FE,$FE,$FE,$F1,$F5,$F1,$FF,$FE,$FE,$FE,$F1,$F5,$F5,$F5,$F5,$F5,$F5,$F5,$F1,$FE,$FE,$FE,$FF
    .byte $F0,$F7,$F0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F0,$F7,$F0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$F0,$F7
    .byte $F0,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FE,$FD,$FD,$FB,$FB,$F7,$EF,$EF,$DF,$EF,$EF,$F7,$F7,$FB,$FD,$FD,$FE,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$DB
    .byte $D5,$ED,$FF,$C1,$F5,$FB,$FF,$C3,$F5,$C3,$FF,$E3,$DD,$DD,$FF,$C1,$D5,$DD,$FF,$FF,$FF,$FF,$FF,$FD
    .byte $C1,$FD,$FF,$E3,$DD,$E3,$FF,$FF,$FF,$FF,$FF,$DB,$D5,$ED,$FF,$FD,$C1,$FD,$FF,$C3,$F5,$C3,$FF,$C1
    .byte $F5,$CB,$FF,$FD,$C1,$FD,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
tiles:
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$88,$48,$48,$28,$28,$18,$18,$FF,$08,$0C,$0C,$0A,$0A,$0A,$09,$09
    .byte $88,$48,$48,$28,$1C,$36,$63,$C1,$63,$36,$1C,$0A,$0A,$0A,$09,$09,$88,$48,$48,$28,$1C,$3E,$7F,$FF
    .byte $7F,$3E,$1C,$0A,$0A,$0A,$09,$09
view_cells:
    .byte $FF,$FF,$00,$00,$01,$01,$02,$02,$03,$03,$04,$04,$05,$05,$FF,$FF,$FF,$FF,$06,$06,$07,$07,$08,$08
    .byte $09,$09,$0A,$0A,$0B,$0B,$FF,$FF,$FF,$FF,$0C,$0C,$0D,$0D,$0E,$0E,$0F,$0F,$10,$10,$11,$11,$FF,$FF
    .byte $FF,$FF,$12,$12,$13,$13,$14,$14,$15,$15,$16,$16,$17,$17,$FF,$FF,$FF,$FF,$18,$18,$19,$19,$1A,$1A
    .byte $1B,$1B,$1C,$1C,$1D,$1D,$FF,$FF,$FF,$FF,$1E,$1E,$1F,$1F,$20,$20,$21,$21,$22,$22,$23,$23,$FF,$FF
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
view_subtiles:
    .byte $00,$00,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$00,$00,$00,$00,$01,$00,$01,$00,$01
    .byte $00,$01,$00,$01,$00,$01,$00,$00,$00,$00,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$00
    .byte $00,$00,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$00,$00,$00,$00,$01,$00,$01,$00,$01
    .byte $00,$01,$00,$01,$00,$01,$00,$00,$00,$00,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$01,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
neighbors:
    .byte $FF,$FF,$FF,$FF,$FF,$FF,$00,$01,$02,$03,$04,$05,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E,$0F,$10,$11
    .byte $12,$13,$14,$15,$16,$17,$18,$19,$1A,$1B,$1C,$1D,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E,$0F,$10,$11
    .byte $12,$13,$14,$15,$16,$17,$18,$19,$1A,$1B,$1C,$1D,$1E,$1F,$20,$21,$22,$23,$FF,$FF,$FF,$FF,$FF,$FF
    .byte $FF,$00,$01,$02,$03,$04,$FF,$06,$07,$08,$09,$0A,$FF,$0C,$0D,$0E,$0F,$10,$FF,$12,$13,$14,$15,$16
    .byte $FF,$18,$19,$1A,$1B,$1C,$FF,$1E,$1F,$20,$21,$22,$01,$02,$03,$04,$05,$FF,$07,$08,$09,$0A,$0B,$FF
    .byte $0D,$0E,$0F,$10,$11,$FF,$13,$14,$15,$16,$17,$FF,$19,$1A,$1B,$1C,$1D,$FF,$1F,$20,$21,$22,$23,$FF
hex_links:
    .byte $FF,$06,$FF,$01,$FF,$FF,$FF,$07,$00,$02,$FF,$06,$FF,$08,$01,$03,$FF,$07,$FF,$09,$02,$04,$FF,$08
    .byte $FF,$0A,$03,$05,$FF,$09,$FF,$0B,$04,$FF,$FF,$0A,$00,$0C,$FF,$07,$01,$FF,$01,$0D,$06,$08,$02,$0C
    .byte $02,$0E,$07,$09,$03,$0D,$03,$0F,$08,$0A,$04,$0E,$04,$10,$09,$0B,$05,$0F,$05,$11,$0A,$FF,$FF,$10
    .byte $06,$12,$FF,$0D,$07,$FF,$07,$13,$0C,$0E,$08,$12,$08,$14,$0D,$0F,$09,$13,$09,$15,$0E,$10,$0A,$14
    .byte $0A,$16,$0F,$11,$0B,$15,$0B,$17,$10,$FF,$FF,$16,$0C,$18,$FF,$13,$0D,$FF,$0D,$19,$12,$14,$0E,$18
    .byte $0E,$1A,$13,$15,$0F,$19,$0F,$1B,$14,$16,$10,$1A,$10,$1C,$15,$17,$11,$1B,$11,$1D,$16,$FF,$FF,$1C
    .byte $12,$1E,$FF,$19,$13,$FF,$13,$1F,$18,$1A,$14,$1E,$14,$20,$19,$1B,$15,$1F,$15,$21,$1A,$1C,$16,$20
    .byte $16,$22,$1B,$1D,$17,$21,$17,$23,$1C,$FF,$FF,$22,$18,$FF,$FF,$1F,$19,$FF,$19,$FF,$1E,$20,$1A,$FF
    .byte $1A,$FF,$1F,$21,$1B,$FF,$1B,$FF,$20,$22,$1C,$FF,$1C,$FF,$21,$23,$1D,$FF,$1D,$FF,$22,$FF,$FF,$FF
hex_sources:
    .byte $01,$01,$01,$01,$01,$01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$01,$01,$01,$01,$01,$01
    .byte $01,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00
    .byte $01,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$01
    .byte $00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$01,$00,$00,$00,$00,$00,$01
hex_weights:
    .byte $01,$02,$03,$02,$01,$00,$02,$03,$04,$03,$02,$01,$03,$04,$05,$04,$03,$02,$02,$03,$04,$03,$02,$01
    .byte $01,$02,$03,$02,$01,$00,$00,$01,$02,$01,$00,$00
