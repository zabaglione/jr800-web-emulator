; SPDX-License-Identifier: MIT
; Small convex, opaque meshes. Caller draws separate objects back-to-front.
; Q7 yaw/pitch; orthographic projection; clipped inclusive scanline triangles.
; All workspace is relocatable BSS. No private CPU RAM or second framebuffer.
.global p3_init
.global p3_begin
.global p3_draw
.global p3_line
.global p3_x
.global p3_y
.global p3_yaw
.global p3_pitch
.global p3_scale
.global p3_mode
.global p3_clip_top
.global p3_clip_bottom
.global p3_status
.global p3_error
.global p3_visible_count
.global p3_projected
.global p3_state_end
.extern framebuffer
.extern dirty_min
.extern dirty_max
.extern dirty_pending
.extern p3_poll
.section .text, code
p3_init:
    LDX #p3_occupied
    LDAB #8
p3_init_bounds:
    LDAA #192
    STAA 0,X
    CLR 8,X
    INX
    DECB
    BNE p3_init_bounds
    LDAA #96
    STAA p3_x
    STAA p3_scale
    LDAA #32
    STAA p3_y
    LDAA #4
    STAA p3_pitch
    LDAA #63
    STAA p3_clip_bottom
    CLR p3_clip_top
    CLR p3_yaw
    CLR p3_mode
    CLR p3_error
    CLR p3_status
    RTS

; Erase the previous geometry's byte ranges and retain their dirty bounds.
; The caller redraws any background sharing those ranges each frame.
p3_begin:
    CLR p3_error
    CLR p3_status
    CLR p3_band
    LDD #framebuffer
    STD p3_row_base
p3_begin_band:
    JSR p3_poll
    LDAB p3_band
    LDX #p3_occupied
    ABX
    LDAA 0,X
    CMPA #192
    BEQ p3_begin_next
    STAA p3_left
    LDAA 8,X
    STAA p3_right
    SUBA p3_left
    INCA
    STAA p3_count
    ; Mark before resetting the old occupied range.
    JSR p3_mark
    LDX p3_row_base
    LDAB p3_left
    ABX
    CLRA
p3_erase:
    STAA 0,X
    INX
    DEC p3_count
    BNE p3_erase
p3_begin_next:
    LDAB p3_band
    LDX #p3_occupied
    ABX
    LDAA #192
    STAA 0,X
    CLR 8,X
    LDD p3_row_base
    ADDD #192
    STD p3_row_base
    INC p3_band
    LDAA p3_band
    CMPA #8
    BNE p3_begin_band
    RTS

; X -> count8,count8,vertices16,faces16. Vertices are signed XYZ (-24..24).
; Faces are i,j,k,nx,ny,nz,shade (255=lit; otherwise 0,2,4,6).
; Validate before writing pixels. A/status=0 success, 1 invalid arguments.
p3_draw:
    CLR p3_status
    CLR p3_visible_count
    LDAA 0,X
    CMPA #3
    BCC p3_count_min_ok
    JMP p3_bad
p3_count_min_ok:
    CMPA #8
    BLS p3_count_max_ok
    JMP p3_bad
p3_count_max_ok:
    STAA p3_vertex_count
    LDAA 1,X
    BEQ p3_bad_counts
    CMPA #12
    BLS p3_faces_ok
p3_bad_counts:
    JMP p3_bad
p3_faces_ok:
    STAA p3_face_count
    LDD 2,X
    STD p3_vertices
    LDD 4,X
    STD p3_faces
    JSR p3_validate_view
    TSTA
    BEQ p3_view_ok
    RTS
p3_view_ok:
    LDAA p3_scale
    BEQ p3_bad_scale
    CMPA #128
    BLS p3_scale_ok
p3_bad_scale:
    JMP p3_bad
p3_scale_ok:
    LDAA p3_x
    CMPA #192
    BCC p3_bad_scale
    LDAA p3_y
    CMPA #64
    BCC p3_bad_scale
    LDAA p3_mode
    CMPA #1
    BHI p3_bad_scale
    LDAA p3_vertex_count
    LDAB #3
    MUL
    STAB p3_count
    LDX p3_vertices
p3_validate_vertex:
    LDAA 0,X
    CMPA #24
    BGT p3_bad_scale
    CMPA #$E8
    BLT p3_bad_scale
    INX
    DEC p3_count
    BNE p3_validate_vertex
    LDAA p3_face_count
    STAA p3_remaining
    LDX p3_faces
p3_validate_face:
    LDAA 0,X
    CMPA p3_vertex_count
    BCC p3_bad_face
    LDAA 1,X
    CMPA p3_vertex_count
    BCC p3_bad_face
    LDAA 2,X
    CMPA p3_vertex_count
    BCC p3_bad_face
    LDAA 6,X
    CMPA #255
    BEQ p3_face_valid
    CMPA #6
    BHI p3_bad_face
    BITA #1
    BEQ p3_face_valid
p3_bad_face:
    JMP p3_bad
p3_face_valid:
    LDAB #7
    ABX
    DEC p3_remaining
    BNE p3_validate_face
    LDAA p3_yaw
    JSR p3_trig
    STAA p3_sy
    STAB p3_cy
    LDAA p3_pitch
    JSR p3_trig
    STAA p3_sp
    STAB p3_cp
    LDD p3_vertices
    STD p3_source
    LDD #p3_projected
    STD p3_destination
    LDAA p3_vertex_count
    STAA p3_remaining
p3_transform:
    JSR p3_poll
    LDX p3_source
    LDD 0,X
    STD p3_vx
    LDAA 2,X
    STAA p3_vz
    LDAB #3
    ABX
    STX p3_source
    JSR p3_rotate
    LDAA p3_rx
    LDAB p3_scale
    JSR p3_mul_unsigned
    TAB
    CLRA
    TSTB
    BPL p3_positive_x
    DECA
p3_positive_x:
    ADDB p3_x
    ADCA #0
    LDX p3_destination
    STD 0,X
    LDAA p3_ry
    LDAB p3_scale
    JSR p3_mul_unsigned
    STAA p3_term
    LDAA p3_y
    SUBA p3_term
    STAA 2,X
    LDAB #3
    ABX
    STX p3_destination
    DEC p3_remaining
    BNE p3_transform
    LDD p3_faces
    STD p3_face_ptr
    LDAA p3_face_count
    STAA p3_faces_left
p3_face:
    JSR p3_poll
    LDX p3_face_ptr
    LDD 3,X
    STD p3_vx
    LDAA 5,X
    STAA p3_vz
    JSR p3_rotate
    TST p3_rz
    BGT p3_front
    JMP p3_next_face
p3_front:
    INC p3_visible_count
    LDX p3_face_ptr
    LDAA 6,X
    CMPA #255
    BEQ p3_lighting
    STAA p3_shade
    BRA p3_triangle_load
p3_lighting:
    LDAA p3_rx
    LDAB #$D0
    JSR p3_mul
    STAA p3_light
    LDAA p3_ry
    LDAB #88
    JSR p3_mul_unsigned
    ADDA p3_light
    STAA p3_light
    LDAA p3_rz
    LDAB #72
    JSR p3_mul_unsigned
    ADDA p3_light
    CLR p3_shade
    TSTA
    BMI p3_triangle_load
    LDAB #2
    STAB p3_shade
    CMPA #24
    BLT p3_triangle_load
    LDAB #4
    STAB p3_shade
    CMPA #60
    BLT p3_triangle_load
    LDAB #6
    STAB p3_shade
p3_triangle_load:
    LDD p3_face_ptr
    STD p3_source
    LDD #p3_triangle
    STD p3_destination
    LDAA #3
    STAA p3_remaining
p3_corner_load:
    LDX p3_source
    LDAB 0,X
    INX
    STX p3_source
    LDAA #3
    MUL
    LDX #p3_projected
    ABX
    LDD 0,X
    STD p3_temp
    LDAA 2,X
    STAA p3_temp + 2
    LDX p3_destination
    LDD p3_temp
    STD 0,X
    LDAA p3_temp + 2
    STAA 2,X
    LDAB #3
    ABX
    STX p3_destination
    DEC p3_remaining
    BNE p3_corner_load
    LDD p3_triangle
    STD p3_triangle + 9
    LDAA p3_triangle + 2
    STAA p3_triangle + 11
    JSR p3_raster
p3_next_face:
    LDD p3_face_ptr
    ADDD #7
    STD p3_face_ptr
    DEC p3_faces_left
    BEQ p3_draw_done
    JMP p3_face
p3_draw_done:
    CLRA
    RTS
p3_bad:
    LDAA #1
    STAA p3_status
    STAA p3_error
    RTS
p3_validate_view:
    LDAA p3_clip_top
    ANDA #7
    BNE p3_bad
    LDAA p3_clip_bottom
    ANDA #7
    CMPA #7
    BNE p3_bad
    LDAA p3_clip_bottom
    CMPA #64
    BCC p3_bad
    CMPA p3_clip_top
    BCS p3_bad
    CLRA
    RTS

; Signed A * signed B / 128, rounding down. X is preserved.
p3_mul:
    TSTB
    BPL p3_mul_unsigned
    NEGB
    TSTA
    BMI p3_mul_both_negative
    MUL
    COMA
    COMB
    ADDD #1
    ASLD
    RTS
p3_mul_both_negative:
    NEGA
    BRA p3_mul_positive
p3_mul_unsigned:
    TSTA
    BPL p3_mul_positive
    STAB p3_sign_factor
    MUL
    SUBA p3_sign_factor
    ASLD
    RTS
p3_mul_positive:
    MUL
    ASLD
    RTS
p3_trig:
    ANDA #63
    STAA p3_term
    TAB
    LDX #p3_sine
    ABX
    LDAA 0,X
    STAA p3_trig_value
    LDAB p3_term
    ADDB #16
    ANDB #63
    LDX #p3_sine
    ABX
    LDAB 0,X
    LDAA p3_trig_value
    RTS
p3_rotate:
    LDAA p3_vx
    LDAB p3_cy
    JSR p3_mul
    STAA p3_term
    LDAA p3_vz
    LDAB p3_sy
    JSR p3_mul
    ADDA p3_term
    STAA p3_rx
    LDAA p3_vx
    LDAB p3_sy
    JSR p3_mul
    STAA p3_term
    LDAA p3_vz
    LDAB p3_cy
    JSR p3_mul
    SUBA p3_term
    STAA p3_yaw_z
    LDAB p3_sp
    JSR p3_mul
    STAA p3_term
    LDAA p3_vy
    LDAB p3_cp
    JSR p3_mul
    SUBA p3_term
    STAA p3_ry
    LDAA p3_vy
    LDAB p3_sp
    JSR p3_mul
    STAA p3_term
    LDAA p3_yaw_z
    LDAB p3_cp
    JSR p3_mul
    ADDA p3_term
    STAA p3_rz
    RTS

; X -> x0,y0,x1,y1, all inside 192x64. Viewport clipping still applies.
; Axis-aligned lines write whole LCD bytes; diagonals use triangle coverage.
p3_line:
    STX p3_source
    JSR p3_validate_view
    TSTA
    BNE p3_line_done
    LDX p3_source
    LDAA 0,X
    CMPA #192
    BCC p3_line_bad
    LDAA 2,X
    CMPA #192
    BCC p3_line_bad
    LDAA 1,X
    CMPA #64
    BCC p3_line_bad
    LDAA 3,X
    CMPA #64
    BCC p3_line_bad
    LDAA 0,X
    CMPA 2,X
    BEQ p3_line_axis
    LDAA 1,X
    CMPA 3,X
    BEQ p3_line_axis
    CLRA
    LDAB 0,X
    STD p3_triangle
    STD p3_triangle + 9
    LDAA 1,X
    STAA p3_triangle + 2
    STAA p3_triangle + 11
    CLRA
    LDAB 2,X
    STD p3_triangle + 3
    STD p3_triangle + 6
    LDAA 3,X
    STAA p3_triangle + 5
    STAA p3_triangle + 8
    CLR p3_shade
    JSR p3_raster
    CLR p3_status
    CLRA
p3_line_done:
    RTS
p3_line_bad:
    JMP p3_bad
p3_line_axis:
    LDAA 0,X
    LDAB 2,X
    CBA
    BLS p3_axis_x_sorted
    PSHA
    TBA
    PULB
p3_axis_x_sorted:
    STAA p3_left
    STAB p3_right
    LDAA 1,X
    LDAB 3,X
    CBA
    BLS p3_axis_y_sorted
    PSHA
    TBA
    PULB
p3_axis_y_sorted:
    CMPA p3_clip_bottom
    BHI p3_axis_outside
    CMPB p3_clip_top
    BCC p3_axis_inside
p3_axis_outside:
    CLR p3_status
    CLRA
    RTS
p3_axis_inside:
    CMPA p3_clip_top
    BCC p3_axis_top
    LDAA p3_clip_top
p3_axis_top:
    STAA p3_first_row
    LSRA
    LSRA
    LSRA
    STAA p3_band
    CMPB p3_clip_bottom
    BLS p3_axis_bottom
    LDAB p3_clip_bottom
p3_axis_bottom:
    STAB p3_last_row
    LSRB
    LSRB
    LSRB
    STAB p3_last_band
p3_axis_band:
    JSR p3_poll
    JSR p3_mark
    LDAA #255
    LDAB p3_first_row
    ANDB #7
    BEQ p3_axis_start_mask
p3_axis_shift_start:
    ASLA
    DECB
    BNE p3_axis_shift_start
p3_axis_start_mask:
    STAA p3_mask
    LDAB p3_band
    CMPB p3_last_band
    BNE p3_axis_write
    LDAA #255
    LDAB p3_last_row
    ANDB #7
    EORB #7
    BEQ p3_axis_end_mask
p3_axis_shift_end:
    LSRA
    DECB
    BNE p3_axis_shift_end
p3_axis_end_mask:
    ANDA p3_mask
    STAA p3_mask
p3_axis_write:
    LDAA p3_band
    LDAB #192
    MUL
    ADDD #framebuffer
    STD p3_span_start
    LDX p3_span_start
    LDAB p3_left
    ABX
    LDAA p3_right
    SUBA p3_left
    INCA
    STAA p3_count
p3_axis_pixel:
    LDAA 0,X
    ORAA p3_mask
    STAA 0,X
    INX
    DEC p3_count
    BNE p3_axis_pixel
    LDAA p3_band
    CMPA p3_last_band
    BEQ p3_axis_empty
    CLR p3_first_row
    INC p3_band
    BRA p3_axis_band
p3_axis_empty:
    CLR p3_status
    CLRA
    RTS

p3_raster:
    LDAA #127
    STAA p3_min_y
    LDAA #$80
    STAA p3_max_y
    LDX #p3_triangle
    LDAB #3
p3_extent:
    LDAA 2,X
    CMPA p3_min_y
    BGE p3_extent_max
    STAA p3_min_y
p3_extent_max:
    CMPA p3_max_y
    BLE p3_extent_next
    STAA p3_max_y
p3_extent_next:
    INX
    INX
    INX
    DECB
    BNE p3_extent
    LDAA p3_min_y
    CMPA p3_clip_bottom
    BGT p3_raster_empty
    CMPA p3_clip_top
    BGE p3_first_row_ok
    LDAA p3_clip_top
p3_first_row_ok:
    STAA p3_first_row
    LDAA p3_max_y
    CMPA p3_clip_top
    BLT p3_raster_empty
    CMPA p3_clip_bottom
    BLE p3_last_row_ok
    LDAA p3_clip_bottom
p3_last_row_ok:
    STAA p3_last_row
    LDAB p3_first_row
    STAB p3_row
    ASLB
    LDX #p3_row_min
    ABX
p3_reset_rows:
    LDD #$7FFF
    STD 0,X
    LDD #$8000
    STD 128,X
    INX
    INX
    LDAA p3_row
    INC p3_row
    CMPA p3_last_row
    BNE p3_reset_rows
    LDD #p3_triangle
    STD p3_edge_ptr
    LDAA #3
    STAA p3_edges_left
p3_trace:
    LDX p3_edge_ptr
    JSR p3_edge
    LDD p3_edge_ptr
    ADDD #3
    STD p3_edge_ptr
    DEC p3_edges_left
    BNE p3_trace
    LDAA p3_first_row
    STAA p3_row
    JMP p3_paint_row
p3_raster_empty:
    RTS

; Walk one edge using quotient/remainder. Intersections are
; x0 + sign(dx)*floor(abs(dx)*(y-y0)/dy), independent of clipping.
p3_edge:
    LDD 0,X
    STD p3_edge_x
    LDAA 2,X
    STAA p3_edge_y
    LDD 3,X
    STD p3_end_x
    LDAA 5,X
    STAA p3_end_y
    CMPA p3_edge_y
    BGE p3_edge_ordered
    LDD p3_edge_x
    STD p3_temp
    LDD p3_end_x
    STD p3_edge_x
    LDD p3_temp
    STD p3_end_x
    LDAA p3_edge_y
    LDAB p3_end_y
    STAB p3_edge_y
    STAA p3_end_y
p3_edge_ordered:
    LDAA p3_end_y
    SUBA p3_edge_y
    STAA p3_dy
    BNE p3_edge_sloped
    JSR p3_record_x
    LDD p3_end_x
    STD p3_edge_x
    JMP p3_record_x
p3_edge_sloped:
    CLR p3_negative
    LDD p3_end_x
    SUBD p3_edge_x
    BPL p3_dx_positive
    INC p3_negative
    COMA
    COMB
    ADDD #1
p3_dx_positive:
    CLR p3_step
p3_divide:
    CMPB p3_dy
    BCS p3_divided
    SUBB p3_dy
    INC p3_step
    BRA p3_divide
p3_divided:
    STAB p3_remainder
    CLR p3_error_term
p3_edge_row:
    JSR p3_record_x
    LDAA p3_edge_y
    CMPA p3_end_y
    BEQ p3_edge_done
    LDAA p3_error_term
    ADDA p3_remainder
    LDAB p3_step
    CMPA p3_dy
    BCS p3_error_ready
    SUBA p3_dy
    INCB
p3_error_ready:
    STAA p3_error_term
    CLRA
    TST p3_negative
    BEQ p3_step_add
    COMA
    COMB
    ADDD #1
p3_step_add:
    ADDD p3_edge_x
    STD p3_edge_x
    INC p3_edge_y
    BRA p3_edge_row
p3_edge_done:
    RTS
p3_record_x:
    LDAA p3_edge_y
    CMPA p3_clip_top
    BLT p3_record_done
    CMPA p3_clip_bottom
    BGT p3_record_done
    TAB
    ASLB
    LDX #p3_row_min
    ABX
    LDD p3_edge_x
    SUBD 0,X
    BGE p3_record_max
    LDD p3_edge_x
    STD 0,X
p3_record_max:
    LDD p3_edge_x
    SUBD 128,X
    BLE p3_record_done
    LDD p3_edge_x
    STD 128,X
p3_record_done:
    RTS

p3_paint_row:
    ; Poll once per row; a wide primitive cannot hide a short key press.
    JSR p3_poll
    LDAB p3_row
    ASLB
    LDX #p3_row_min
    ABX
    LDD 0,X
    TSTA
    BMI p3_left_zero
    BNE p3_skip_row
    CMPB #192
    BCC p3_skip_row
    BRA p3_left_ready
p3_left_zero:
    CLRB
p3_left_ready:
    STAB p3_left
    LDD 128,X
    TSTA
    BMI p3_skip_row
    BNE p3_right_limit
    CMPB #192
    BCS p3_right_ready
p3_right_limit:
    LDAB #191
p3_right_ready:
    STAB p3_right
    LDAA p3_row
    LSRA
    LSRA
    LSRA
    STAA p3_band
    JSR p3_mark
    LDAA p3_band
    LDAB #192
    MUL
    ADDD #framebuffer
    STD p3_row_base
    LDAB p3_row
    ANDB #7
    LDX #p3_masks
    ABX
    LDAA 0,X
    STAA p3_mask
    COMA
    STAA p3_inverse
    LDAA p3_right
    SUBA p3_left
    INCA
    STAA p3_count
    LDX p3_row_base
    LDAB p3_left
    ABX
    STX p3_span_start
    LDAA p3_row
    CMPA p3_min_y
    BEQ p3_black_span
    CMPA p3_max_y
    BEQ p3_black_span
    TST p3_mode
    BNE p3_outline_only
    JMP p3_dither_span
p3_skip_row:
    JMP p3_next_row
p3_black_span:
    LDAA 0,X
    ORAA p3_mask
    STAA 0,X
    INX
    DEC p3_count
    BNE p3_black_span
    JMP p3_next_row
p3_dither_span:
    LDAA p3_row
    ANDA #1
    ADDA p3_shade
    TAB
    LDX #p3_patterns
    ABX
    LDAA 0,X
    STAA p3_pattern
    CLR p3_ink_even
    CLR p3_ink_odd
    BITA #1
    BEQ p3_odd_pattern
    LDAB p3_mask
    STAB p3_ink_even
p3_odd_pattern:
    BITA #2
    BEQ p3_pattern_ready
    LDAB p3_mask
    STAB p3_ink_odd
p3_pattern_ready:
    LDAA p3_ink_even
    EORA p3_ink_odd
    STAA p3_toggle
    LDAA p3_ink_even
    LDAB p3_left
    BITB #1
    BEQ p3_ink_ready
    LDAA p3_ink_odd
p3_ink_ready:
    STAA p3_ink
    LDX p3_span_start
p3_fill_pixel:
    LDAA 0,X
    ANDA p3_inverse
    ORAA p3_ink
    STAA 0,X
    LDAA p3_ink
    EORA p3_toggle
    STAA p3_ink
    INX
    DEC p3_count
    BNE p3_fill_pixel
p3_outline_only:
    LDX p3_span_start
    LDAA 0,X
    ORAA p3_mask
    STAA 0,X
    LDX p3_row_base
    LDAB p3_right
    ABX
    LDAA 0,X
    ORAA p3_mask
    STAA 0,X
p3_next_row:
    LDAA p3_row
    CMPA p3_last_row
    BEQ p3_raster_done
    INC p3_row
    JMP p3_paint_row
p3_raster_done:
    RTS

; Register the current clipped span in both the old-frame and LCD ranges.
p3_mark:
    LDAA #1
    STAA dirty_pending
    LDAB p3_band
    LDX #p3_occupied
    ABX
    LDAA p3_left
    CMPA 0,X
    BCC p3_mark_occupied_max
    STAA 0,X
p3_mark_occupied_max:
    LDAA p3_right
    CMPA 8,X
    BLS p3_mark_dirty
    STAA 8,X
p3_mark_dirty:
    LDX #dirty_min
    ABX
    LDAA p3_left
    CMPA 0,X
    BCC p3_mark_dirty_max
    STAA 0,X
p3_mark_dirty_max:
    LDAA p3_right
    CMPA 8,X
    BLS p3_mark_done
    STAA 8,X
p3_mark_done:
    RTS

.section .data, data
p3_masks: .byte 1,2,4,8,16,32,64,128
p3_patterns: .byte 3,3,3,1,1,2,1,0
p3_sine:
    .byte 0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126
    .byte 127,126,125,122,117,112,106,98,90,81,71,60,49,37,25,12
    .byte 0,244,231,219,207,196,185,175,166,158,150,144,139,134,131,130
    .byte 129,130,131,134,139,144,150,158,166,175,185,196,207,219,231,244
.section .bss, bss
p3_x: .space 1
p3_y: .space 1
p3_yaw: .space 1
p3_pitch: .space 1
p3_scale: .space 1
p3_mode: .space 1
p3_clip_top: .space 1
p3_clip_bottom: .space 1
p3_status: .space 1
p3_error: .space 1
p3_visible_count: .space 1
p3_vertex_count: .space 1
p3_face_count: .space 1
p3_vertices: .space 2
p3_faces: .space 2
p3_source: .space 2
p3_destination: .space 2
p3_face_ptr: .space 2
p3_remaining: .space 1
p3_faces_left: .space 1
p3_vx: .space 1
p3_vy: .space 1
p3_vz: .space 1
p3_rx: .space 1
p3_ry: .space 1
p3_rz: .space 1
p3_yaw_z: .space 1
p3_sy: .space 1
p3_cy: .space 1
p3_sp: .space 1
p3_cp: .space 1
p3_term: .space 1
p3_trig_value: .space 1
p3_sign_factor: .space 1
p3_light: .space 1
p3_shade: .space 1
p3_temp: .space 3
p3_triangle: .space 12
p3_edge_ptr: .space 2
p3_edges_left: .space 1
p3_edge_x: .space 2
p3_end_x: .space 2
p3_edge_y: .space 1
p3_end_y: .space 1
p3_dy: .space 1
p3_negative: .space 1
p3_step: .space 1
p3_remainder: .space 1
p3_error_term: .space 1
p3_min_y: .space 1
p3_max_y: .space 1
p3_first_row: .space 1
p3_last_row: .space 1
p3_row: .space 1
p3_band: .space 1
p3_last_band: .space 1
p3_left: .space 1
p3_right: .space 1
p3_count: .space 1
p3_row_base: .space 2
p3_span_start: .space 2
p3_mask: .space 1
p3_inverse: .space 1
p3_pattern: .space 1
p3_ink: .space 1
p3_ink_even: .space 1
p3_ink_odd: .space 1
p3_toggle: .space 1
p3_occupied: .space 16
p3_projected: .space 24
p3_row_min: .space 128
p3_row_max: .space 128
p3_state_end:
