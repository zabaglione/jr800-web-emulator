# SPDX-License-Identifier: MIT
"""Build author-owned RAM programs and a matching, digest-checked catalog."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def catalog(root):
    programs = json.loads((root / 'games/catalog.json').read_text())['programs']
    ids = [p['id'] for p in programs]
    if len(set(ids)) != len(ids) or any(not re.fullmatch(r'[a-z][a-z0-9-]{0,39}', i) for i in ids):
        raise ValueError('Invalid or duplicate program ID')
    return programs


def build_games(root, output, games=None):
    programs = catalog(root)
    ids = {p['id'] for p in programs}
    selected = ids if games is None else set(games)
    if not selected <= ids:
        raise ValueError('Unknown game selection')
    native = root / 'build/native-release'
    if selected and any(not (native / p).exists() for p in
                        ('tools/jr8as', 'tools/jr8ld', 'tests/game_replay_test')):
        subprocess.run(['cmake', '--preset', 'native-release'], cwd=root, check=True)
        subprocess.run(['cmake', '--build', '--preset', 'native-release', '--target',
                        'jr8as', 'jr8ld', 'game_replay_test'], cwd=root, check=True)
    output.mkdir(parents=True, exist_ok=True)
    for program in programs:
        ident = program['id']
        if ident in selected:
            subprocess.run(['make', '--no-print-directory', '-s', '-C',
                            str(root / 'games' / ident)], check=True)
        file = ident + '.j8a'
        built = root / 'build/games' / ident / file
        if not built.is_file():
            raise ValueError(f'Missing reusable game artifact: {ident}; build it first')
        data = built.read_bytes()
        target = output / file
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
        program.update(file=file, sha256=hashlib.sha256(data).hexdigest(),
                       source=f'https://github.com/zabaglione/jr800-web-emulator/tree/main/games/{ident}')
    encoded = json.dumps({'version': 1, 'programs': programs}, indent=2) + '\n'
    target = output / 'program-catalog.json'
    if not target.exists() or target.read_text() != encoded:
        target.write_text(encoded)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--games', nargs='*', help='Build only these games; reuse the others from build/games')
    args = parser.parse_args()
    build_games(Path(__file__).resolve().parents[1], args.output, args.games)
