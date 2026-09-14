; SPDX-License-Identifier: MIT
; Integer 3D pipeline: Q7 yaw/pitch, view-space lighting, painter sorting,
; and opaque scanline triangles with a 2x2 ordered monochrome dither.
; Shared LCD scratch is $80-$91. This renderer owns $92-$D2 (65 bytes).
.equ vx, $92
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
.equ long_row, $C8
.equ short_row, $CA
.equ lower_row, $CC
.equ middle_y, $CE
.equ cache_ptr, $CF
.equ pitch_y, $D1
.equ pitch_z, $D2
.global render_fighter
.global projected_vertices
.global visible_count
.global face_records
.global view_valid
.global span_done
.global row_done
.extern angle
.extern vertices
.extern vertices_end
.extern faces
.extern faces_end
.extern edge_cache
.extern edge_cache_pointers
.extern edge_cache_pointers_end
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
    ; Retain the occupied range of each LCD band before erasing the old view.
    TST view_valid
    BEQ first_view
    JSR clear_previous
    BRA empty_view
first_view:
    LDX #edge_cache_pointers
reset_edge_cache:
    STX source_ptr
    LDX 0,X
    LDAA #$FF
    STAA 0,X
    LDX source_ptr
    INX
    INX
    CPX #edge_cache_pointers_end
    BNE reset_edge_cache
    LDX #occupied_min
    LDAB #8
reset_occupied:
    LDAA #192
    STAA 0,X
    CLR 8,X
    INX
    DECB
    BNE reset_occupied
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
    LDAA 0,X
    STAA vx
    LDAA 2,X
    STAA vz
    LDD 3,X
    STD pitch_y
    LDAB #5
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
    JMP measure_view

; The previous frame's nonzero byte bounds also cover pixels erased this frame.
; Clear only those ranges, rounded outward to 16-byte blocks. Neighboring bytes
; are already blank. Reset the saved ranges for measure_view to replace.
clear_previous:
    LDD #framebuffer + 192
    STD row_base
    LDAA #1
    STAA edge_count
previous_band:
    LDAB edge_count
    LDX #occupied_min
    ABX
    LDAA 0,X
    STAA edge_x
    LDAA 8,X
    STAA end_x
    LDAA #192
    STAA 0,X
    CLR 8,X
    LDX #dirty_min
    ABX
    LDAA edge_x
    CMPA 0,X
    BCC previous_right
    STAA 0,X
previous_right:
    LDAA end_x
    CMPA 8,X
    BLS previous_clear
    STAA 8,X
previous_clear:
    LDAA edge_x
    CMPA #192
    BEQ previous_next
    ANDA #$F0
    STAA left_x
    LDAA end_x
    SUBA left_x
    LSRA
    LSRA
    LSRA
    LSRA
    INCA
    STAA span_count
    LDX row_base
    LDAB left_x
    ABX
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
previous_next:
    LDD row_base
    ADDD #192
    STD row_base
    INC edge_count
    LDAA edge_count
    CMPA #7
    BEQ previous_done
    JMP previous_band
previous_done:
    RTS

; Scan inward from the projected bounds after drawing. Only first/last nonzero
; bytes are needed, not a second framebuffer or a comparison of every byte.
measure_view:
    LDAA #1
    STAA dirty_pending
    LDAA view_max_y
    LSRA
    LSRA
    LSRA
    STAA max_y
    LDAB view_min_y
    LSRB
    LSRB
    LSRB
    STAB edge_count
    LDAA #192
    MUL
    ADDD #framebuffer
    STD row_base
measure_band:
    LDAB view_max_x
    ANDB #$FE
    CLRA
    ADDD row_base
    STD swap_point
    LDX row_base
    LDAB view_min_x
    ANDB #$FE
    ABX
measure_left:
    LDD 0,X
    BNE left_pair
    INX
    INX
    CPX swap_point
    BLS measure_left
    BRA measure_next
left_pair:
    TSTA
    BNE measured_left
    INX
measured_left:
    XGDX
    SUBD row_base
    STAB edge_x
    LDX swap_point
measure_right:
    LDD 0,X
    BNE right_pair
    DEX
    DEX
    BRA measure_right
right_pair:
    TSTB
    BEQ measured_right
    INX
measured_right:
    XGDX
    SUBD row_base
    STAB end_x
    LDAB edge_count
    LDX #occupied_min
    ABX
    LDAA edge_x
    STAA 0,X
    LDAA end_x
    STAA 8,X
    LDX #dirty_min
    ABX
    LDAA edge_x
    CMPA 0,X
    BCC measured_max
    STAA 0,X
measured_max:
    LDAA end_x
    CMPA 8,X
    BLS measure_next
    STAA 8,X
measure_next:
    LDAA edge_count
    CMPA max_y
    BEQ measure_done
    INC edge_count
    LDD row_base
    ADDD #192
    STD row_base
    BRA measure_band
measure_done:
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
; Each product is Q7. Coordinates stay signed 8-bit throughout. The two Y
; products depend only on the fixed pitch and are supplied as model constants.
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
    LDAA pitch_y
    SUBA term
    STAA ry
    LDAA yaw_z
    LDAB #118
    JSR multiply_positive
    STAA term
    LDAA pitch_z
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
    LDAA 3,X
    STAA vx
    LDAA 5,X
    STAA vz
    LDD 7,X
    STD pitch_y
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
    LDAA face_flags
    BITA #2
    BEQ light_face
    ; Canopy shade is constant; its rotated normal still controls culling.
    LDAA #2
    STAA shade
    BRA record_face
light_face:
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
    LDAB #12
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

; Each sorted corner stores XY and the cache pointer of the opposite edge.
load_triangle:
    STX source_ptr
    LDAB 0,X
    LDX #projected_vertices
    ABX
    LDD 0,X
    STD triangle
    LDX source_ptr
    LDAB 9,X
    LDX #edge_cache_pointers
    ABX
    LDD 0,X
    STD triangle + 2
    LDX source_ptr
    LDAB 1,X
    LDX #projected_vertices
    ABX
    LDD 0,X
    STD triangle + 4
    LDX source_ptr
    LDAB 10,X
    LDX #edge_cache_pointers
    ABX
    LDD 0,X
    STD triangle + 6
    LDX source_ptr
    LDAB 2,X
    LDX #projected_vertices
    ABX
    LDD 0,X
    STD triangle + 8
    LDX source_ptr
    LDAB 11,X
    LDX #edge_cache_pointers
    ABX
    LDD 0,X
    STD triangle + 10
    RTS

fill_triangle:
    LDX #triangle
    JSR sort_corners
    LDX #triangle + 4
    JSR sort_corners
    LDX #triangle
    JSR sort_corners
    LDD triangle
    STD triangle + 12
    LDAA triangle + 1
    STAA min_y
    LDAA triangle + 5
    STAA middle_y
    LDAA triangle + 9
    STAA max_y
    CMPA min_y
    BNE cached_triangle
    ; A horizontal triangle needs both endpoints, not a one-valued edge cache.
    LDAA triangle
    STAA flat_left
    STAA flat_right
    LDAA triangle + 4
    JSR flat_corner
    LDAA triangle + 8
    JSR flat_corner
    LDD #flat_left
    SUBB min_y
    SBCA #0
    STD long_row
    ADDD #1
    STD short_row
    JMP trace_edge
cached_triangle:
    LDX triangle + 6
    STX cache_ptr
    LDX #triangle + 8
    JSR cached_edge
    STX long_row
    LDAA min_y
    CMPA middle_y
    BEQ flat_top
    LDX triangle + 10
    STX cache_ptr
    LDX #triangle
    JSR cached_edge
    STX short_row
    LDAA middle_y
    CMPA max_y
    BEQ trace_edge
    LDX triangle + 2
    STX cache_ptr
    LDX #triangle + 4
    JSR cached_edge
    STX lower_row
    BRA trace_edge
flat_top:
    LDX triangle + 2
    STX cache_ptr
    LDX #triangle + 4
    JSR cached_edge
    STX short_row
    LDAA max_y
    STAA middle_y
trace_edge:
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
    LDX long_row
    ABX
    LDAA 0,X
    STAA left_x
    LDX short_row
    ABX
    LDAB 0,X
    CBA
    BLS intersections_sorted
    STAB left_x
    TAB
intersections_sorted:
    SUBB left_x
    INCB
    STAB span_count
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
    CMPA middle_y
    BNE same_short_edge
    LDX lower_row
    STX short_row
same_short_edge:
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
    CMPA 5,X
    BLS corners_sorted
    LDD 0,X
    STD swap_point
    LDD 4,X
    STD 0,X
    LDD swap_point
    STD 4,X
    LDD 2,X
    STD swap_point
    LDD 6,X
    STD 2,X
    LDD swap_point
    STD 6,X
corners_sorted:
    RTS
flat_corner:
    CMPA flat_left
    BCC flat_max
    STAA flat_left
flat_max:
    CMPA flat_right
    BLS flat_done
    STAA flat_right
flat_done:
    RTS

; Cache one runtime-computed X intersection per row of a shared mesh edge.
; An angle tag permits adjacent faces to share a result. On a later revolution,
; an unchanged tag is also valid because that angle has identical projection.
; X points to two corners four bytes apart; cache_ptr selects its storage.
; Return an anchor such that anchor + screen Y addresses the intersection.
cached_edge:
    STX edge_input
    LDX cache_ptr
    LDAA angle
    CMPA 0,X
    BNE cache_miss
    LDX 1,X
    RTS
cache_miss:
    STAA 0,X
    LDX edge_input
    LDD 0,X
    STD edge_x
    LDD 4,X
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
    LDAA #1
    STAA x_step
    LDAA end_x
    SUBA edge_x
    BCC edge_positive
    NEGA
    LDAB #$FF
    STAB x_step
edge_positive:
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
    LDX cache_ptr
    LDD cache_ptr
    ADDD #3
    SUBB edge_y
    SBCA #0
    STD 1,X
    LDD cache_ptr
    ADDD #3
    ADDB edge_dy
    ADCA #0
    STD swap_point
    INX
    INX
    INX
    LDAB edge_x
    CLRA
cache_row:
    STAB 0,X
    CPX swap_point
    BEQ cache_done
    ADDB x_advance
    ADDA edge_dx
    CMPA edge_dy
    BCS cache_next
    SUBA edge_dy
    ADDB x_step
cache_next:
    INX
    BRA cache_row
cache_done:
    LDX cache_ptr
    LDX 1,X
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
triangle: .space 14
flat_left: .space 1
flat_right: .space 1
sort_remaining: .space 1
sort_temp: .space 5
view_valid: .space 1
view_min_x: .space 1
view_max_x: .space 1
view_min_y: .space 1
view_max_y: .space 1
occupied_min: .space 8
occupied_max: .space 8
