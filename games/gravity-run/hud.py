# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS,t,flag
SPEC=deepcopy(SPECS['gravity-run'])
SPEC['fields'].append(t('',flag('gravity_banner'),['RUN','CLEAR']))
