# SPDX-License-Identifier: MIT
"""A compact score panel and a pictorial route/bonus legend."""
from art import Bitmap
from visual_art import tiny
from hud_layouts import setup, status, c

# The common HUD supplies the stage counter; game code paints the legend.
SPEC = setup(0, 'panel', 'LOOP', [], status(c(0), ['']))


def panel_art():
    art = Bitmap()
    art.rect(0, 0, 192, 8, 1, True)
    tiny(art, 'LOOP TRACE', 6, 1, 0)
    tiny(art, 'STAGE', 150, 1, 0)
    art.line(128, 8, 128, 63)
    art.line(191, 8, 191, 63)
    for text, x, y in [('USED', 132, 9), ('PAR', 132, 17),
                       ('GATES', 132, 25), ('BONUS', 132, 49), ('/2', 178, 49)]:
        tiny(art, text, x, y)
    art.line(132, 23, 187, 23)
    # Short edge traces frame the board without resembling playable nodes.
    for x in (7, 120):
        art.line(x, 16, x, 47)
        for y in (12, 28, 44, 52):
            art.line(x - 2, y, x + 2, y)
    return art
