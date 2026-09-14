; SPDX-License-Identifier: MIT
; Integer 3D pipeline: Q7 yaw/pitch, view-space lighting, painter sorting,
; and opaque scanline triangles with a 2x2 ordered monochrome dither.
; Shared LCD scratch is $80-$91. This renderer owns $92-$C3 (50 bytes).
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
.equ record_count, $A5
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
.equ edge_error, $B9
.equ row_start, $BA
.equ max_y, $BC
.equ edge_count, $BD
.equ span_count, $BE
.equ row_mask, $BF
.equ clear_mask, $C0
.equ draw_even, $C1
.equ draw_odd, $C2
.equ left_x, $C3
.global render_fighter
.global projected_vertices
.global visible_count
.global face_records
.global scan_left
.global scan_right
.global state_end
.extern angle
.extern vertices
.extern vertices_end
.extern faces
.extern faces_end
.extern sine
.extern framebuffer
.extern poll_controls
.section .text, code
render_fighter:
    ; Keep the header/footer. One framebuffer, no stored animation frames.
    LDX #framebuffer + 192
    CLRA
clear_view:
    STAA 0,X
    INX
    CPX #framebuffer + 1344
    BNE clear_view
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
transform_vertex:
    JSR poll_controls
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
    LDAA #34
    SUBA ry
    STAA 1,X
    LDAA rz
    ADDA #128
    STAA 2,X
    LDAB #3
    ABX
    STX dest_ptr
    LDX source_ptr
    CPX #vertices_end
    BNE transform_vertex
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

; Signed A * signed B / 128, rounded toward negative infinity, returned in A.
; Products are bounded by the original model/normal range; X is preserved.
multiply:
    CLR mul_sign
    TSTA
    BPL multiply_a
    NEGA
    INC mul_sign
multiply_a:
    TSTB
    BPL multiply_b
    NEGB
    INC mul_sign
multiply_b:
    MUL
    PSHB
    LDAB mul_sign
    BITB #1
    PULB
    BEQ multiply_shift
    COMA
    COMB
    ADDD #1
multiply_shift:
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
    JSR multiply
    STAA term
    LDAA vy
    LDAB #118
    JSR multiply
    SUBA term
    STAA ry
    LDAA yaw_z
    LDAB #118
    JSR multiply
    STAA term
    LDAA vy
    LDAB #49
    JSR multiply
    ADDA term
    STAA rz
    RTS

prepare_faces:
    CLR visible_count
    LDX #face_records
    STX record_ptr
    LDX #faces
    STX face_ptr
prepare_face:
    JSR poll_controls
    LDX face_ptr
    LDD 3,X
    STD vx
    LDAA 5,X
    STAA vz
    LDAA 6,X
    STAA face_flags
    JSR rotate
    TST rz
    BGT front_face
    LDAA face_flags
    BITA #1
    BNE sheet_back
    JMP next_face
sheet_back:
    TST rz
    BNE flip_sheet
    JMP next_face
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
    JSR multiply
    ADDA light
    STAA light
    LDAA rz
    LDAB #72
    JSR multiply
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
    CLRA
    CLRB
    STD depth_sum
    LDX face_ptr
    STX source_ptr
    LDAA #3
    STAA record_count
sum_depth:
    LDX source_ptr
    LDAB 0,X
    INX
    STX source_ptr
    LDX #projected_vertices
    ABX
    LDAB 2,X
    CLRA
    ADDD depth_sum
    STD depth_sum
    DEC record_count
    BNE sum_depth
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
sort_pass:
    CLR sort_changed
    LDAA visible_count
    DECA
    STAA sort_remaining
    LDX #face_records
sort_pair:
    LDD 0,X
    SUBD 5,X
    BLS sort_advance
    LDAB #5
    STAB sort_bytes
sort_swap:
    LDAA 0,X
    LDAB 5,X
    STAB 0,X
    STAA 5,X
    INX
    DEC sort_bytes
    BNE sort_swap
    INC sort_changed
    BRA sort_next
sort_advance:
    LDAB #5
    ABX
sort_next:
    DEC sort_remaining
    BNE sort_pair
    TST sort_changed
    BNE sort_pass
sort_done:
    RTS

; X points to three vertex offsets in a face. Copy XY and close the triangle.
load_triangle:
    STX source_ptr
    LDX #triangle
    STX dest_ptr
    LDAA #3
    STAA record_count
load_corner:
    LDX source_ptr
    LDAB 0,X
    INX
    STX source_ptr
    LDX #projected_vertices
    ABX
    LDD 0,X
    LDX dest_ptr
    STD 0,X
    INX
    INX
    STX dest_ptr
    DEC record_count
    BNE load_corner
    LDD triangle
    STD triangle + 6
    RTS

fill_triangle:
    LDX #scan_left
    LDAA #255
    CLRB
reset_edges:
    STAA 0,X
    STAB 64,X
    INX
    CPX #scan_right
    BNE reset_edges
    LDAA #63
    STAA min_y
    CLR max_y
    LDX #triangle
    STX edge_input
    LDAA #3
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
    ; Byte address = framebuffer + (y / 8)*192 + x.
    LDAB row_y
    LSRB
    LSRB
    LSRB
    LDAA #192
    MUL
    ADDD #framebuffer
    ADDB left_x
    ADCA #0
    STD row_start
    LDAB row_y
    ANDB #7
    LDX #bit_masks
    ABX
    LDAA 0,X
    STAA row_mask
    COMA
    STAA clear_mask
    CLR draw_even
    CLR draw_odd
    LDAB row_y
    ANDB #1
    ADDB shade
    LDX #patterns
    ABX
    LDAB 0,X
    BITB #1
    BEQ odd_color
    LDAA row_mask
    STAA draw_even
odd_color:
    BITB #2
    BEQ border_row
    LDAA row_mask
    STAA draw_odd
border_row:
    LDAA row_y
    CMPA min_y
    BEQ solid_row
    CMPA max_y
    BNE span_begin
solid_row:
    LDAA row_mask
    STAA draw_even
    STAA draw_odd
span_begin:
    LDX row_start
    LDAB span_count
    LDAA left_x
    BITA #1
    BNE span_odd
span_even:
    LDAA 0,X
    ANDA clear_mask
    ORAA draw_even
    STAA 0,X
    INX
    DECB
    BEQ span_done
span_odd:
    LDAA 0,X
    ANDA clear_mask
    ORAA draw_odd
    STAA 0,X
    INX
    DECB
    BNE span_even
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
    LDAA row_y
    CMPA max_y
    BEQ triangle_done
    INC row_y
    JMP fill_row
triangle_done:
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
    LDAA edge_y
    CMPA min_y
    BCC edge_max
    STAA min_y
edge_max:
    LDAA end_y
    CMPA max_y
    BLS edge_delta
    STAA max_y
edge_delta:
    LDAA end_y
    SUBA edge_y
    STAA edge_dy
    BNE edge_sloped
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
    STAA edge_dx
    CLR edge_error
edge_row:
    JSR edge_record
    LDAA edge_y
    CMPA end_y
    BEQ edge_done
    LDAA edge_error
    ADDA edge_dx
edge_step:
    CMPA edge_dy
    BCS edge_next_row
    SUBA edge_dy
    LDAB edge_x
    ADDB x_step
    STAB edge_x
    BRA edge_step
edge_next_row:
    STAA edge_error
    INC edge_y
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
; Pairs of even/odd row patterns. Bit 0 = even column, bit 1 = odd column.
patterns: .byte 3,3, 3,1, 1,2, 1,0
.section .bss, bss
projected_vertices: .space 60
visible_count: .space 1
face_records: .space 115
triangle: .space 8
scan_left: .space 64
scan_right: .space 64
sort_changed: .space 1
sort_remaining: .space 1
sort_bytes: .space 1
state_end:
