; SPDX-License-Identifier: MIT
; Generated original convex mesh data. No frame images.
.section .data, data
.global p3_model_tetra
p3_model_tetra:
    .byte 4,4
    .word p3_model_tetra_vertices,p3_model_tetra_faces
p3_model_tetra_vertices:
    .byte 238,246,244
    .byte 18,246,244
    .byte 0,246,24
    .byte 0,20,0
p3_model_tetra_faces:
    .byte 0,1,2,0,160,0,255
    .byte 0,3,1,0,36,167,255
    .byte 1,3,2,81,32,40,255
    .byte 2,3,0,175,32,40,255
.global p3_model_octa
p3_model_octa:
    .byte 6,8
    .word p3_model_octa_vertices,p3_model_octa_faces
p3_model_octa_vertices:
    .byte 0,22,0
    .byte 0,234,0
    .byte 236,0,0
    .byte 20,0,0
    .byte 0,0,20
    .byte 0,0,236
p3_model_octa_faces:
    .byte 0,2,4,199,52,57,255
    .byte 0,4,3,57,52,57,255
    .byte 0,3,5,57,52,199,255
    .byte 0,5,2,199,52,199,255
    .byte 1,4,2,199,204,57,255
    .byte 1,3,4,57,204,57,255
    .byte 1,5,3,57,204,199,255
    .byte 1,2,5,199,204,199,255
.global p3_model_cube
p3_model_cube:
    .byte 8,12
    .word p3_model_cube_vertices,p3_model_cube_faces
p3_model_cube_vertices:
    .byte 240,240,240
    .byte 16,240,240
    .byte 16,16,240
    .byte 240,16,240
    .byte 240,240,16
    .byte 16,240,16
    .byte 16,16,16
    .byte 240,16,16
p3_model_cube_faces:
    .byte 0,2,1,0,0,160,255
    .byte 0,3,2,0,0,160,255
    .byte 4,6,7,0,0,96,255
    .byte 4,5,6,0,0,96,255
    .byte 0,5,4,0,160,0,255
    .byte 0,1,5,0,160,0,255
    .byte 3,6,2,0,96,0,255
    .byte 3,7,6,0,96,0,255
    .byte 0,7,3,160,0,0,255
    .byte 0,4,7,160,0,0,255
    .byte 1,6,5,96,0,0,255
    .byte 1,2,6,96,0,0,255
