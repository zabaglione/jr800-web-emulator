#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate and emit the original five-vertex laser guardian."""
import argparse
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('poly_mesh_author', root / 'sdk/lib/poly3d/generate_models.py')
author = importlib.util.module_from_spec(spec)
spec.loader.exec_module(author)
author.MODELS = {'prism': {
    'vertices': [(-24, -10, -12), (24, -10, -12), (0, -10, 22), (0, 18, 0), (0, -22, 0)],
    'faces': [(0, 1, 3), (1, 2, 3), (2, 0, 3), (1, 0, 4), (2, 1, 4), (0, 2, 4)],
}}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
content = author.sources()['models.s']
path = Path(__file__).with_name('boss-model.s')
if args.check:
    if path.read_text() != content:
        raise SystemExit('Stale boss mesh')
else:
    path.write_text(content)
print('Verified original five-vertex guardian.' if args.check else 'Generated original guardian.')
