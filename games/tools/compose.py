# SPDX-License-Identifier: MIT
"""Select game-owned optional code before invoking the JR-800 assembler."""
import argparse
import re
from pathlib import Path
from puzzle_assets import PUZZLES, pack
from art import asm_bytes

def select(source, puzzle):
    active = True; stack = []; out = []
    for line in source.splitlines():
        marker = line.strip()
        if marker == '; @if puzzle':
            stack.append((active, puzzle)); active = active and puzzle
        elif marker == '; @else':
            assert stack, 'Unmatched feature else'
            parent, condition = stack[-1]; active = parent and not condition
        elif marker == '; @endif':
            assert stack, 'Unmatched feature end'
            active = stack.pop()[0]
        elif active: out.append(line)
    assert not stack, 'Unclosed feature selection'
    return '\n'.join(out) + '\n'

def compose(game, sources):
    puzzle = game in PUZZLES
    result = ''.join(select(Path(p).read_text(), puzzle) for p in sources)
    if puzzle:
        title = re.search(r'^title_art:\n((?:\s*\.byte[^\n]*\n)+)', result, re.M)
        assert title, 'Missing original title pixels'
        pixels = [int(v, 16) for v in re.findall(r'\$([0-9A-Fa-f]{2})', title[1])]
        assert len(pixels) == 1536
        result = result[:title.start()] + asm_bytes('title_art', pack(pixels)) + result[title.end():]
        result += Path(__file__).resolve().parents[1].joinpath('common/puzzle.s').read_text()
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('game'); parser.add_argument('output'); parser.add_argument('sources', nargs='+')
    args = parser.parse_args()
    Path(args.output).write_text(compose(args.game, args.sources))
