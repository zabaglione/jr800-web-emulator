; SPDX-License-Identifier: MIT
.section .gfx, code
gfx_init:
    CLR gfx_last_mode
    LDAA #16
    STAA $0000
    LDAA #$EF
    STAA sound_port
    JMP dirty_reset
gfx_title:
    LDX #gfx_title_data
    STX P1
    LDX #framebuffer
    STX DRAW
gfx_unpack_packet:
    LDX P1
    LDAB 0,X
    BEQ gfx_title_choices
    INX
    STX P1
    TSTB
    BMI gfx_unpack_repeat
    STAB COUNT
gfx_unpack_literal:
    LDX P1
    LDAA 0,X
    INX
    STX P1
    LDX DRAW
    STAA 0,X
    INX
    STX DRAW
    DEC COUNT
    BNE gfx_unpack_literal
    JSR poll_key
    BRA gfx_unpack_packet
gfx_unpack_repeat:
    SUBB #127
    STAB COUNT
    LDAA 0,X
    STAA TEMP
    INX
    STX P1
    LDD DRAW
    SUBB TEMP
    SBCA #0
    STD P2
gfx_unpack_run:
    LDX P2
    LDAA 0,X
    INX
    STX P2
    LDX DRAW
    STAA 0,X
    INX
    STX DRAW
    DEC COUNT
    BNE gfx_unpack_run
    JSR poll_key
    BRA gfx_unpack_packet
gfx_title_choices:
    CLR framebuffer + 771
    CLR framebuffer + 772
    CLR framebuffer + 963
    CLR framebuffer + 964
    CLR framebuffer + 1155
    CLR framebuffer + 1156
    LDAA G_DIFFICULTY
    STAA G_MENU
    LDX #difficulty_names
    STX UI_PTR
    LDAA #3
    STAA COUNT
    LDAA #4
    STAA UI_ROW
    JMP render_choices
gfx_hud_finish:
    LDX #framebuffer
    LDAB #192
gfx_hud_inverse:
    COM 0,X
    INX
    DECB
    BNE gfx_hud_inverse
    RTS

; Reconstruct the original map layers in the single framebuffer. Comparing
; each copied byte keeps the dirty ranges conservative without another image.
.section .gfx_draw, code
gfx_clear:
    LDAA G_MODE
    CMPA #1
    BNE gfx_clear_all
    LDAB gfx_last_mode
    STAA gfx_last_mode
    CMPB #1
    BNE gfx_clear_all
    LDX #framebuffer
    JSR gfx_blank_band
    CLRA
    JSR gfx_dirty_band
    LDX #framebuffer + 1344
    JSR gfx_blank_band
    LDAA #7
    JMP gfx_dirty_band
gfx_clear_all:
    TSTA
    BNE gfx_keep_clear_state
    CLR gfx_clear_done
    CLR gfx_clear_active
gfx_keep_clear_state:
    STAA gfx_last_mode
    JSR clear
    JMP dirty_all
gfx_blank_band:
    LDAB #192
    CLRA
gfx_blank_column:
    STAA 0,X
    INX
    DECB
    BNE gfx_blank_column
    RTS
gfx_dirty_band:
    STAA gfx_band
    CLRB
    JSR dirty_mark
    LDAA gfx_band
    LDAB #191
    JMP dirty_mark
gfx_draw_tile:
    LDAB #8
    MUL
    ADDD #tiles
    STD P1
    CLR gfx_changed
    LDAB #8
gfx_tile_column:
    LDX P1
    LDAA 0,X
    INX
    STX P1
    LDX DRAW
    CMPA 0,X
    BEQ gfx_same_column
    STAA 0,X
    INC gfx_changed
gfx_same_column:
    INX
    STX DRAW
    DECB
    BNE gfx_tile_column
    TST gfx_changed
    BEQ gfx_tile_done
    LDD DRAW
    SUBD #framebuffer + 8
    CLR gfx_band
gfx_find_band:
    SUBD #192
    BCS gfx_found_band
    INC gfx_band
    BRA gfx_find_band
gfx_found_band:
    ADDD #192
    STAB gfx_column
    LDAA gfx_band
    JSR dirty_mark
    LDAA gfx_band
    LDAB gfx_column
    ADDB #7
    JMP dirty_mark
gfx_tile_done:
    RTS
.section .gfx_state, bss
gfx_last_mode: .space 1
gfx_changed: .space 1
gfx_band: .space 1
gfx_column: .space 1

.section .gfx, code
gfx_chirp:
    LDX #100
    LDD #8
    JSR sound_tone
    JMP poll_key
.section .gfx_audio, code
gfx_win_scene:
    LDAA #1
    STAA gfx_clear_active
    CLR gfx_note
    CLR gfx_started
    LDAA #6
    STAA gfx_delay
    LDAA #1
    STAA G_MODE
    JSR render_screen
    LDAA #9
    STAA G_MODE
    RTS
; Two sound periods per poll; the hardware timer owns the note duration.
gfx_tick:
    TST gfx_clear_active
    BEQ gfx_tick_idle
    LDAA G_MODE
    CMPA #9
    BEQ gfx_tick_won
    CLR gfx_clear_active
    BRA gfx_tick_idle
gfx_tick_won:
    TST gfx_started
    BNE gfx_tick_clock
    INC gfx_started
    LDD $0009
    STD gfx_clock
gfx_tick_clock:
    LDD $0009
    SUBD gfx_clock
    SUBD #20000
    BCS gfx_note_sound
    LDD gfx_clock
    ADDD #20000
    STD gfx_clock
    DEC gfx_delay
    BNE gfx_note_sound
    INC gfx_note
    LDAA #8
    STAA gfx_delay
    LDAB gfx_note
    CMPB #7
    BCS gfx_note_sound
    LDAA #20
    STAA gfx_delay
    CMPB #9
    BCS gfx_note_sound
    CLR gfx_clear_active
    CLR G_PENDING
    INC gfx_clear_done
    LDAA #1
    RTS
gfx_note_sound:
    LDAB gfx_note
    BEQ gfx_tick_idle
    CMPB #8
    BCC gfx_tick_idle
    DECB
    ASLB
    LDX #gfx_notes
    ABX
    LDX 0,X
    LDD #2
    JSR sound_tone
gfx_tick_idle:
    CLRA
    RTS
.section .gfx_notes, data
gfx_notes: .word 339,286,226,189,226,169,139
.section .gfx_state, bss
.global gfx_clear_active
.global gfx_clear_done
gfx_clear_active: .space 1
gfx_clear_done: .space 1
gfx_note: .space 1
gfx_started: .space 1
gfx_delay: .space 1
gfx_clock: .space 2
