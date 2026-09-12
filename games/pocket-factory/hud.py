# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
SPEC=deepcopy(SPECS['pocket-factory'])
SPEC['fields'][4]['code']='LDAB tool\nASLB\nTST selection_active\nBEQ @done\nTST cursor_blink_mask\nBEQ @done\nINCB\n@done:\nCLRA'
SPEC['fields'][4]['values']=[prefix+name for name in ['RIGHT','DOWN','LEFT','UP','PRS A','PRS B','ERASE'] for prefix in [' ','>']]
SPEC['status']['values']=['SELECT TOOL','RUNNING','SPACE: SET']
