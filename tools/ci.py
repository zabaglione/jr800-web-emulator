# SPDX-License-Identifier: MIT
"""Select CI checks by verified input fingerprints, then build/test/seal artifacts.

Receipts are written only after all selected checks pass. No Git diff base or
checkout timestamp is used: skipped/cancelled commits cannot hide an input.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from build_games import build_games, catalog
from build_web_bundle import build_bundle

PRESETS = ('native-debug', 'native-release', 'wasm-debug', 'wasm-release')
GAME_SUFFIXES = ('.j8a', '.sym', '.map', '.j8d')
ENGINE_ROOTS = ('core/', 'debugger/', 'formats/', 'isa/', 'disassembler/',
                'assembler/', 'linker/', 'runtime/', 'wasm/')
DOC_TOOLS = {'games/tools/wiki.py', 'games/tools/wiki_index.py',
             'games/tools/gallery.json', 'tools/sync_game_wiki.py'}
POLICY = {'CMakeLists.txt', 'CMakePresets.json', 'tests/CMakeLists.txt',
          'tools/ci.py', 'tools/build_games.py', 'tests/ci_selection_test.py'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(values):
    return digest(json.dumps(values, sort_keys=True, separators=(',', ':')).encode())


def source_files(root):
    result = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others',
                                      '--exclude-standard'], cwd=root)
    return {name: digest((root / name).read_bytes()) for name in
            sorted(set(result.decode().strip('\0').split('\0'))) if (root / name).is_file()}


def fingerprints(files, programs, preset):
    def subset(predicate):
        return {p: h for p, h in files.items() if predicate(p)}
    policy = subset(lambda p: p in POLICY or p.startswith('.github/workflows/'))
    engine = subset(lambda p: (p.startswith(ENGINE_ROOTS) and not p.endswith(('.md', '.png', '.svg'))) or
                    (p.startswith('tools/') and (p.endswith(('.cpp', '.hpp')) or p == 'tools/CMakeLists.txt')))
    # Unknown source paths invalidate every check. Documentation and artwork do
    # not, except for the explicitly tested ABI document and distributed notices.
    known = (*ENGINE_ROOTS, 'tools/', 'tests/', 'sdk/', 'games/', 'web/', '.github/',
             'docs/', 'evidence-private/', 'private-programs/')
    unknown = subset(lambda p: p not in POLICY and not p.startswith(known) and
                     not p.endswith(('.md', '.png', '.svg')) and
                     p not in {'.gitignore', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'AGENTS.md'})
    core = fingerprint([policy, engine, unknown, preset])
    sdk = subset(lambda p: p.startswith('sdk/') and not p.endswith(('.md', '.png', '.svg')))
    common = subset(lambda p: p.startswith(('games/common/', 'games/tools/')) and
                    p not in DOC_TOOLS and not p.endswith(('.md', '.png', '.svg')))
    native = preset.startswith('native')
    compiled_tests = subset(lambda p: p.startswith('tests/') and p.endswith(('.cpp', '.c', '.h', '.hpp')))
    build = fingerprint([core, compiled_tests if native else files.get('tests/game_replay_test.cpp')])
    shared = fingerprint([build, sdk if native else {}, subset(lambda p:
        (p.startswith('tests/') and p != 'tests/game_rules_driver.py' and
         (not native or not p.endswith(('.mjs', '.cjs')))) or
        (not native and (p.startswith('web/') or p in {'tools/build_web_bundle.py',
         'games/catalog.json', 'LICENSE', 'THIRD_PARTY_NOTICES.md'})) or
        (native and p == 'web/wasm-machine.mjs') or
        p == 'tools/validate_evidence_package.py' or p == 'docs/wasm/c-abi-v42.md')])
    game_tests = (files.get('tests/game_rules_test.cpp'), files.get('tests/game_rules_driver.py')) if native else (
        files.get('tests/game_replay_test.cpp'), files.get('web/wasm-machine.mjs'),
        files.get('web/basic-boot-profile.mjs'))
    games = {p['id']: fingerprint([core, sdk, common, game_tests, subset(lambda path:
        path.startswith('games/' + p['id'] + '/') and not path.endswith(('.md', '.png', '.svg')))])
        for p in programs if not (native and p['id'] == 'relic-dive-gfx')}
    bundle = fingerprint([build, games, {} if native else subset(lambda p: p.startswith('web/') or p in
        {'games/catalog.json', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'tools/build_web_bundle.py'})])
    return {'build': build, 'shared': shared, 'games': games, 'bundle': bundle}


def read_receipt(cache):
    try:
        receipt = json.loads((cache / 'verified.json').read_text())
        if receipt.get('version') == 1:
            return receipt
    except (OSError, ValueError):
        pass
    return {}


def valid_artifacts(cache, records):
    if not records:
        return False
    for name, expected in records.items():
        # Cache paths are always relative to the project's build directory.
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            return False
        file = cache / 'products' / path
        if file.is_symlink() or not file.is_file() or digest(file.read_bytes()) != expected:
            return False
    return True


def select(current, previous, cache, force=False):
    old = previous.get('inputs', {})
    artifacts = previous.get('artifacts', {})
    rebuild = force or old.get('build') != current['build'] or not valid_artifacts(cache, artifacts.get('build'))
    shared = force or rebuild or old.get('shared') != current['shared']
    games = [game for game, key in current['games'].items() if force or rebuild or
             old.get('games', {}).get(game) != key or
             not valid_artifacts(cache, artifacts.get('games', {}).get(game))]
    bundle = force or rebuild or bool(games) or old.get('bundle') != current['bundle']
    return {'build': rebuild, 'shared': shared, 'games': games, 'bundle': bundle,
            'work': rebuild or shared or bool(games) or bundle, 'inputs': current}


def run(root, *command):
    print('+ ' + ' '.join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], cwd=root, check=True)


def configure(root, preset):
    prefix = ['emcmake'] if preset.startswith('wasm') else []
    run(root, *prefix, 'cmake', '--preset', preset)


def restore(root, cache, previous, plan):
    groups = [] if plan['build'] else [previous['artifacts']['build']]
    groups += [previous['artifacts']['games'][game] for game in plan['inputs']['games']
               if game not in plan['games']]
    for records in groups:
        if not valid_artifacts(cache, records):
            raise ValueError('Verified cache changed after planning')
        for name in records:
            target = root / 'build' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(cache / 'products' / name, target)


def ctest(root, preset, games, shared):
    listing = json.loads(subprocess.check_output(
        ['ctest', '--preset', preset, '--show-only=json-v1'], cwd=root))
    available = {test['name'] for test in listing['tests']}
    prefix = ('game_', 'native_replay_') if preset.startswith('wasm') else ('game_rules_',)
    selected = {name for name in available if shared and not name.startswith(prefix)}
    for game in games:
        for pre in prefix:
            name = pre + game
            if name not in available:
                raise ValueError('Missing registered test: ' + name)
            selected.add(name)
    if selected:
        expression = '^(' + '|'.join(re.escape(n) for n in sorted(selected)) + ')$'
        run(root, 'ctest', '--preset', preset, '--parallel', '4', '--no-tests=error', '-R', expression)
    return sorted(selected)


def web_assets(root):
    # Share the canonical CMake asset allowlist; do not glob private/extra files.
    cmake = (root / 'wasm/CMakeLists.txt').read_text()
    match = re.search(r'set\(\s*JR800_WEB_ASSET_NAMES\s+(.*?)\)', cmake, re.S)
    if not match:
        raise ValueError('Missing Web asset allowlist')
    names = match[1].split()
    if any('/' in name or not re.fullmatch(r'[a-z0-9.-]+', name) for name in names):
        raise ValueError('Invalid Web asset name')
    return [root / 'web' / name for name in names] + [root / 'LICENSE', root / 'THIRD_PARTY_NOTICES.md']


def seal(root, cache, preset, plan, tests):
    native = preset.startswith('native')
    def record(paths):
        result = {}
        for source in paths:
            name = str(source.relative_to(root / 'build'))
            target = cache / 'products' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            result[name] = digest(source.read_bytes())
        if not result:
            raise ValueError('Cannot certify missing build artifacts')
        return result
    if native:
        binaries = [file for directory in ('tests', 'tools') for file in
                    (root / 'build' / preset / directory).iterdir() if file.is_file() and os.access(file, os.X_OK)]
    else:
        binaries = [root / 'build' / preset / 'web-module' / ('jr800_wasm.' + ext) for ext in ('mjs', 'wasm')]
        binaries += [root / 'build/native-release' / name for name in
                     ('tools/jr8as', 'tools/jr8ld', 'tests/game_replay_test')]
    artifacts = {'build': record(binaries), 'games': {}}
    for game in plan['inputs']['games']:
        artifacts['games'][game] = record([root / 'build/games' / game / (game + suffix) for suffix in GAME_SUFFIXES])
    receipt = {'version': 1, 'inputs': plan['inputs'], 'artifacts': artifacts,
               'tested': tests, 'commit': os.environ.get('GITHUB_SHA', 'local')}
    cache.mkdir(parents=True, exist_ok=True)
    temporary = cache / 'verified.tmp'
    temporary.write_text(json.dumps(receipt, indent=2) + '\n')
    temporary.replace(cache / 'verified.json')


def package(root, preset, games):
    module = root / 'build' / preset / 'web-module'
    module.mkdir(parents=True, exist_ok=True)
    assets = web_assets(root)
    for source in assets:
        shutil.copy2(source, module / source.name)
    build_games(root, module, games)
    bundle_files = [module / p.name for p in assets] + [module / 'program-catalog.json']
    bundle_files += [module / (p['id'] + '.j8a') for p in catalog(root)]
    bundle_files += [module / ('jr800_wasm.' + ext) for ext in ('mjs', 'wasm')]
    output = root / 'build' / preset / 'web'
    shutil.rmtree(output, ignore_errors=True)
    build_bundle(root / 'web/index.html', output, bundle_files)


def execute(root, cache, preset, plan):
    if not plan['work']:
        print('All applicable checks already passed for these inputs and artifacts.')
        return
    previous = read_receipt(cache)
    restore(root, cache, previous, plan)
    configure(root, preset)
    native = preset.startswith('native')
    if plan['build']:
        if native:
            run(root, 'cmake', '--build', '--preset', preset, '--parallel', '4')
        else:
            run(root, 'cmake', '--build', '--preset', preset, '--parallel', '4', '--target', 'jr800_wasm')
            configure(root, 'native-release')
            run(root, 'cmake', '--build', '--preset', 'native-release', '--parallel', '4',
                '--target', 'jr8as', 'jr8ld', 'game_replay_test')
    # Never let restored files or Make timestamps preserve a changed game.
    for game in plan['games']:
        shutil.rmtree(root / 'build/games' / game, ignore_errors=True)
    if native:
        # Each selected rule test builds only its own program using this preset.
        pass
    else:
        package(root, preset, plan['games'])
    tests = ctest(root, preset, plan['games'], plan['shared'])
    # Generators must reproduce committed inputs; never cache an untested revision.
    current = fingerprints(source_files(root), catalog(root), preset)
    if current != plan['inputs']:
        raise ValueError('Source inputs changed during CI; regenerate and commit generated sources')
    seal(root, cache, preset, plan, tests)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('plan', 'run', 'bundle'))
    parser.add_argument('--preset', choices=PRESETS, required=True)
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--github-output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    cache = root / 'build/ci-cache' / args.preset
    plan_path = root / 'build' / ('ci-plan-' + args.preset + '.json')
    if args.action == 'plan':
        current = fingerprints(source_files(root), catalog(root), args.preset)
        plan = select(current, read_receipt(cache), cache, args.force)
        plan_path.parent.mkdir(parents=True, exist_ok=True)
        plan_path.write_text(json.dumps(plan, indent=2) + '\n')
        print(json.dumps({key: value for key, value in plan.items() if key != 'inputs'}, indent=2))
        if args.github_output:
            with args.github_output.open('a') as output:
                for key in ('work', 'build', 'shared', 'bundle'):
                    output.write(f'{key}={str(plan[key]).lower()}\n')
        summary = os.environ.get('GITHUB_STEP_SUMMARY')
        if summary:
            with open(summary, 'a') as output:
                output.write(f'### {args.preset}\n\nEngine build: {plan["build"]}; shared checks: {plan["shared"]}.\n\n')
                output.write('Games: ' + (', '.join(plan['games']) or 'none (verified inputs unchanged)') + '\n\n')
    elif args.action == 'run':
        execute(root, cache, args.preset, json.loads(plan_path.read_text()))
    else:
        if args.preset != 'wasm-release':
            raise ValueError('Only Release bundles can be published')
        previous = read_receipt(cache)
        current = fingerprints(source_files(root), catalog(root), args.preset)
        plan = select(current, previous, cache)
        if plan['work']:
            raise ValueError('Cannot publish without matching successful checks')
        restore(root, cache, previous, plan)
        package(root, args.preset, [])
        revision = digest((root / 'build' / args.preset / 'web/index.html').read_bytes())
        if args.github_output:
            with args.github_output.open('a') as output:
                output.write('revision=' + revision + '\n')



if __name__ == '__main__':
    main()
