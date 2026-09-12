# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS,w,t,flag
SPEC=deepcopy(SPECS['river-hop'])
SPEC['fields'][3]['code']=w('river_display_score')
SPEC['fields'].append(t('',flag('river_banner'),['','NEXT']))
SPEC['status']['values']=['SPACE: RESUME','HOP TO A HOME']
