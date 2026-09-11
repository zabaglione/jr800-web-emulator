; SPDX-License-Identifier: MIT
; Small moving objects share the single framebuffer with cached background tiles.
; Invalidate the old/new rectangle, paint the background, then plot the objects.
.global scene_rect
.global scene_pixel
.global scene_w
.global scene_h
.section .text, code
; A=x, B=y; scene_w/h are pixel dimensions. Clip to x0..127,y8..63.
scene_rect:
    TST scene_w
    BNE scene_width_nonzero
    RTS
scene_width_nonzero:
    TST scene_h
    BEQ scene_rect_done
    CMPA #128
    BCC scene_rect_done
    CMPB #64
    BCC scene_rect_done
    STAA scene_left
    STAB scene_top
    ADDA scene_w
    BCS scene_clip_right
    DECA
    CMPA #128
    BCS scene_right_ready
scene_clip_right:
    LDAA #127
scene_right_ready:
    LSRA
    LSRA
    LSRA
    STAA scene_right
    LDAA scene_top
    ADDA scene_h
    BCS scene_clip_bottom
    DECA
    CMPA #64
    BCS scene_bottom_ready
scene_clip_bottom:
    LDAA #63
scene_bottom_ready:
    CMPA #8
    BCS scene_rect_done
    LSRA
    LSRA
    LSRA
    DECA
    STAA scene_bottom
    LDAA scene_top
    CMPA #8
    BCC scene_top_ready
    LDAA #8
scene_top_ready:
    LSRA
    LSRA
    LSRA
    DECA
    STAA scene_row
    LDAA scene_left
    LSRA
    LSRA
    LSRA
    STAA scene_left
scene_rect_row:
    LDAA scene_row
    ASLA
    ASLA
    ASLA
    ASLA
    ADDA scene_left
    TAB
    LDX #tile_cache
    ABX
    LDAB scene_right
    SUBB scene_left
    INCB
    LDAA #255
scene_rect_column:
    STAA 0,X
    INX
    DECB
    BNE scene_rect_column
    LDAA scene_row
    CMPA scene_bottom
    BEQ scene_rect_done
    INC scene_row
    BRA scene_rect_row
scene_rect_done:
    RTS
; A=x,B=y. Set one pixel, compare before marking the LCD band.
scene_pixel:
    CMPA #128
    BCC scene_pixel_done
    CMPB #8
    BCS scene_pixel_done
    CMPB #64
    BCC scene_pixel_done
    STAA scene_x
    STAB scene_y
    ANDB #7
    LDX #scene_masks
    ABX
    LDAA 0,X
    STAA scene_mask
    LDAA scene_y
    LSRA
    LSRA
    LSRA
    STAA scene_band
    LDAB #192
    MUL
    ADDD #framebuffer
    ADDB scene_x
    ADCA #0
    XGDX
    LDAA 0,X
    ORAA scene_mask
    CMPA 0,X
    BEQ scene_pixel_done
    STAA 0,X
    LDAA scene_band
    LDAB scene_x
    JMP dirty_mark
scene_pixel_done:
    RTS
.section .bss, bss
scene_w: .space 1
scene_h: .space 1
scene_left: .space 1
scene_top: .space 1
scene_right: .space 1
scene_bottom: .space 1
scene_row: .space 1
scene_x: .space 1
scene_y: .space 1
scene_band: .space 1
scene_mask: .space 1
.section .data, data
scene_masks: .byte 1,2,4,8,16,32,64,128
