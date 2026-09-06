; SPDX-License-Identifier: MIT
; Software PCG runner using the existing JR-800 LCD and keyboard models.
; SPACE jumps; releasing SPACE early limits ascent. No ROM routines are called.
.global entry
.global frame_ready
.global phase
.global height
.global velocity
.global speed
.global distance
.global score
.global high_score
.global tick
.global animation
.global obstacles
.global random_state
.global space_previous
.global space_pending
.extern init
.extern clear
.extern present_begin
.extern present_next
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
.extern rock
.extern crate
.equ hud_dest, $A4
.equ hud_digits, $A6
.equ object_ptr, $A8

.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR init
    CLRA
    STAA phase
    STAA space_previous
    STAA space_pending
    LDX #high_score
    LDAB #5
clear_best:
    STAA 0,X
    INX
    DECB
    BNE clear_best
    LDAA #$A7
    STAA random_state
    JSR reset_run
    JMP render

reset_run:
    LDX #run_state
    LDAB #24 ; run_state through the three obstacle records
    CLRA
clear_run:
    STAA 0,X
    INX
    DECB
    BNE clear_run
    LDAA #3
    STAA speed
    LDAA #130
    STAA cloud_x
    ; Each record is signed 16-bit X followed by kind.
    LDD #192
    STD obstacles
    LDD #280
    STD obstacles + 3
    LDD #368
    STD obstacles + 6
    LDAA #3
    STAA obstacles + 2
    LDAA #2
    STAA obstacles + 5
    LDAA #4
    STAA obstacles + 8
    RTS

frame_ready:
    LDX #2000
wait_frame:
    DEX
    BNE wait_frame
    JSR poll_keys
    LDAA space_pending
    STAA space_edge
    CLR space_pending
    LDAA phase
    CMPA #1
    BEQ update_running
    JSR next_random
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
    BEQ adjust_ascent
    TST height
    BNE adjust_ascent
    LDAA #7
    STAA velocity
    BRA integrate_jump
adjust_ascent:
    TST space_previous
    BNE integrate_jump
    LDAA velocity
    BLE integrate_jump
    CMPA #3
    BLS integrate_jump
    ; Early release cuts upward velocity, but never restarts ascent.
    LDAA #3
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
    CLR height
    CLR velocity
move_world:
    INC animation
    LDAA ground_offset
    ADDA speed
    ANDA #15
    STAA ground_offset
    LDAA animation
    ANDA #3
    BNE move_objects
    DEC cloud_x
    LDAA cloud_x
    CMPA #240
    BNE move_objects
    LDAA #191
    STAA cloud_x
move_objects:
    LDX #obstacles
    JSR scroll_object
    LDX #obstacles + 3
    JSR scroll_object
    LDX #obstacles + 6
    JSR scroll_object
    LDX #obstacles
    JSR recycle_object
    LDX #obstacles + 3
    JSR recycle_object
    LDX #obstacles + 6
    JSR recycle_object
    LDX #obstacles
    JSR collision
    LDAA phase
    CMPA #1
    BNE running_done
    LDX #obstacles + 3
    JSR collision
    LDAA phase
    CMPA #1
    BNE running_done
    LDX #obstacles + 6
    JSR collision
    LDAA phase
    CMPA #1
    BNE running_done
count_distance:
    JSR advance_score
running_done:
    JMP render

scroll_object:
    LDD 0,X
    SUBB speed
    SBCA #0
    STD 0,X
    RTS

next_random:
    LDAA random_state
    LSRA
    BCC random_ready
    EORA #$B8
random_ready:
    STAA random_state
    RTS

recycle_object:
    STX object_ptr
    LDAA 0,X
    CMPA #$FF
    BNE recycle_done
    LDAA 1,X
    CMPA #224
    BHI recycle_done
    ; Pick one of eight weights: cactus A/B, rock, crate x3, pit x2.
    JSR next_random
    ANDA #7
    LDX #kind_table
    TAB
    ABX
    LDAA 0,X
    LDX object_ptr
    STAA 2,X
    JSR next_random
    ANDA #3
    ASLA
    ASLA
    ASLA
    ADDA #80
    STAA spawn_gap
    ; Follow the rightmost object. Some pits lead into a short rock approach.
    CLR spawn_x
    CLR spawn_x + 1
    LDAA #3
    STAA spawn_count
    LDX #obstacles
find_rightmost:
    LDD 0,X
    BMI rightmost_next
    SUBD spawn_x
    BLE rightmost_next
    LDD 0,X
    STD spawn_x
    LDAA 2,X
    STAA spawn_kind
rightmost_next:
    INX
    INX
    INX
    DEC spawn_count
    BNE find_rightmost
    LDAA spawn_kind
    CMPA #4
    BNE spawn_position
    LDAA random_state
    ANDA #1
    BEQ spawn_position
    LDX object_ptr
    LDAA #2
    STAA 2,X
    LDAA #56
    STAA spawn_gap
spawn_position:
    LDD spawn_x
    ADDB spawn_gap
    ADCA #0
    TSTA
    BNE spawn_store
    CMPB #192
    BCC spawn_store
    LDD #192
spawn_store:
    LDX object_ptr
    STD 0,X
recycle_done:
    RTS

collision:
    LDAA 0,X
    BNE collision_done
    LDAA 1,X
    LDAB 2,X
    CMPB #4
    BEQ pit_collision
    CMPA #17
    BCS collision_done
    CMPA #35
    BCC collision_done
    LDAA #7
    CMPB #3
    BEQ solid_collision
    LDAA #14
    CMPB #2
    BNE solid_collision
    LDAA #18
solid_collision:
    STAA clearance
    LDAA height
    CMPA clearance
    BCC collision_done
    BRA collision_fatal
pit_collision:
    ; Feet center x=32 must be above the open interval [X, X+24).
    CMPA #9
    BCS collision_done
    CMPA #33
    BCC collision_done
    TST height
    BNE collision_done
collision_fatal:
    LDAA #2
    STAA phase
    JMP update_high_score
collision_done:
    RTS

advance_score:
    INC tick
    LDAA tick
    ANDA #3
    BNE score_done
    JSR score_point
    LDAA distance
    CMPA #9
    BNE distance_increment
    LDAA distance + 1
    CMPA #9
    BNE distance_increment
    LDAA distance + 2
    CMPA #9
    BEQ score_done
distance_increment:
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

; Five decimal digits, saturating at 99999. Speed depends only on distance.
score_point:
    LDX #score
    LDAB #5
score_check:
    LDAA 0,X
    CMPA #9
    BNE score_add
    INX
    DECB
    BNE score_check
    RTS
score_add:
    LDX #score + 4
score_carry:
    INC 0,X
    LDAA 0,X
    CMPA #10
    BNE score_done
    CLR 0,X
    DEX
    BRA score_carry

update_high_score:
    LDX #score
    LDAB #5
compare_best:
    LDAA 0,X
    CMPA 18,X ; high_score follows score by 18 bytes
    BHI save_high_score
    BCS score_done
    INX
    DECB
    BNE compare_best
    RTS
save_high_score:
    LDX #score
    LDAB #5
copy_best:
    LDAA 0,X
    STAA 18,X
    INX
    DECB
    BNE copy_best
    RTS

; Latch new presses around both CPU drawing and LCD transfer so short taps
; survive the Web host's 49,152-cycle minimum hold (E-404).
poll_keys:
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
    LDX #speed_label
    LDD #framebuffer + 54
    JSR text
    LDAA speed
    ADDA #48
    LDX #framebuffer + 90
    JSR glyph
    LDX #hi_label
    LDD #framebuffer + 102
    JSR text
    LDAA #5
    STAA digit_count
    LDX #high_score
    LDD #framebuffer + 114
    JSR draw_number
    LDAA #5
    STAA digit_count
    LDX #score
    LDD #framebuffer + 156
    JSR draw_number
    JSR poll_keys
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
    TST phase
    BEQ draw_dinosaur
    LDX #obstacles
    JSR draw_object
    JSR poll_keys
    LDX #obstacles + 3
    JSR draw_object
    JSR poll_keys
    LDX #obstacles + 6
    JSR draw_object
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
    LDX #jump_help
    LDD #framebuffer + 768 + 60
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
    JSR poll_keys
    JSR present_begin
transfer_span:
    JSR present_next
    BEQ transfer_done
    JSR poll_keys
    BRA transfer_span
transfer_done:
    JSR poll_keys
    JMP frame_ready

; Draw one obstacle; 16-bit coordinates keep off-screen objects distinct
; from the negative tail of a tile. Pit pixels clear the ground, not an overlay.
draw_object:
    STX object_ptr
    LDAA 2,X
    CMPA #4
    BEQ draw_pit
    LDAA 0,X
    BEQ object_positive
    CMPA #255
    BNE object_done
    LDAA 1,X
    CMPA #241
    BCS object_done
    BRA object_visible
object_positive:
    LDAA 1,X
    CMPA #192
    BCC object_done
object_visible:
    STAA draw_x
    LDAB 2,X
    LDX #cactus_a
    TSTB
    BEQ object_selected
    LDX #cactus_b
    CMPB #1
    BEQ object_selected
    LDX #rock
    CMPB #2
    BEQ object_selected
    LDX #crate
object_selected:
    LDAA draw_x
    LDAB #40
    JMP blit
object_done:
    RTS

draw_pit:
    LDD 0,X
    SUBD #1
    STD pit_column
    CLR pit_index
pit_loop:
    LDAA pit_column
    BNE pit_next
    LDAB pit_column + 1
    CMPB #192
    BCC pit_next
    LDX #framebuffer + 1344
    ABX
    LDAA pit_index
    BEQ pit_edge
    CMPA #25
    BEQ pit_edge
    CLR 0,X
    BRA pit_next
pit_edge:
    LDAA #255
    STAA 0,X
pit_next:
    LDD pit_column
    ADDD #1
    STD pit_column
    INC pit_index
    LDAA pit_index
    CMPA #26
    BNE pit_loop
    RTS

; X = unpacked decimal digits; D = HUD destination; digit_count set by caller.
draw_number:
    STX hud_digits
    STD hud_dest
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
space_previous: .space 1
space_pending: .space 1
space_edge: .space 1
random_state: .space 1
run_state:
height: .space 1
velocity: .space 1
speed: .space 1
distance: .space 3
score: .space 5
tick: .space 1
animation: .space 1
ground_offset: .space 1
cloud_x: .space 1
obstacles: .space 9
run_end:
high_score: .space 5
digit_count: .space 1
clearance: .space 1
spawn_gap: .space 1
spawn_x: .space 2
spawn_count: .space 1
spawn_kind: .space 1
draw_x: .space 1
pit_column: .space 2
pit_index: .space 1

.section .data, data
brand: .byte 74, 82, 32, 68, 73, 78, 79, 0
hi_label: .byte 72, 73, 0
start_label: .byte 83, 80, 65, 67, 69, 32, 84, 79, 32, 83, 84, 65, 82, 84, 0
over_label: .byte 71, 65, 77, 69, 32, 79, 86, 69, 82, 0
retry_label: .byte 83, 80, 65, 67, 69, 32, 84, 79, 32, 82, 69, 84, 82, 89, 0
jump_help: .byte 72, 79, 76, 68, 32, 70, 79, 82, 32, 72, 73, 71, 72, 0
speed_label: .byte 83, 80, 69, 69, 68, 0
kind_table: .byte 0, 1, 2, 3, 3, 3, 4, 4
