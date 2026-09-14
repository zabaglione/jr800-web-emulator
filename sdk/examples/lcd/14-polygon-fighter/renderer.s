; SPDX-License-Identifier: MIT
; Integer 3D pipeline: Q7 yaw/pitch, view-space lighting, painter sorting,
; and opaque scanline triangles with a 2x2 ordered monochrome dither.
; Shared LCD scratch is $80-$91. This renderer owns $92-$C7 (54 bytes).
.equ vx, $92
.equ vy, $93
.equ vz, $94
.equ rx, $95
.equ ry, $96
.equ rz, $97
.equ yaw_z, $98
.equ term, $99
.equ sin_yaw, $9A
.equ cos_yaw, $9B
.equ mul_sign, $9C
.equ source_ptr, $9D
.equ dest_ptr, $9F
.equ face_ptr, $A1
.equ record_ptr, $A3
.equ face_flags, $A6
.equ light, $A7
.equ depth_sum, $A8
.equ shade, $AA
.equ draw_count, $AB
.equ edge_input, $AC
.equ row_y, $AE
.equ min_y, $AF
.equ edge_x, $B0
.equ edge_y, $B1
.equ end_x, $B2
.equ end_y, $B3
.equ swap_point, $B4
.equ x_step, $B6
.equ edge_dx, $B7
.equ edge_dy, $B8
.equ edge_primary, $B9
.equ row_start, $BA
.equ max_y, $BC
.equ edge_count, $BD
.equ span_count, $BE
.equ row_mask, $BF
.equ clear_mask, $C0
.equ x_advance, $C1
.equ row_pattern, $C2
.equ left_x, $C3
.equ row_base, $C4
.equ pattern_base, $C6
.global render_fighter
.global projected_vertices
.global visible_count
.global face_records
.global scan_left
.global scan_right
.global state_end
.global view_valid
.global span_done
.global row_done
.extern angle
.extern vertices
.extern vertices_end
.extern faces
.extern faces_end
.extern sine
.extern framebuffer
.extern poll_controls
.extern dirty_min
.extern dirty_max
.extern dirty_pending
.extern span_bb
.extern span_bw
.extern span_wb
.extern span_ww
.section .text, code
render_fighter:
    ; Erase the previous bounds, then transfer the union of old/new bounds.
    TST view_valid
    BEQ empty_view
    JSR mark_view
    JSR clear_view
empty_view:
    LDAA #192
    STAA view_min_x
    LDAA #63
    STAA view_min_y
    CLR view_max_x
    CLR view_max_y
    LDX #sine
    LDAB angle
    ABX
    LDAA 0,X
    STAA sin_yaw
    LDAB angle
    ADDB #16
    ANDB #63
    LDX #sine
    ABX
    LDAA 0,X
    STAA cos_yaw
    LDX #vertices
    STX source_ptr
    LDX #projected_vertices
    STX dest_ptr
    JSR poll_controls
transform_vertex:
    LDX source_ptr
    LDD 0,X
    STD vx
    LDAA 2,X
    STAA vz
    LDAB #3
    ABX
    STX source_ptr
    JSR rotate
    LDX dest_ptr
    LDAA rx
    ADDA #96
    STAA 0,X
    CMPA view_min_x
    BCC vertex_min_x
    STAA view_min_x
vertex_min_x:
    CMPA view_max_x
    BLS vertex_max_x
    STAA view_max_x
vertex_max_x:
    LDAA #34
    SUBA ry
    STAA 1,X
    CMPA view_min_y
    BCC vertex_min_y
    STAA view_min_y
vertex_min_y:
    CMPA view_max_y
    BLS vertex_max_y
    STAA view_max_y
vertex_max_y:
    LDAA rz
    ADDA #128
    STAA 2,X
    LDAB #3
    ABX
    STX dest_ptr
    LDX source_ptr
    CPX #vertices_end
    BNE transform_vertex
    LDAA #1
    STAA view_valid
    JSR mark_view
    JSR prepare_faces
    JSR sort_faces
    LDX #face_records
    STX record_ptr
    LDAA visible_count
    STAA draw_count
    BEQ render_done
render_face:
    JSR poll_controls
    LDX record_ptr
    LDAA 4,X
    STAA shade
    LDX 2,X
    JSR load_triangle
    JSR fill_triangle
    LDD record_ptr
    ADDD #5
    STD record_ptr
    DEC draw_count
    BNE render_face
render_done:
    RTS

; Inclusive bounds are measured from the runtime-projected vertices.
mark_view:
    LDAA #1
    STAA dirty_pending
    LDAB view_min_y
    LSRB
    LSRB
    LSRB
    STAB edge_count
    LDAA view_max_y
    LSRA
    LSRA
    LSRA
    STAA max_y
mark_band:
    LDX #dirty_min
    ABX
    LDAA view_min_x
    CMPA 0,X
    BCC mark_right
    STAA 0,X
mark_right:
    LDAA view_max_x
    CMPA 8,X
    BLS mark_next
    STAA 8,X
mark_next:
    LDAB edge_count
    CMPB max_y
    BEQ mark_done
    INCB
    STAB edge_count
    BRA mark_band
mark_done:
    RTS

; Round horizontal erase bounds outward to 16-byte blocks. Bytes outside the
; previous geometry are already blank. Vertical bounds exclude both captions.
clear_view:
    LDAB view_min_y
    LSRB
    LSRB
    LSRB
    STAB edge_count
    LDAA #192
    MUL
    ADDD #framebuffer
    STD row_start
    LDAA view_min_x
    ANDA #$F0
    STAA left_x
    LDAA view_max_x
    SUBA left_x
    LSRA
    LSRA
    LSRA
    LSRA
    INCA
    STAA edge_dx
    LDD row_start
    ADDB left_x
    ADCA #0
    STD row_start
clear_band:
    LDAA edge_dx
    STAA span_count
    LDX row_start
    CLRA
    CLRB
clear_block:
    STD 0,X
    STD 2,X
    STD 4,X
    STD 6,X
    STD 8,X
    STD 10,X
    STD 12,X
    STD 14,X
    LDAB #16
    ABX
    CLRB
    DEC span_count
    BNE clear_block
    LDAA edge_count
    CMPA max_y
    BEQ clear_done
    INC edge_count
    LDD row_start
    ADDD #192
    STD row_start
    BRA clear_band
clear_done:
    RTS

; Signed A * signed B / 128, rounded toward negative infinity, returned in A.
; Products are bounded by the original model/normal range; X is preserved.
multiply:
    TSTB
    BPL multiply_positive
    NEGB
    TSTA
    BMI multiply_both_negative
    MUL
    COMA
    COMB
    ADDD #1
    ASLD
    RTS
multiply_both_negative:
    NEGA
    BRA multiply_unsigned
; Signed A times nonnegative B: only the product's high byte needs correction.
multiply_positive:
    TSTA
    BPL multiply_unsigned
    STAB mul_sign
    MUL
    SUBA mul_sign
    ASLD
    RTS
multiply_unsigned:
    MUL
    ASLD
    RTS

; Rotate a point or length-96 normal. View pitch is fixed at about 22.5 deg.
; Each product is Q7. Coordinates stay signed 8-bit throughout.
rotate:
    LDAA vx
    LDAB cos_yaw
    JSR multiply
    STAA term
    LDAA vz
    LDAB sin_yaw
    JSR multiply
    ADDA term
    STAA rx
    LDAA vx
    LDAB sin_yaw
    JSR multiply
    STAA term
    LDAA vz
    LDAB cos_yaw
    JSR multiply
    SUBA term
    STAA yaw_z
    LDAA yaw_z
    LDAB #49
    JSR multiply_positive
    STAA term
    LDAA vy
    LDAB #118
    JSR multiply_positive
    SUBA term
    STAA ry
    LDAA yaw_z
    LDAB #118
    JSR multiply_positive
    STAA term
    LDAA vy
    LDAB #49
    JSR multiply_positive
    ADDA term
    STAA rz
    RTS

prepare_faces:
    JSR poll_controls
    CLR visible_count
    LDX #face_records
    STX record_ptr
    LDX #faces
    STX face_ptr
prepare_face:
    LDX face_ptr
    LDAA 6,X
    STAA face_flags
    BITA #4
    BEQ calculate_normal
    ; The generator marks consecutive equal normal/material pairs only.
    ; Shade $FF means the preceding face was culled this frame.
    TST shade
    BPL reused_normal
    JMP culled_face
reused_normal:
    JMP record_face
calculate_normal:
    LDD 3,X
    STD vx
    LDAA 5,X
    STAA vz
    JSR rotate
    TST rz
    BGT front_face
    LDAA face_flags
    BITA #1
    BNE sheet_back
    JMP culled_face
sheet_back:
    TST rz
    BNE flip_sheet
    JMP culled_face
flip_sheet:
    NEG rx
    NEG ry
    NEG rz
front_face:
    ; A fixed view-space light (-48,88,72)/128. Brighter = fewer black dots.
    LDAA rx
    LDAB #$D0
    JSR multiply
    STAA light
    LDAA ry
    LDAB #88
    JSR multiply_positive
    ADDA light
    STAA light
    LDAA rz
    LDAB #72
    JSR multiply_positive
    ADDA light
    STAA light
    CLR shade
    TSTA
    BMI shade_ready
    LDAA #2
    STAA shade
    LDAA light
    CMPA #24
    BLT shade_ready
    LDAA #4
    STAA shade
    LDAA light
    CMPA #60
    BLT shade_ready
    LDAA #6
    STAA shade
shade_ready:
    LDAA face_flags
    BITA #2
    BEQ record_face
    LDAA #2
    STAA shade
record_face:
    ; Depth is the sum of the three vertex depths; no division is necessary.
    LDX face_ptr
    LDAB 0,X
    LDX #projected_vertices
    ABX
    LDAB 2,X
    CLRA
    STD depth_sum
    LDX face_ptr
    LDAB 1,X
    LDX #projected_vertices
    ABX
    LDAB 2,X
    CLRA
    ADDD depth_sum
    STD depth_sum
    LDX face_ptr
    LDAB 2,X
    LDX #projected_vertices
    ABX
    LDAB 2,X
    CLRA
    ADDD depth_sum
    STD depth_sum
    LDX record_ptr
    LDD depth_sum
    STD 0,X
    LDD face_ptr
    STD 2,X
    LDAA shade
    STAA 4,X
    LDAB #5
    ABX
    STX record_ptr
    INC visible_count
    BRA next_face
culled_face:
    LDAA #$FF
    STAA shade
next_face:
    LDX face_ptr
    LDAB #7
    ABX
    STX face_ptr
    CPX #faces_end
    BEQ faces_done
    JMP prepare_face
faces_done:
    RTS

; Stable ascending depth order: farther triangles first. Each record is
; depth16, face_pointer16, shade8. Equal depths retain the mesh's face order.
sort_faces:
    LDAA visible_count
    CMPA #2
    BCS sort_done
    DECA
    STAA sort_remaining
    LDX #face_records + 5
    STX record_ptr
sort_item:
    LDX record_ptr
    LDD 0,X
    STD sort_temp
    LDD 2,X
    STD sort_temp + 2
    LDAA 4,X
    STAA sort_temp + 4
sort_previous:
    CPX #face_records
    BEQ sort_insert
    XGDX
    SUBD #5
    XGDX
    LDD 0,X
    SUBD sort_temp
    BLS sort_after
    LDD 0,X
    STD 5,X
    LDD 2,X
    STD 7,X
    LDAA 4,X
    STAA 9,X
    BRA sort_previous
sort_after:
    LDAB #5
    ABX
sort_insert:
    LDD sort_temp
    STD 0,X
    LDD sort_temp + 2
    STD 2,X
    LDAA sort_temp + 4
    STAA 4,X
    LDD record_ptr
    ADDD #5
    STD record_ptr
    DEC sort_remaining
    BNE sort_item
sort_done:
    RTS

; X points to three vertex offsets in a face. Copy XY and close the triangle.
load_triangle:
    STX source_ptr
    LDAB 0,X
    LDX #projected_vertices
    ABX
    LDD 0,X
    STD triangle + 0
    LDX source_ptr
    LDAB 1,X
    LDX #projected_vertices
    ABX
    LDD 0,X
    STD triangle + 2
    LDX source_ptr
    LDAB 2,X
    LDX #projected_vertices
    ABX
    LDD 0,X
    STD triangle + 4
    LDD triangle
    STD triangle + 6
    RTS

fill_triangle:
    ; Sort three corners by Y. The long edge initializes every touched row;
    ; the two shorter edges then extend its bounds without clearing a table.
    LDX #triangle
    JSR sort_corners
    INX
    INX
    JSR sort_corners
    LDX #triangle
    JSR sort_corners
    LDD triangle
    STD triangle + 6
    LDAA triangle + 1
    STAA min_y
    LDAA triangle + 5
    STAA max_y
    LDAA #1
    STAA edge_primary
    LDX #triangle + 4
    JSR edge
    CLR edge_primary
    LDX #triangle
    STX edge_input
    LDAA #2
    STAA edge_count
trace_edge:
    LDX edge_input
    JSR edge
    LDD edge_input
    ADDD #2
    STD edge_input
    DEC edge_count
    BNE trace_edge
    LDAA min_y
    STAA row_y
    ANDA #1
    ASLA
    ASLA
    STAA row_pattern
    LDAB min_y
    LSRB
    LSRB
    LSRB
    ASLB
    LDX #band_addresses
    ABX
    LDD 0,X
    STD row_base
    LDAB min_y
    ANDB #7
    LDX #bit_masks
    ABX
    LDAA 0,X
    STAA row_mask
    COMA
    STAA clear_mask
    LDAB shade
    ASLB
    ASLB
    LDX #span_functions
    ABX
    STX pattern_base
fill_row:
    LDAB row_y
    LDX #scan_left
    ABX
    LDAA 0,X
    STAA left_x
    LDAA 64,X
    SUBA left_x
    INCA
    STAA span_count
    ; Advance the band address and bit mask only at the end of each row.
    LDD row_base
    ADDB left_x
    ADCA #0
    STD row_start
    LDAB span_count
    CMPB #2
    BHI patterned_row
    ; One or two pixels are entirely contour: avoid dither/span setup.
    LDX row_start
    DECB
    BEQ single_pixel
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    JMP row_done
single_pixel:
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
    JMP row_done
patterned_row:
    LDAA row_y
    CMPA min_y
    BEQ solid_row
    CMPA max_y
    BEQ solid_row
    LDAB left_x
    ANDB #1
    ASLB
    ADDB row_pattern
    LDX pattern_base
    ABX
    LDX 0,X
    BRA span_begin
solid_row:
    LDX #span_bb
span_begin:
    STX edge_input
    LDAA span_count
    ANDA #7
    STAA edge_count
    LDAA span_count
    LSRA
    LSRA
    LSRA
    STAA span_count
    LDX edge_input
    JMP 0,X
span_done:
    ; Dark contour on every triangle makes the facets legible at 192x64.
    DEX
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
    LDX row_start
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
row_done:
    LDAA row_y
    CMPA max_y
    BEQ triangle_done
    INC row_y
    LDAA row_pattern
    EORA #4
    STAA row_pattern
    ASL row_mask
    BNE same_band
    INC row_mask
    LDD row_base
    ADDD #192
    STD row_base
same_band:
    LDAA row_mask
    COMA
    STAA clear_mask
    JMP fill_row
triangle_done:
    RTS
sort_corners:
    LDAA 1,X
    CMPA 3,X
    BLS corners_sorted
    LDD 0,X
    STD swap_point
    LDD 2,X
    STD 0,X
    LDD swap_point
    STD 2,X
corners_sorted:
    RTS

; Walk integer edge intersections from smaller Y to larger Y, including both
; endpoints. The same edge always produces the same X samples in either order.
; Mesh projection bounds guarantee 0<=x<192, 8<=y<56, dx+dy<256.
edge:
    LDD 0,X
    STD edge_x
    LDD 2,X
    STD end_x
    LDAA edge_y
    CMPA end_y
    BLS edge_sorted
    LDD edge_x
    STD swap_point
    LDD end_x
    STD edge_x
    LDD swap_point
    STD end_x
edge_sorted:
    LDAA end_y
    SUBA edge_y
    STAA edge_dy
    BNE edge_sloped
    TST edge_primary
    BEQ horizontal_ready
    LDAB edge_y
    LDX #scan_left
    ABX
    LDAA edge_x
    STAA 0,X
    STAA 64,X
horizontal_ready:
    JSR edge_record
    LDAA end_x
    STAA edge_x
    JMP edge_record
edge_sloped:
    LDAA #1
    STAA x_step
    LDAA end_x
    SUBA edge_x
    BCC edge_positive
    NEGA
    LDAB #$FF
    STAB x_step
edge_positive:
    ; Divide once per edge. The row loop adds the quotient and needs at most
    ; one remainder correction, instead of stepping once per horizontal pixel.
    CLR x_advance
edge_quotient:
    CMPA edge_dy
    BCS edge_remainder
    SUBA edge_dy
    INC x_advance
    BRA edge_quotient
edge_remainder:
    STAA edge_dx
    LDAB x_step
    CMPB #1
    BEQ edge_advance_ready
    NEG x_advance
edge_advance_ready:
    ; X walks the scanline table, B holds X intersection, A holds remainder.
    ; The point swap temporary is no longer needed and stores the final row.
    CLRA
    LDAB end_y
    ADDD #scan_left
    STD swap_point
    LDAB edge_y
    LDX #scan_left
    ABX
    LDAB edge_x
    CLRA
    TST edge_primary
    BEQ edge_row
primary_row:
    STAB 0,X
    STAB 64,X
    CPX swap_point
    BEQ edge_done
    ADDB x_advance
    ADDA edge_dx
    CMPA edge_dy
    BCS primary_next
    SUBA edge_dy
    ADDB x_step
primary_next:
    INX
    BRA primary_row
edge_row:
    CMPB 0,X
    BCC edge_row_right
    STAB 0,X
edge_row_right:
    CMPB 64,X
    BLS edge_row_recorded
    STAB 64,X
edge_row_recorded:
    CPX swap_point
    BEQ edge_done
    ADDB x_advance
    ADDA edge_dx
    CMPA edge_dy
    BCS edge_next_row
    SUBA edge_dy
    ADDB x_step
edge_next_row:
    INX
    BRA edge_row
edge_done:
    RTS
edge_record:
    LDAB edge_y
    LDX #scan_left
    ABX
    LDAA edge_x
    CMPA 0,X
    BCC edge_right
    STAA 0,X
edge_right:
    CMPA 64,X
    BLS edge_recorded
    STAA 64,X
edge_recorded:
    RTS

.section .data, data
bit_masks: .byte 1,2,4,8,16,32,64,128
; For each shade: even-row/even-X, even-row/odd-X, odd-row/even-X,
; odd-row/odd-X. B/W select setting/clearing the current vertical LCD bit.
span_functions:
    .word span_bb, span_bb, span_bb, span_bb
    .word span_bb, span_bb, span_bw, span_wb
    .word span_bw, span_wb, span_wb, span_bw
    .word span_bw, span_wb, span_ww, span_ww
band_addresses:
    .word framebuffer, framebuffer+192, framebuffer+384, framebuffer+576
    .word framebuffer+768, framebuffer+960, framebuffer+1152, framebuffer+1344
.section .bss, bss
projected_vertices: .space 60
visible_count: .space 1
face_records: .space 115
triangle: .space 8
scan_left: .space 64
scan_right: .space 64
sort_remaining: .space 1
sort_temp: .space 5
view_valid: .space 1
view_min_x: .space 1
view_max_x: .space 1
view_min_y: .space 1
view_max_y: .space 1
state_end:
