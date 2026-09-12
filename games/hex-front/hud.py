# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
SPEC=deepcopy(SPECS['hex-front'])
SPEC['status']['code']='CLRB\nTST hex_charge\nBEQ @used\nTST selection_active\nBEQ @done\nINCB\nBRA @done\n@used:\nLDAB #2\n@done:\nCLRA'
SPEC['status']['values']=['RELAY: OFF','RELAY: ON','RELAY: USED']
