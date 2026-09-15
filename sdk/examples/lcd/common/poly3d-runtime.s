; SPDX-License-Identifier: MIT
; Example host code executed on the JR-800, not in the browser host.
.global entry
.global frame_ready
.global frame_counter
.global p3_poll
.global scene_clear
.global ui_number
.global ui_footer
.global hud_dirty
.extern basic_save
.extern init
.extern clear
.extern text
.extern glyph
.extern framebuffer
.extern dirty_all
.extern dirty_begin
.extern dirty_next
.extern dirty_min
.extern dirty_max
.extern dirty_pending
.extern input_init
.extern input_poll
.extern input_take
.extern input_ticks
.extern p3_init
.extern p3_begin
.extern p3_clip_top
.extern p3_clip_bottom
.extern p3_error
.extern app_init
.extern app_update
.extern app_draw
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR scene_clear
    JSR input_init
    CLR frame_counter
    CLR runtime_tick
    JSR app_init
    BRA runtime_draw
frame_ready:
    INC frame_counter
runtime_wait:
    ; Minimum update interval: three existing 20,000-E-cycle input ticks.
    ; Long rendering reduces the update rate; no host timer or input patching.
    JSR input_poll
    LDAA input_ticks
    SUBA runtime_tick
    CMPA #3
    BCS runtime_wait
    LDAA input_ticks
    STAA runtime_tick
    JSR input_take
    JSR app_update
runtime_draw:
    JSR p3_begin
    JSR app_draw
    TST p3_error
    BNE runtime_error
    JSR runtime_present
    BRA frame_ready
p3_poll:
    JMP input_poll
runtime_present:
    JSR dirty_begin
runtime_transfer:
    JSR input_poll
    JSR dirty_next
    BNE runtime_transfer
    RTS
runtime_error:
    JSR clear
    JSR dirty_all
    LDX #runtime_error_text
    LDD #framebuffer + 576 + 54
    JSR text
    JSR runtime_present
runtime_error_wait:
    JSR input_poll
    BRA runtime_error_wait
scene_clear:
    JSR clear
    JSR dirty_all
    JSR p3_init
    LDAA #8
    STAA p3_clip_top
    LDAA #55
    STAA p3_clip_bottom
    LDAA #1
    STAA hud_dirty
    RTS
; A=0..99, X=two-character destination in the top band.
ui_number:
    STX ui_pointer
    CMPA #100
    BCS ui_number_bounded
    LDAA #99
ui_number_bounded:
    CLR ui_tens
ui_divide:
    CMPA #10
    BCS ui_digits
    SUBA #10
    INC ui_tens
    BRA ui_divide
ui_digits:
    STAA ui_ones
    LDAA ui_tens
    ADDA #48
    JSR glyph
    LDX ui_pointer
    LDAB #6
    ABX
    LDAA ui_ones
    ADDA #48
    JSR glyph
    LDD ui_pointer
    SUBD #framebuffer
    CMPB dirty_min
    BCC ui_number_max
    STAB dirty_min
ui_number_max:
    ADDB #11
    CMPB dirty_max
    BLS ui_number_done
    STAB dirty_max
ui_number_done:
    LDAA #1
    STAA dirty_pending
    RTS
; X=ASCII footer, at most 31 characters; only the bottom band is replaced.
ui_footer:
    STX ui_pointer
    LDX #framebuffer + 1344
    CLRA
ui_footer_clear:
    STAA 0,X
    INX
    CPX #framebuffer + 1536
    BNE ui_footer_clear
    LDX ui_pointer
    LDD #framebuffer + 1344 + 6
    JSR text
    CLR dirty_min + 7
    LDAA #191
    STAA dirty_max + 7
    LDAA #1
    STAA dirty_pending
    RTS
.section .bss, bss
frame_counter: .space 1
runtime_tick: .space 1
hud_dirty: .space 1
ui_pointer: .space 2
ui_tens: .space 1
ui_ones: .space 1
.section .data, data
runtime_error_text: .byte 80,79,76,89,51,68,32,69,82,82,79,82,0
