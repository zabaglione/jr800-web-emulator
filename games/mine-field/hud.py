# SPDX-License-Identifier: MIT
"""A visible mode control in the existing seven-field dashboard."""
from copy import deepcopy
from hud_layouts import SPECS

SPEC=deepcopy(SPECS['mine-field'])
SPEC['status']['values']=['FLAG MODE: OFF','FLAG MODE: ON']
