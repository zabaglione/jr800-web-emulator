; SPDX-License-Identifier: MIT
.global entry
.global frame_ready
.extern basic_save
.extern basic_check_break
.extern init
.extern clear
.extern present
.extern text
.extern glyph
.extern delay
.extern framebuffer
.extern keys_reset
.extern keys_latch
.extern keys_ack
.extern present_begin
.extern present_next
.extern sprite8
.extern sprite_width
.extern sprite_mode
.extern scroll_left
.extern sound_play
.extern sound_port
.global phase
.global lane
.global hull
.global energy
.global shield
.global score
.global sector
.global ticks
.global speed
.global objects
.global key_state
.global frame_counter
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    LDX #key_state
    JSR keys_reset
    CLR phase
    CLR frame_counter
    LDAA #8
    STAA sprite_width
    CLR sprite_mode
    LDAA #$EF
    STAA sound_port
    LDAA #$10
    STAA $0000
    LDX #framebuffer + 1344
    CLRB
stars:
    TBA
    ANDA #15
    BNE no_star
    LDAA #$10
    BRA store_star
no_star:
    CLRA
store_star:
    STAA 0,X
    INX
    INCB
    CMPB #192
    BNE stars
    JMP render
frame_ready:
    JSR basic_check_break
    LDAA #4
    STAA wait_count
wait:
    LDX #1000
wait_loop:
    DEX
    BNE wait_loop
    LDX #key_state
    JSR poll_controls
    DEC wait_count
    BNE wait
    JSR update
    LDX #key_state
    JSR keys_ack
    INC frame_counter
render:
    ; Leave the bottom star band intact; no second framebuffer.
    LDX #framebuffer
    CLRA
erase:
    STAA 0,X
    INX
    CPX #framebuffer + 1344
    BNE erase
    LDX #title
    LDD #framebuffer + 6
    JSR text
    LDX #hud
    LDD #framebuffer + 192 + 6
    JSR text
    LDAA phase
    BEQ title_screen
    LDAA hull
    ADDA #48
    LDX #framebuffer + 192 + 18
    JSR glyph
    LDAA energy
    ADDA #48
    LDX #framebuffer + 192 + 42
    JSR glyph
    LDAA sector
    ADDA #48
    LDX #framebuffer + 192 + 96
    JSR glyph
    LDAA score
    CLR tens
divide_score:
    CMPA #10
    BCS score_digits
    SUBA #10
    INC tens
    BRA divide_score
score_digits:
    STAA ones
    LDAA tens
    ADDA #48
    LDX #framebuffer + 150
    JSR glyph
    LDAA ones
    ADDA #48
    LDX #framebuffer + 156
    JSR glyph
    LDAA #48
    LDX #framebuffer + 162
    JSR glyph
    LDAA phase
    CMPA #1
    BNE outcome_check
    JSR draw_objects
    JSR draw_player
outcome_check:
    LDAA phase
    CMPA #2
    BEQ lost_screen
    CMPA #3
    BEQ won_screen
    BRA transfer_start
title_screen:
    LDX #instructions
    LDD #framebuffer + 576 + 18
    JSR text
    LDX #start_label
    LDD #framebuffer + 960 + 42
    JSR text
    BRA transfer_start
lost_screen:
    LDX #lost_label
    BRA outcome
won_screen:
    LDX #won_label
outcome:
    LDD #framebuffer + 576 + 24
    JSR text
    LDX #retry_label
    LDD #framebuffer + 960 + 42
    JSR text
transfer_start:
    JSR present_begin
transfer:
    LDX #key_state
    JSR poll_controls
    JSR present_next
    BNE transfer
    JMP frame_ready

; keypad 8 is on $0FFD bit 0; keypad 2/5 on $0FFE bits 2/5.
; Combine only assigned controls before the shared edge-latching routine.
; X points to key_state and remains valid throughout both row reads.
poll_controls:
    LDAA $0FFD
    COMA
    ANDA #$01
    STAA scan_mask
    LDAA $0FFE
    COMA
    ANDA #$24
    ORAA scan_mask
    JMP keys_latch

update:
    LDAA phase
    CMPA #1
    BEQ playing
    LDAA key_state + 1
    BITA #$20
    BNE start_game
    RTS
start_game:
    JMP new_game
playing:
    CLR effect_id
    TST shield
    BEQ input
    DEC shield
input:
    LDAA key_state + 1
    BITA #$01
    BEQ down
    TST lane
    BEQ activate
    DEC lane
    BRA activate
down:
    BITA #$04
    BEQ activate
    LDAB lane
    CMPB #2
    BEQ activate
    INC lane
activate:
    BITA #$20
    BEQ move_world
    TST energy
    BEQ move_world
    TST shield
    BNE move_world
    DEC energy
    LDAA #12
    STAA shield
move_world:
    LDX #framebuffer + 1344
    LDAA #1
    JSR scroll_left
    ; Supply a new star every sixteen updates instead of losing the horizon.
    LDAA frame_counter
    ANDA #15
    BNE move_objects
    LDAA #$10
    STAA framebuffer + 1535
move_objects:
    LDX #objects
    STX slot_ptr
    LDAA #6
    STAA slot_count
object_loop:
    LDX slot_ptr
    LDAA 0,X
    BEQ object_next
    CMPA speed
    BLS expire
    SUBA speed
    STAA 0,X
    CMPA #17
    BCS object_next
    CMPA #32
    BCC object_next
    LDAA 1,X
    CMPA lane
    BNE object_next
    JSR collide
    LDAA phase
    CMPA #1
    BNE play_effect
    BRA object_next
expire:
    CLR 0,X
object_next:
    LDX slot_ptr
    LDAB #3
    ABX
    STX slot_ptr
    DEC slot_count
    BNE object_loop
    DEC spawn_wait
    BNE advance
    JSR spawn
advance:
    INC ticks
    LDAA ticks
    CMPA #96
    BNE play_effect
    CLR ticks
    LDAA sector
    CMPA #3
    BEQ victory
    INC sector
    INC speed
    BRA play_effect
victory:
    LDAA #3
    STAA phase
    LDX #finish_tune
    JMP sound_play
play_effect:
    LDAA effect_id
    BEQ update_return
    LDX #pickup_se
    CMPA #1
    BEQ sound
    LDX #hit_se
sound:
    JMP sound_play
update_return:
    RTS

new_game:
    LDX #game_state
    CLRA
reset_state:
    STAA 0,X
    INX
    CPX #game_state_end
    BNE reset_state
    LDAA #1
    STAA phase
    STAA lane
    STAA sector
    LDAA #3
    STAA hull
    STAA energy
    STAA speed
    LDAA #8
    STAA spawn_wait
    RTS

collide:
    ; X selects a colliding three-byte object: x, lane, type.
    CLR 0,X
    LDAA 2,X
    BEQ rock
    CMPA #2
    BEQ repair
    LDAA score
    CMPA #99
    BEQ charge
    INC score
charge:
    LDAA energy
    CMPA #3
    BEQ pickup
    INC energy
    BRA pickup
repair:
    LDAA hull
    CMPA #3
    BEQ pickup
    INC hull
pickup:
    LDAA #1
    STAA effect_id
    RTS
rock:
    TST shield
    BNE collision_return
    DEC hull
    LDAA #2
    STAA effect_id
    LDAA #8
    STAA shield
    TST hull
    BNE collision_return
    LDAA #2
    STAA phase
collision_return:
    RTS

spawn:
    LDAA #12
    STAA spawn_wait
    LDX #course
    LDAB course_index
    ABX
    LDAA 0,X
    STAA spawn_code
    INC course_index
    LDAA course_index
    CMPA #24
    BNE choose_slot
    CLR course_index
choose_slot:
    LDAA next_slot
    LDAB #3
    MUL
    ADDD #objects
    STD slot_ptr
    LDX #course_lanes
    LDAB spawn_code
    ABX
    LDAA 0,X
    LDX slot_ptr
    STAA 1,X
    LDX #course_types
    LDAB spawn_code
    ABX
    LDAA 0,X
    LDX slot_ptr
    STAA 2,X
    LDAA #184
    STAA 0,X
    INC next_slot
    LDAA next_slot
    CMPA #6
    BNE spawn_return
    CLR next_slot
spawn_return:
    RTS

draw_objects:
    LDX #objects
    STX slot_ptr
    LDAA #6
    STAA slot_count
draw_next:
    LDX slot_ptr
    LDAA 0,X
    BEQ draw_skip
    STAA actor_x
    LDAB 1,X
    STAB actor_lane
    LDAB 2,X
    LDX #rock_image
    TSTB
    BEQ actor
    LDX #gem_image
    CMPB #1
    BEQ actor
    LDX #repair_image
actor:
    STX actor_image
    JSR draw_actor
draw_skip:
    LDX slot_ptr
    LDAB #3
    ABX
    STX slot_ptr
    DEC slot_count
    BNE draw_next
    RTS
draw_player:
    LDAA #24
    STAA actor_x
    LDAA lane
    STAA actor_lane
    LDX #ship_image
    TST shield
    BEQ player_image
    LDX #shield_image
player_image:
    STX actor_image
    JMP draw_actor
draw_actor:
    LDX #lane_y
    LDAB actor_lane
    ABX
    LDAB 0,X
    LDX actor_image
    LDAA actor_x
    JMP sprite8

.section .bss, bss
scan_mask: .space 1
key_state: .space 3
frame_counter: .space 1
phase: .space 1
game_state:
lane: .space 1
hull: .space 1
energy: .space 1
shield: .space 1
score: .space 1
sector: .space 1
ticks: .space 1
speed: .space 1
spawn_wait: .space 1
course_index: .space 1
next_slot: .space 1
objects: .space 18
game_state_end:
wait_count: .space 1
slot_ptr: .space 2
slot_count: .space 1
spawn_code: .space 1
actor_x: .space 1
actor_lane: .space 1
actor_image: .space 2
effect_id: .space 1
tens: .space 1
ones: .space 1
.section .data, data
title: .byte 83,84,65,82,32,67,79,85,82,73,69,82,0
hud: .byte 72,32,32,32,69,32,32,32,83,69,67,84,79,82,0
instructions: .byte 56,32,85,80,32,50,32,68,79,87,78,32,53,32,83,72,73,69,76,68,0
start_label: .byte 53,32,84,79,32,76,65,85,78,67,72,0
lost_label: .byte 83,72,73,80,32,76,79,83,84,0
won_label: .byte 68,69,76,73,86,69,82,89,32,67,79,77,80,76,69,84,69,0
retry_label: .byte 53,32,84,79,32,82,69,84,82,89,0
lane_y: .byte 16,32,48
; 0..2 rocks, 3..5 cargo, 6..8 repairs; column chosen independently.
course_lanes: .byte 0,1,2,0,1,2,0,1,2
course_types: .byte 0,0,0,1,1,1,2,2,2
course: .byte 4,0,2,1,3,2,0,5,1,2,7,0,1,5,0,2,4,1,0,3,2,1,6,2
ship_image: .byte $DB,$7E,$3C,$18,$18,$18,$18,$18
shield_image: .byte $7E,$99,$BD,$FF,$FF,$BD,$99,$7E
rock_image: .byte $3C,$7E,$E7,$DB,$BD,$E7,$7E,$3C
gem_image: .byte $18,$3C,$66,$C3,$C3,$66,$3C,$18
repair_image: .byte $00,$18,$18,$7E,$7E,$18,$18,$00
pickup_se: .word 172,8,115,8,0,0
hit_se: .word 700,12,0,0
finish_tune: .word 344,50,273,60,229,70,172,90,0,0
