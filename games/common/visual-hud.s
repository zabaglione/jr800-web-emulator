; SPDX-License-Identifier: MIT
; Per-game visual specification. One framebuffer; scalar caches, no second screen.
; Field: x,band,characters,font,invert,limit,cache pointer,text table pointer.
.global hud_ready
.global view_origin
.section .text, code
hud_begin:
    TST hud_ready
    BEQ hud_begin_first
    RTS
hud_begin_first:
    INC hud_ready
    LDX #hud_cache
    LDAB #HUD_FIELDS * 3
    CLRA
hud_reset_fields:
    STAA 0,X
    INX
    DECB
    BNE hud_reset_fields
    JMP hud_background
hud_background:
    JSR hud_select_background
    STX hud_span_pointer
hud_background_next:
    LDX hud_span_pointer
    LDAA 4,X
    BEQ hud_background_done
    STAA paint_count
    LDD 0,X
    STD paint_source
    LDAA 2,X
    STAA paint_band
    LDAA 3,X
    STAA paint_x
    LDAB #5
    ABX
    STX hud_span_pointer
    JSR paint_address
    CLR paint_id
    JSR paint_blit
    JSR input_poll
    BRA hud_background_next
hud_background_done:
    RTS
; D=value, X=field. Return A=0 when unchanged, A=1 when changed.
hud_changed:
    STD hud_value
    STX hud_field
    LDX 6,X
    TST 0,X
    BEQ hud_cache_write
    LDD 1,X
    SUBD hud_value
    BNE hud_cache_write
    CLRA
    RTS
hud_cache_write:
    LDAA #1
    STAA 0,X
    LDD hud_value
    STD 1,X
    LDAA #1
    RTS
hud_number:
    JSR hud_changed
    TSTA
    BNE hud_number_new
    RTS
hud_number_new:
    LDX hud_field
    LDAA 3,X
    CMPA #4
    BNE hud_number_digits
    JMP hud_meter
hud_number_digits:
    LDX #hud_divisors
    STX hud_divisor_ptr
    LDX #hud_digits
    STX hud_digit_ptr
hud_digit_loop:
    LDX hud_divisor_ptr
    LDD 0,X
    STD hud_divisor
    INX
    INX
    STX hud_divisor_ptr
    LDAA #48
    STAA hud_digit
hud_digit_subtract:
    LDD hud_value
    SUBD hud_divisor
    BCS hud_digit_write
    STD hud_value
    INC hud_digit
    BRA hud_digit_subtract
hud_digit_write:
    LDX hud_digit_ptr
    LDAA hud_digit
    STAA 0,X
    INX
    STX hud_digit_ptr
    CPX #hud_digits + 5
    BNE hud_digit_loop
    LDX #hud_digits
hud_trim_zeros:
    CPX #hud_digits + 4
    BEQ hud_digits_ready
    LDAA 0,X
    CMPA #48
    BNE hud_digits_ready
    LDAA #32
    STAA 0,X
    INX
    BRA hud_trim_zeros
hud_digits_ready:
    LDX hud_field
    LDAB #5
    SUBB 2,X
    LDX #hud_digits
    ABX
    STX hud_string
    JMP hud_paint
hud_choice:
    JSR hud_changed
    TSTA
    BNE hud_choice_new
    RTS
hud_choice_new:
    LDX hud_field
    LDAA hud_value + 1
    CMPA 5,X
    BCS hud_choice_valid
    CLRA
hud_choice_valid:
    LDAB 2,X
    MUL
    ADDD 8,X
    STD hud_string
    JMP hud_paint
hud_play_text:
    STX hud_string
    STAA hud_play_field
    STAB hud_play_field + 1
    CLRB
hud_play_length:
    TST 0,X
    BEQ hud_play_length_done
    INCB
    INX
    BRA hud_play_length
hud_play_length_done:
    TSTB
    BEQ hud_play_done
    STAB hud_play_field + 2
    LDX #hud_play_field
    STX hud_field
    JMP hud_paint
hud_play_done:
    RTS
hud_paint:
    LDX hud_field
    LDAA 0,X
    STAA hud_pen
    LDAA 1,X
    STAA hud_band
    LDAA 2,X
    STAA hud_count
    LDAA 3,X
    STAA hud_font
    LDAA 4,X
    STAA hud_inverse
hud_character:
    LDX hud_string
    LDAA 0,X
    INX
    STX hud_string
    STAA hud_char
    LDAA #1
    STAA hud_rows
    LDAA hud_font
    BEQ hud_glyph_tiny
    LDAB #6
    CMPA #1
    BEQ hud_glyph_dimensions
    CMPA #5
    BNE hud_glyph_large
    LDAB #5
    BRA hud_glyph_dimensions
hud_glyph_large:
    INC hud_rows
    CMPA #2
    BEQ hud_glyph_dimensions
    LDAB #11
    BRA hud_glyph_dimensions
hud_glyph_tiny:
    LDAB #4
hud_glyph_dimensions:
    STAB hud_width
    LDAA hud_font
    BEQ hud_source_tiny
    CMPA #5
    BEQ hud_source_text
    LDAA hud_char
    CMPA #32
    BNE hud_source_digit
    LDAA #58
hud_source_digit:
    SUBA #48
    LDAB hud_font
    CMPB #1
    BEQ hud_source_normal
    CMPB #2
    BEQ hud_source_tall
    LDAB #22
    MUL
    ADDD #hud_font_wide
    BRA hud_source_ready
hud_source_tall:
    LDAB #12
    MUL
    ADDD #hud_font_tall
    BRA hud_source_ready
hud_source_normal:
    LDAB #6
    MUL
    ADDD #hud_font_normal
    BRA hud_source_ready
hud_source_text:
    LDAA hud_char
    SUBA #32
    LDAB #5
    MUL
    ADDD #font
    BRA hud_source_ready
hud_source_tiny:
    LDAA hud_char
    SUBA #32
    LDAB #4
    MUL
    ADDD #hud_font_tiny
hud_source_ready:
    STD paint_source
    LDAA hud_band
    STAA paint_band
hud_glyph_row:
    LDAA hud_pen
    STAA paint_x
    JSR paint_address
    LDAA hud_inverse
    STAA paint_id
    LDAA hud_width
    STAA paint_count
    JSR paint_blit
    LDAA hud_font
    CMPA #5
    BNE hud_no_text_space
    LDX #hud_zero
    STX paint_source
    LDAA #1
    STAA paint_count
    JSR paint_blit
hud_no_text_space:
    INC paint_band
    DEC hud_rows
    BNE hud_glyph_row
    LDAA paint_x
    STAA hud_pen
    JSR input_poll
    DEC hud_count
    BEQ hud_paint_done
    JMP hud_character
hud_paint_done:
    RTS
; A six-dot gauge. Saturation and scale are computed only when its value changes.
hud_meter:
    LDX hud_field
    LDAA hud_value + 1
    TST hud_value
    BNE hud_meter_full
    CMPA 5,X
    BCS hud_meter_scale
hud_meter_full:
    LDAA 5,X
hud_meter_scale:
    LDAB 2,X
    SUBB #2
    MUL
    STD hud_value
    CLR hud_filled
    CLRA
    LDAB 5,X
    STD hud_divisor
hud_meter_divide:
    LDD hud_value
    SUBD hud_divisor
    BCS hud_meter_ready
    STD hud_value
    INC hud_filled
    BRA hud_meter_divide
hud_meter_ready:
    LDAA 0,X
    STAA paint_x
    LDAA 1,X
    STAA paint_band
    JSR paint_address
    LDX hud_field
    LDAA 2,X
    STAA hud_count
    STAA hud_width
    CLR hud_char
hud_meter_column:
    LDAA #63
    TST hud_char
    BEQ hud_meter_store
    LDAA hud_count
    CMPA #1
    BEQ hud_meter_solid
    LDAA hud_char
    CMPA hud_filled
    BLS hud_meter_solid
    LDAA #33
    BRA hud_meter_store
hud_meter_solid:
    LDAA #63
hud_meter_store:
    LDX hud_field
    TST 4,X
    BEQ hud_meter_compare
    COMA
hud_meter_compare:
    LDX paint_dest
    CMPA 0,X
    BEQ hud_meter_same
    STAA 0,X
    LDAA paint_band
    LDAB paint_x
    JSR dirty_mark
hud_meter_same:
    LDX paint_dest
    INX
    STX paint_dest
    INC paint_x
    INC hud_char
    DEC hud_count
    BNE hud_meter_column
    JMP input_poll
.section .bss, bss
hud_play_field: .space 10
hud_span_pointer: .space 2
hud_ready: .space 1
hud_cache: .space HUD_FIELDS * 3
hud_value: .space 2
hud_field: .space 2
hud_string: .space 2
hud_divisor_ptr: .space 2
hud_divisor: .space 2
hud_digit_ptr: .space 2
hud_digit: .space 1
hud_digits: .space 5
hud_pen: .space 1
hud_band: .space 1
hud_count: .space 1
hud_font: .space 1
hud_inverse: .space 1
hud_char: .space 1
hud_width: .space 1
hud_rows: .space 1
hud_filled: .space 1
.section .data, data
hud_zero: .byte 0
hud_divisors: .word 10000,1000,100,10,1
