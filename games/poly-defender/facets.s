; SPDX-License-Identifier: MIT
; Game-owned opaque polygon rasterizer. Geometry, never bitmap animation.
; Authored meshes stay within y=8..55; horizontal columns clip at the right edge.
; Column spans map directly to the LCD's vertical bytes. Four flat shades.
.global facet_reset
.global facet_begin
.global facet_mesh
.global facet_register
.global facet_left
.global facet_right
.global facet_top
.global facet_bottom
.global facet_double
.global facet_wire
.global facet_x
.global facet_y
.global facet_shade
.extern framebuffer
.extern dirty_min
.extern dirty_max
.extern dirty_pending
.extern input_poll
.section .facets, code
facet_reset:
    CLR facet_double
    CLR facet_wire
    LDX #facet_old_left
    LDAB #6
facet_reset_loop:
    LDAA #192
    STAA 0,X
    CLR 6,X
    INX
    DECB
    BNE facet_reset_loop
    RTS
facet_begin:
    CLR facet_band
facet_clear_band:
    JSR input_poll
    LDAB facet_band
    LDX #facet_old_left
    ABX
    LDAA 0,X
    CMPA #192
    BEQ facet_clear_next
    STAA facet_left
    LDAA 6,X
    STAA facet_right
    JSR facet_dirty
    LDAA facet_band
    INCA
    LDAB #192
    MUL
    ADDD #framebuffer
    LDX #facet_pointer
    STD 0,X
    LDX facet_pointer
    LDAB facet_left
    ABX
    LDAA facet_right
    SUBA facet_left
    INCA
    TAB
    CLRA
facet_clear_bytes:
    STAA 0,X
    INX
    DECB
    BNE facet_clear_bytes
facet_clear_next:
    INC facet_band
    LDAA facet_band
    CMPA #6
    BNE facet_clear_band
    JMP facet_reset
 ; X points to face count, then shade,left,width,minY,maxY and column bounds.
; Bounds are derived from the projected polygon edges by the model generator.
; No pixel colors are stored: shading and opaque composition run on the CPU.
facet_mesh:
    LDAA 0,X
    STAA facet_faces
    INX
    STX facet_source
facet_mesh_face:
    CLR facet_clip
    LDX facet_source
    LDAA 0,X
    TST facet_wire
    BEQ facet_shade_ready
    LDAA #3
facet_shade_ready:
    STAA facet_shade
    LDAA 1,X
    TST facet_double
    BEQ facet_mesh_x_size
    ASLA
facet_mesh_x_size:
    ADDA facet_x
    STAA facet_left
    STAA facet_column
    LDAA 2,X
    TST facet_double
    BEQ facet_mesh_width
    ASLA
facet_mesh_width:
    STAA facet_columns
    ADDA facet_left
    DECA
    CMPA #192
    BCS facet_mesh_right
    LDAA #191
facet_mesh_right:
    STAA facet_right
    LDAA 3,X
    JSR facet_scale_y
    STAA facet_top
    LDAA 4,X
    JSR facet_scale_y
    STAA facet_bottom
    LDAB #5
    ABX
    STX facet_source
    ; X now points to the columns, so recover the unscaled width from source-3.
    LDX facet_source
    DEX
    DEX
    DEX
    LDAB 0,X
    ASLB
    LDX facet_source
    ABX
    STX facet_next_face
    LDAA facet_left
    CMPA #192
    BCC facet_mesh_next
    LDAA facet_top
    CMPA #56
    BCC facet_mesh_next
    CMPA #8
    BCC facet_top_ready
    LDAA #8
    STAA facet_top
    INC facet_clip
facet_top_ready:
    LDAA facet_bottom
    CMPA #8
    BCS facet_mesh_next
    CMPA #56
    BCS facet_bottom_ready
    LDAA #55
    STAA facet_bottom
    INC facet_clip
facet_bottom_ready:
    JSR facet_register
    LDAA facet_right
    SUBA facet_left
    INCA
    STAA facet_columns
    CLR facet_index
    JSR facet_paint_column
facet_mesh_next:
    LDX facet_next_face
    STX facet_source
    DEC facet_faces
    BEQ facet_mesh_done
    JMP facet_mesh_face
facet_mesh_done:
    RTS
; A caller can register a procedural solid's box without touching its pixels.
facet_register:
    LDAA facet_top
    LSRA
    LSRA
    LSRA
    DECA
    STAA facet_band
    LDAA facet_bottom
    LSRA
    LSRA
    LSRA
    DECA
    STAA facet_last_band
facet_mark_loop:
    JSR facet_dirty
    LDAB facet_band
    LDX #facet_old_left
    ABX
    LDAA facet_left
    CMPA 0,X
    BCC facet_mark_right
    STAA 0,X
facet_mark_right:
    LDAA facet_right
    CMPA 6,X
    BLS facet_mark_next
    STAA 6,X
facet_mark_next:
    LDAA facet_band
    CMPA facet_last_band
    BEQ facet_register_done
    INC facet_band
    BRA facet_mark_loop
facet_register_done:
    RTS
facet_paint_column:
    LDAA facet_index
    ANDA #15
    BNE facet_paint_poll_done
    JSR input_poll
facet_paint_poll_done:
    LDX facet_source
    TST facet_double
    BNE facet_double_column
    LDAA 0,X
    ADDA facet_y
    STAA facet_top
    LDAA 1,X
    ADDA facet_y
    STAA facet_bottom
    BRA facet_advance_source
facet_double_column:
    LDAA 0,X
    ASLA
    ADDA facet_y
    STAA facet_top
    LDAA 1,X
    ASLA
    ADDA facet_y
    STAA facet_bottom
    LDAA facet_index
    BITA #1
    BEQ facet_source_ready
facet_advance_source:
    INX
    INX
    STX facet_source
facet_source_ready:
    TST facet_clip
    BEQ facet_vertical_ready
    LDAA facet_top
    CMPA #56
    BCS facet_column_top
    JMP facet_column_next
facet_column_top:
    CMPA #8
    BCC facet_column_bottom
    LDAA #8
    STAA facet_top
facet_column_bottom:
    LDAA facet_bottom
    CMPA #8
    BCC facet_column_limit
    JMP facet_column_next
facet_column_limit:
    CMPA #56
    BCS facet_vertical_ready
    LDAA #55
    STAA facet_bottom
facet_vertical_ready:
    LDAA facet_top
    LSRA
    LSRA
    LSRA
    STAA facet_band
    LDAA facet_bottom
    LSRA
    LSRA
    LSRA
    STAA facet_last_band
    LDAB facet_top
    ANDB #7
    LDX #facet_start_masks
    ABX
    LDAA 0,X
    STAA facet_mask
    LDAA #1
    STAA facet_first
    LDAA facet_shade
    ASLA
    STAA facet_color
    LDAA facet_column
    ANDA #1
    ADDA facet_color
    TAB
    LDX #facet_patterns
    ABX
    LDAA 0,X
    STAA facet_color
    LDAA facet_column
    CMPA facet_left
    BEQ facet_edge_column
    CMPA facet_right
    BNE facet_pointer_ready
facet_edge_column:
    LDAA #255
    STAA facet_color
facet_pointer_ready:
    LDAA facet_band
    LDAB #192
    MUL
    ADDD #framebuffer
    STD facet_pointer
    LDX facet_pointer
    LDAB facet_column
    ABX
    STX facet_pointer
facet_paint_band:
    LDAA facet_band
    CMPA facet_last_band
    BNE facet_mask_ready
    LDAB facet_bottom
    ANDB #7
    LDX #facet_end_masks
    ABX
    LDAA 0,X
    ANDA facet_mask
    STAA facet_mask
facet_mask_ready:
    LDAA facet_color
    STAA facet_ink
    CMPA #255
    BEQ facet_write
    TST facet_first
    BEQ facet_bottom_edge
    LDAB facet_top
    ANDB #7
    LDX #facet_bits
    ABX
    LDAA 0,X
    ORAA facet_ink
    STAA facet_ink
facet_bottom_edge:
    LDAA facet_band
    CMPA facet_last_band
    BNE facet_write
    LDAB facet_bottom
    ANDB #7
    LDX #facet_bits
    ABX
    LDAA 0,X
    ORAA facet_ink
    STAA facet_ink
facet_write:
    LDAA facet_ink
    ANDA facet_mask
    STAA facet_ink
    LDAA facet_mask
    COMA
    LDX facet_pointer
    ANDA 0,X
    ORAA facet_ink
    STAA 0,X
    LDAA facet_band
    CMPA facet_last_band
    BEQ facet_column_next
    LDD facet_pointer
    ADDD #192
    STD facet_pointer
    INC facet_band
    CLR facet_first
    LDAA #255
    STAA facet_mask
    JMP facet_paint_band
facet_column_next:
    INC facet_column
    INC facet_index
    DEC facet_columns
    BEQ facet_triangle_done
    JMP facet_paint_column
facet_triangle_done:
    RTS
facet_scale_y:
    TST facet_double
    BEQ facet_scale_y_add
    ASLA
facet_scale_y_add:
    ADDA facet_y
    RTS
facet_dirty:
    LDAB facet_band
    INCB
    LDX #dirty_min
    ABX
    LDAA facet_left
    CMPA 0,X
    BCC facet_dirty_right
    STAA 0,X
facet_dirty_right:
    LDAA facet_right
    CMPA 8,X
    BLS facet_dirty_done
    STAA 8,X
facet_dirty_done:
    LDAA #1
    STAA dirty_pending
    RTS
facet_start_masks: .byte 255,254,252,248,240,224,192,128
facet_end_masks: .byte 1,3,7,15,31,63,127,255
facet_bits: .byte 1,2,4,8,16,32,64,128
facet_patterns: .byte 255,255,85,170,85,0,0,0
.section .bss, bss
facet_old_left: .space 6
facet_old_right: .space 6
facet_x: .space 1
facet_y: .space 1
facet_shade: .space 1
facet_source: .space 2
facet_faces: .space 1
facet_left: .space 1
facet_right: .space 1
facet_top: .space 1
facet_bottom: .space 1
facet_band: .space 1
facet_last_band: .space 1
facet_pointer: .space 2
facet_columns: .space 1
facet_column: .space 1
facet_index: .space 1
facet_mask: .space 1
facet_ink: .space 1
facet_first: .space 1
facet_next_face: .space 2
facet_color: .space 1
facet_double: .space 1
facet_wire: .space 1
facet_clip: .space 1
