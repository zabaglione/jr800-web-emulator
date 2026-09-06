; SPDX-License-Identifier: MIT
; Software PCG runner for the existing E-184/E-186 JR-800 LCD model.
; SPACE: E-392 ROM-expected $0FEF bit 0, requiring the explicit idle base.
.global entry
.global frame_ready
.global phase
.global height
.global velocity
.global obstacle_x
.global obstacle_kind
.global speed
.global distance
.global high_score
.global tick
.global space_previous
.global animation
.global gap_wait
.extern init
.extern clear
.extern present
.extern text
.extern glyph
.extern framebuffer
.extern blit
.extern dino_run_a
.extern dino_run_b
.extern dino_air
.extern dino_dead
.extern cactus_a
.extern cactus_b
.extern cloud
.equ hud_dest, $A4
.equ hud_digits, $A6

.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR init
    CLRA
    STAA phase
    STAA space_previous
    STAA space_pending
    STAA high_score
    STAA high_score + 1
    STAA high_score + 2
    LDAA #$A7
    STAA random_state
    JSR reset_run
    JMP render

reset_run:
    CLRA
    STAA height
    STAA velocity
    STAA distance
    STAA distance + 1
    STAA distance + 2
    STAA tick
    STAA animation
    STAA obstacle_kind
    STAA gap_wait
    STAA ground_offset
    LDAA #190
    STAA obstacle_x
    LDAA #3
    STAA speed
    LDAA #130
    STAA cloud_x
    RTS

frame_ready:
    ; A short CPU delay leaves room for the LCD transfer (~50k E cycles).
    LDX #2000
wait_frame:
    DEX
    BNE wait_frame
    JSR poll_space
    LDAA space_pending
    STAA space_edge
    CLR space_pending
    LDAA phase
    CMPA #1
    BEQ update_running
    TST space_edge
    BEQ redraw
    JSR reset_run
    LDAA #1
    STAA phase
    LDAA #7
    STAA velocity
    BRA update_running
redraw:
    JMP render

update_running:
    TST space_edge
    BEQ integrate_jump
    TST height
    BNE integrate_jump
    LDAA #7
    STAA velocity
integrate_jump:
    LDAA height
    ORAA velocity
    BEQ move_world
    LDAA height
    ADDA velocity
    BEQ landed
    BMI landed
    STAA height
    DEC velocity
    BRA move_world
landed:
    CLRA
    STAA height
    STAA velocity
move_world:
    INC animation
    LDAA ground_offset
    ADDA speed
    ANDA #15
    STAA ground_offset
    LDAA animation
    ANDA #3
    BNE move_obstacle
    DEC cloud_x
    LDAA cloud_x
    CMPA #240
    BNE move_obstacle
    LDAA #191
    STAA cloud_x
move_obstacle:
    TST gap_wait
    BEQ scroll_obstacle
    DEC gap_wait
    BNE world_moved
    LDAA #191
    STAA obstacle_x
    BRA world_moved
scroll_obstacle:
    LDAA obstacle_x
    SUBA speed
    STAA obstacle_x
    CMPA #192
    BCS world_moved
    CMPA #240
    BCC world_moved
    ; A nonzero 8-bit LFSR varies cactus shape and the next safe gap.
    LDAA random_state
    LSRA
    BCC random_ready
    EORA #$B8
random_ready:
    STAA random_state
    ANDA #1
    STAA obstacle_kind
    LDAA random_state
    ANDA #15
    ADDA #4
    STAA gap_wait
world_moved:
    JSR collision
    LDAA phase
    CMPA #1
    BEQ count_distance
    JMP render
count_distance:
    JSR advance_score
    JMP render

; Inset collision boxes: dino x=28..37, cactus x+3..x+11, y=42..55.
; The tail and empty sprite corners are intentionally forgiving.
collision:
    TST gap_wait
    BNE collision_done
    LDAA obstacle_x
    CMPA #17
    BCS collision_done
    CMPA #35
    BCC collision_done
    LDAA height
    CMPA #14
    BCC collision_done
    LDAA #2
    STAA phase
    JSR update_high_score
collision_done:
    RTS

advance_score:
    INC tick
    LDAA tick
    ANDA #3
    BNE score_done
    ; Saturate at 999; do not wrap the best score.
    LDAA distance
    CMPA #9
    BNE score_increment
    LDAA distance + 1
    CMPA #9
    BNE score_increment
    LDAA distance + 2
    CMPA #9
    BEQ score_done
score_increment:
    INC distance + 2
    LDAA distance + 2
    CMPA #10
    BNE choose_speed
    CLR distance + 2
    INC distance + 1
    LDAA distance + 1
    CMPA #10
    BNE choose_speed
    CLR distance + 1
    INC distance
choose_speed:
    LDAA distance
    BEQ score_done
    LDAA #4
    STAA speed
    LDAA distance
    CMPA #2
    BCS score_done
    LDAA #5
    STAA speed
score_done:
    RTS

update_high_score:
    LDAA distance
    CMPA high_score
    BHI save_high_score
    BCS high_score_done
    LDAA distance + 1
    CMPA high_score + 1
    BHI save_high_score
    BCS high_score_done
    LDAA distance + 2
    CMPA high_score + 2
    BLS high_score_done
save_high_score:
    LDAA distance
    STAA high_score
    LDAA distance + 1
    STAA high_score + 1
    LDAA distance + 2
    STAA high_score + 2
high_score_done:
    RTS

; Latch new presses around both CPU drawing and LCD transfer so short taps
; survive the Web host's 49,152-cycle minimum hold (E-404).
poll_space:
    LDAA $0FEF
    ANDA #1
    EORA #1
    BEQ key_sampled
    TST space_previous
    BNE key_sampled
    LDAB #1
    STAB space_pending
key_sampled:
    STAA space_previous
    RTS

render:
    JSR clear
    LDX #brand
    LDD #framebuffer + 3
    JSR text
    LDX #hi_label
    LDD #framebuffer + 96
    JSR text
    LDX #high_score
    LDD #framebuffer + 114
    JSR draw_number
    LDX #distance
    LDD #framebuffer + 168
    JSR draw_number
    JSR poll_space
    ; Ground line at y=56, sparse rolling gravel beneath it.
    LDX #framebuffer + 1344
    CLRB
ground_column:
    LDAA #1
    STAA 0,X
    PSHB
    ADDB ground_offset
    ANDB #15
    CMPB #2
    BNE gravel_second
    LDAA #9
    STAA 0,X
gravel_second:
    CMPB #9
    BNE ground_next
    LDAA #33
    STAA 0,X
ground_next:
    PULB
    INX
    INCB
    CMPB #192
    BNE ground_column
    LDAA phase
    CMPA #2
    BEQ cloud_done
    LDX #cloud
    LDAA cloud_x
    LDAB #13
    JSR blit
cloud_done:
    LDAA phase
    BEQ draw_dinosaur
    TST gap_wait
    BNE draw_dinosaur
    LDX #cactus_a
    TST obstacle_kind
    BEQ cactus_selected
    LDX #cactus_b
cactus_selected:
    LDAA obstacle_x
    LDAB #40
    JSR blit
draw_dinosaur:
    LDX #dino_air
    TST height
    BNE dino_selected
    LDX #dino_run_a
    LDAA animation
    ANDA #2
    BEQ dino_selected
    LDX #dino_run_b
dino_selected:
    LDAA phase
    CMPA #2
    BNE dino_alive
    LDX #dino_dead
dino_alive:
    LDAB #40
    SUBB height
    LDAA #24
    JSR blit
    LDAA phase
    CMPA #1
    BEQ draw_complete
    CMPA #2
    BEQ game_over_text
    LDX #start_label
    LDD #framebuffer + 576 + 54
    JSR text
    BRA draw_complete
game_over_text:
    LDX #over_label
    LDD #framebuffer + 384 + 69
    JSR text
    LDX #retry_label
    LDD #framebuffer + 768 + 54
    JSR text
draw_complete:
    JSR poll_space
    JSR present
    JSR poll_space
    JMP frame_ready

; X = three unpacked decimal digits; D = HUD destination.
draw_number:
    STX hud_digits
    STD hud_dest
    LDAA #3
    STAA digit_count
number_digit:
    LDX hud_digits
    LDAA 0,X
    INX
    STX hud_digits
    ADDA #48
    LDX hud_dest
    JSR glyph
    LDX hud_dest
    LDAB #6
    ABX
    STX hud_dest
    DEC digit_count
    BNE number_digit
    RTS

.section .bss, bss
phase: .space 1
height: .space 1
velocity: .space 1
obstacle_x: .space 1
obstacle_kind: .space 1
speed: .space 1
distance: .space 3
high_score: .space 3
tick: .space 1
space_previous: .space 1
space_pending: .space 1
space_edge: .space 1
animation: .space 1
gap_wait: .space 1
ground_offset: .space 1
cloud_x: .space 1
random_state: .space 1
digit_count: .space 1

.section .data, data
brand: .byte 74, 82, 32, 68, 73, 78, 79, 0
hi_label: .byte 72, 73, 0
start_label: .byte 83, 80, 65, 67, 69, 32, 84, 79, 32, 83, 84, 65, 82, 84, 0
over_label: .byte 71, 65, 77, 69, 32, 79, 86, 69, 82, 0
retry_label: .byte 83, 80, 65, 67, 69, 32, 84, 79, 32, 82, 69, 84, 82, 89, 0
