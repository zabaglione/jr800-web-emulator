# SPDX-License-Identifier: MIT
from hud_layouts import b, n, setup, status

SPEC = setup(32, 'graph', 'DOTS', [
    n('LEFT', b('grid_stat')),
    n('MOVES', b('moves')),
    n('YOU', b('dot_player_score')),
    n('CPU', b('dot_cpu_score')),
], status(b('dot_cpu_active'), ['YOU', 'CPU']))
