# SPDX-License-Identifier: MIT
"""Compile authored puzzle campaigns into small, independently verifiable records."""
import json
from pathlib import Path
from art import asm_bytes

PUZZLES = ('box-shift', 'mirror-link', 'lamp-grid', 'slide-nine', 'ice-route',
           'switch-maze', 'pipe-weave', 'number-rail', 'mine-field', 'loop-trace',
           'knight-tour', 'peg-rescue', 'step-strike', 'ricochet-ops', 'pocket-factory')
ALPHABET = '2346789ACDEFHJKM'
STAGES = 40
BONUS_LABELS = {'box-shift':'VISIT MARKS','mirror-link':'LIGHT MARKS','lamp-grid':'FLIP MARKS',
                'slide-nine':'STAMP TILE','ice-route':'VISIT MARKS','switch-maze':'VISIT MARKS',
                'pipe-weave':'SEAL ALL PIPES','number-rail':'MARKED CORNER','mine-field':'FLAG ALL MINES',
                'loop-trace':'VISIT MARKS','knight-tour':'FINISH ON MARK','peg-rescue':'LAST PEG ON MARK',
                'step-strike':'TAKE INTEL','ricochet-ops':'LIGHT MARKS','pocket-factory':'SHIP VIA MARKS'}

def pack(data):
    """Zero terminator; 1..127 literals; 128..255 copy 1..128 bytes at a byte distance."""
    data = list(data)
    out = []; literal = []; i = 0
    def flush():
        while literal:
            chunk = literal[:127]; del literal[:127]
            out.extend([len(chunk), *chunk])
    while i < len(data):
        count = 0; distance = 0
        for back in range(1,min(i,255)+1):
            n = 0
            while n < 128 and i+n < len(data) and data[i+n] == data[i+n-back]: n += 1
            if n > count: count, distance = n, back
        if count >= 3:
            flush(); out.extend([127 + count, distance]); i += count
        else:
            literal.append(data[i]); i += 1
            if len(literal) == 127: flush()
    flush(); out.append(0)
    assert unpack(out) == data
    return out

def unpack(data):
    out = []; i = 0
    while data[i]:
        n = data[i]; i += 1
        if n & 128:
            distance = data[i]; i += 1
            assert 0 < distance <= len(out)
            for _ in range(n - 127): out.append(out[-distance])
        else:
            out.extend(data[i:i+n]); i += n
    assert i == len(data) - 1
    return out

def campaign(root):
    data = json.loads(Path(root, 'challenges.json').read_text())
    assert data['game'] == Path(root).name and len(data['stages']) == STAGES
    return data['stages']

def level_records(records):
    s = 'level_ptrs: .word ' + ','.join(f'level_{i:02}' for i in range(len(records))) + '\n'
    for i, record in enumerate(records):
        s += asm_bytes(f'level_{i:02}', pack(record))
    return s

def challenge_data(root, stages, width=16, sx=1, sy=1, height=7):
    """Bonus positions use logical cells; artwork belongs to the game tiles."""
    game = Path(root).name
    header = (f'.equ PUZZLE_ID,{PUZZLES.index(game)}\n'
              '.equ SCORE_CHARS,25\n.equ QUICK_CHARS,5\n')
    par = []; targets = []; display = []; needs = []
    for stage in stages:
        assert 0 < stage['par'] <= 9999
        par.extend([stage['par'] >> 8, stage['par'] & 255])
        points = list(stage.get('bonus_cells', []))
        assert len(points) <= 2
        needs.append((1 << max(1, len(points))) - 1)
        points += [255] * (2 - len(points)); targets += points
        for cell in points:
            display.append(255 if cell == 255 else
                           ((7-height*sy)//2 + cell//width*sy)*16 +
                           (16-width*sx)//2 + cell%width*sx)
    labels = [stage.get('bonus_label', BONUS_LABELS[game]) for stage in stages]
    assert all(len(label)<=18 and label.isascii() for label in labels)
    unique = list(dict.fromkeys(labels))
    hints = '.section .text, code\nchallenge_hint:\n'
    if len(unique)==1:
        hints += '    LDX #challenge_hint_0\n    RTS\n.section .data, data\n'
    else:
        hints += ('    LDAB stage\n    LDX #challenge_hint_indices\n    ABX\n    LDAB 0,X\n'
                  '    ASLB\n    LDX #challenge_hint_pointers\n    ABX\n    LDX 0,X\n    RTS\n.section .data, data\n')
        hints += asm_bytes('challenge_hint_indices', [unique.index(label) for label in labels])
        hints += 'challenge_hint_pointers: .word '+','.join(f'challenge_hint_{i}' for i in range(len(unique)))+'\n'
    for i,label in enumerate(unique): hints += asm_bytes(f'challenge_hint_{i}', list(label.ljust(18).encode())+[0])
    return (header + asm_bytes('challenge_pars', par) +
            asm_bytes('challenge_targets', targets) +
            asm_bytes('challenge_tiles', display) + asm_bytes('challenge_needs', needs) + hints)
