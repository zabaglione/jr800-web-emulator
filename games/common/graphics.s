; SPDX-License-Identifier: MIT
; Single framebuffer, cached 16x7 tile IDs; only changed tiles are painted.
; A high tile bit inverts the sprite for selection/cursor highlighting.
.global tile_cache
.global paint_tile
.global paint_text
.global paint_number
paint_clear:
    CLR hud_ready
    LDX #framebuffer
    STX paint_dest
    LDAA #12
    STAA clear_chunks
paint_clear_chunk:
    LDX paint_dest
    CLRA
    LDAB #128
paint_clear_bytes:
    STAA 0,X
    INX
    DECB
    BNE paint_clear_bytes
    STX paint_dest
    JSR input_poll
    DEC clear_chunks
    BNE paint_clear_chunk
    LDX #tile_cache
    LDAA #$FF
    LDAB #112
paint_clear_cache:
    STAA 0,X
    INX
    DECB
    BNE paint_clear_cache
    JMP dirty_all
; A = tile, B = cell 0..111. All registers clobbered.
paint_tile:
    STAB paint_cell
    STAA paint_id
    LDX #tile_cache
    ABX
    CMPA 0,X
    BEQ paint_tile_done
    STAA 0,X
    ANDA #127
    LDAB #8
    MUL
    ADDD #tiles
    STD paint_source
    LDAA paint_cell
    ANDA #15
    ASLA
    ASLA
    ASLA
    ADDA #VIEW_X
    STAA paint_x
    LDAA paint_cell
    LSRA
    LSRA
    LSRA
    LSRA
    INCA
    STAA paint_band
    JSR paint_address
    LDAA #8
    STAA paint_count
    JSR paint_blit
; @if puzzle
    JSR challenge_mark_tile
; @endif
paint_tile_done:
    RTS
; X = source ASCII string; A = x, B = band. One band, caller checks bounds.
paint_text:
    STX paint_string
    STAA paint_x
    STAB paint_band
    LDAA #8
    STAA text_poll_count
paint_text_next:
    LDX paint_string
    LDAA 0,X
    BEQ paint_text_done
    INX
    STX paint_string
    CMPA #124
    BNE paint_text_ascii
    LDX #paint_vertical
    STX paint_source
    BRA paint_text_glyph
paint_text_ascii:
    SUBA #32
    LDAB #5
    MUL
    ADDD #font
    STD paint_source
paint_text_glyph:
    JSR paint_address
    CLR paint_id
    LDAA #5
    STAA paint_count
    JSR paint_blit
    ; Sixth column is spacing, including erasure of an old longer glyph.
    LDX #paint_zero
    STX paint_source
    LDAA #1
    STAA paint_count
    JSR paint_blit
    DEC text_poll_count
    BNE paint_text_next
    JSR input_poll
    LDAA #8
    STAA text_poll_count
    BRA paint_text_next
paint_text_done:
    JMP input_poll
; A = unsigned number; paint_x/paint_band already set. Fixed three digits.
paint_number:
    LDAB #48
paint_hundreds:
    CMPA #100
    BCS paint_tens_begin
    SUBA #100
    INCB
    BRA paint_hundreds
paint_tens_begin:
    STAB paint_digits
    LDAB #48
paint_tens:
    CMPA #10
    BCS paint_ones
    SUBA #10
    INCB
    BRA paint_tens
paint_ones:
    STAB paint_digits + 1
    ADDA #48
    STAA paint_digits + 2
    LDX #paint_digits
    LDAA paint_x
    LDAB paint_band
    JMP paint_text
paint_address:
    LDAA paint_band
    LDAB #192
    MUL
    ADDD #framebuffer
    ADDB paint_x
    ADCA #0
    STD paint_dest
    RTS
paint_blit:
    CLR paint_changed
    LDAA paint_x
    STAA paint_first
paint_blit_loop:
    LDX paint_source
    LDAA 0,X
    INX
    STX paint_source
    TST paint_id
    BPL paint_blit_normal
    COMA
paint_blit_normal:
    LDX paint_dest
    CMPA 0,X
    BEQ paint_blit_same
    STAA 0,X
    INC paint_changed
paint_blit_same:
    INX
    STX paint_dest
    INC paint_x
    DEC paint_count
    BNE paint_blit_loop
    TST paint_changed
    BEQ paint_blit_done
    LDAA paint_band
    LDAB paint_first
    JSR dirty_mark
    LDAA paint_band
    LDAB paint_x
    DECB
    JSR dirty_mark
paint_blit_done:
    RTS
; Draw 16x7 game view through the game-specific tile compositor.
paint_board:
    CLR paint_index
paint_board_loop:
    LDAB paint_index
    JSR game_tile
    LDAB paint_index
    JSR paint_tile
    INC paint_index
    LDAA paint_index
    ANDA #7
    BNE paint_board_continue
    JSR input_poll
paint_board_continue:
    LDAA paint_index
    CMPA #112
    BNE paint_board_loop
    RTS
.section .bss, bss
tile_cache: .space 112
clear_chunks: .space 1
text_poll_count: .space 1
paint_id: .space 1
paint_cell: .space 1
paint_index: .space 1
paint_source: .space 2
paint_dest: .space 2
paint_string: .space 2
paint_x: .space 1
paint_band: .space 1
paint_first: .space 1
paint_count: .space 1
paint_changed: .space 1
paint_digits: .space 4
.section .data, data
paint_zero: .byte 0
paint_vertical: .byte 0,0,255,0,0
.section .text, code
; D = unsigned value 0..9999. Four columns, no division helper or heap.
paint_number16:
    STD paint_value
    LDX #paint_divisors
    STX paint_divisor_ptr
    LDX #paint_digits16
    STX paint_digit_ptr
paint_decimal_loop:
    LDX paint_divisor_ptr
    LDD 0,X
    STD paint_divisor
    INX
    INX
    STX paint_divisor_ptr
    LDAA #48
    STAA paint_digit
paint_decimal_subtract:
    LDD paint_value
    SUBD paint_divisor
    BCS paint_decimal_store
    STD paint_value
    INC paint_digit
    BRA paint_decimal_subtract
paint_decimal_store:
    LDX paint_digit_ptr
    LDAA paint_digit
    STAA 0,X
    INX
    STX paint_digit_ptr
    CPX #paint_digits16 + 4
    BNE paint_decimal_loop
    LDX #paint_digits16
    LDAA paint_x
    LDAB paint_band
    JMP paint_text
.section .bss, bss
paint_value: .space 2
paint_divisor: .space 2
paint_divisor_ptr: .space 2
paint_digit_ptr: .space 2
paint_digit: .space 1
paint_digits16: .space 5
.section .data, data
paint_divisors: .word 1000,100,10,1
.section .text, code
