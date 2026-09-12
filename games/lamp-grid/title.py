# SPDX-License-Identifier: MIT
"""Original LCD title plate, using the same lenses as the playable panel."""
from art import Bitmap
from visual_art import poly, tiny, word


def title_art(faces):
    art = Bitmap()
    art.rect(0, 0, 192, 64, 1, True)

    # A pale, bevelled lamp module inset into the dark circuit board.
    poly(art, [(8, 2), (89, 2), (95, 8), (95, 47),
               (89, 53), (8, 53), (2, 47), (2, 8)], 0, True)
    poly(art, [(9, 4), (88, 4), (93, 9), (93, 46),
               (88, 51), (9, 51), (4, 46), (4, 9)])
    for x in (9, 88):
        art.line(x - 1, 5, x + 1, 5)
        art.line(x - 1, 50, x + 1, 50)
    for row in range(5):
        for col in range(5):
            # A lit cross and its two jewels introduce the game's motifs.
            on = int(row == 2 or col == 2)
            target = int((row, col) in ((0, 0), (2, 2)))
            face = faces[target * 2 + on]
            for y, pixels in enumerate(face.p):
                for x, pixel in enumerate(pixels):
                    art.dot(9 + col * 16 + x, 8 + row * 8 + y, pixel)

    # Paired traces and terminal pads connect the module to the title plate.
    for y in (14, 38):
        art.line(0, y, 1, y, 0)
        art.line(96, y, 99, y, 0)
        art.line(99, y, 99, y + 6, 0)
    for x in range(13, 88, 5):
        art.line(x, 55, x + 2, 55, 0)
    for points in (
        [(6, 57), (15, 57), (19, 61), (76, 61), (80, 57), (91, 57)],
        [(24, 57), (28, 59), (64, 59), (66, 57)],
    ):
        for a, b in zip(points, points[1:]):
            art.line(*a, *b, 0)
    for x in (4, 93):
        art.rect(x - 1, 56, 3, 3, 0)

    # Solid lettering remains readable at the LCD's native resolution.
    word(art, 'LAMP', 103, 4, 4, 3, 'block', 0, 2)
    word(art, 'GRID', 103, 29, 4, 3, 'block', 0, 2)
    art.line(103, 27, 134, 27, 0)
    art.line(156, 27, 188, 27, 0)
    for x in (139, 145, 151):
        art.dot(x, 27, 0)

    # The sole instruction is a key-shaped start badge below the logo.
    poly(art, [(107, 54), (184, 54), (188, 58),
               (184, 62), (107, 62), (103, 58)], 0, True)
    tiny(art, 'SPACE TO START', 120, 56)
    return art
