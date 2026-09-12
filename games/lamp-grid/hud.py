# SPDX-License-Identifier: MIT
from hud_layouts import b, w, n, setup, status

# Plain side rules leave the small count label clear of the frame decoration.
SPEC = setup(0, 'panel', 'LIGHTS', [
    n('LIT', b('grid_stat')),
    n('USED', w('challenge_moves'), 4),
    n('PAR', w('challenge_par'), 4),
], status(
    'LDAB lamp_view_bonus\nCMPB challenge_need\nBEQ @full\n'
    'CLRB\nBRA @done\n@full:\nLDAB #1\n@done:\nCLRA',
    ['BONUS: MISSING', 'BONUS: OK'],
))
