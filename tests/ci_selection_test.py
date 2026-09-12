# SPDX-License-Identifier: MIT
"""Regression checks for CI selection, reusable artifacts and per-game builds."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import ci
import build_games


class SelectionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.cache = Path(self.temp.name)
        self.programs = [{'id': 'box-shift'}, {'id': 'mirror-link'}, {'id': 'relic-dive-gfx'}]
        self.files = {p: 'initial' for p in (
            'core/src/cpu.cpp', 'sdk/lib/lcd/display.s', 'games/common/runtime.s',
            'games/tools/check.mjs', 'games/box-shift/main.s', 'games/mirror-link/main.s',
            'games/relic-dive-gfx/main.s', 'sdk/examples/lcd/07-relic-dive/main.s',
            'tests/game_rules_test.cpp', 'tests/game_replay_test.cpp',
            'tests/wasm_worker_vertical_slice.mjs', 'web/styles.css',
            'web/wasm-machine.mjs', 'games/catalog.json', 'tools/ci.py')}

    def plan(self, preset, changed=None, damage=None, force=False):
        before = ci.fingerprints(self.files, self.programs, preset)
        names = ['engine'] + list(before['games'])
        records = {}
        for name in names:
            path = self.cache / 'products' / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b'artifact')
            records[name] = {name: ci.digest(b'artifact')}
        if damage:
            (self.cache / 'products' / damage).write_bytes(b'corrupt')
        old = {'inputs': before, 'artifacts': {'build': records.pop('engine'), 'games': records}}
        after = dict(self.files)
        after.update(changed or {})
        current = ci.fingerprints(after, self.programs, preset)
        return ci.select(current, old, self.cache, force)

    def test_docs_and_wiki_do_not_build_or_test(self):
        for preset in ci.PRESETS:
            for file in ('docs/games/wiki/BOX-SHIFT.md', 'docs/games/screenshots/box-shift/title.png',
                         'games/box-shift/README.md', 'core/README.md', 'games/tools/wiki.py'):
                with self.subTest(preset=preset, file=file):
                    self.assertFalse(self.plan(preset, {file: 'changed'})['work'])

    def test_one_game_runs_only_that_game(self):
        for preset in ci.PRESETS:
            plan = self.plan(preset, {'games/box-shift/main.s': 'changed'})
            self.assertEqual(plan['games'], ['box-shift'])
            self.assertFalse(plan['build'])
            self.assertFalse(plan['shared'])

    def test_dot_claim_sources_hud_and_checks_are_game_owned(self):
        files = ci.source_files(ROOT)
        programs = build_games.catalog(ROOT)
        for preset in ci.PRESETS:
            before = ci.fingerprints(files, programs, preset)
            for path in ('games/dot-claim/main.s', 'games/dot-claim/generate.py',
                         'games/dot-claim/hud.py', 'games/dot-claim/check.mjs'):
                with self.subTest(preset=preset, path=path):
                    self.assertIn(path, files)
                    after = ci.fingerprints({**files, path: 'changed'}, programs, preset)
                    self.assertEqual(before['build'], after['build'])
                    self.assertEqual(before['shared'], after['shared'])
                    self.assertEqual([game for game in before['games'] if
                                      before['games'][game] != after['games'][game]], ['dot-claim'])

    def test_web_style_only_runs_wasm_shared_tests(self):
        for preset in ci.PRESETS:
            plan = self.plan(preset, {'web/styles.css': 'changed'})
            self.assertFalse(plan['build'])
            self.assertEqual(plan['games'], [])
            self.assertEqual(plan['work'], preset.startswith('wasm'))
            self.assertEqual(plan['shared'], preset.startswith('wasm'))

    def test_shared_dependencies_and_unknown_sources_invalidate_games(self):
        for file in ('core/src/cpu.cpp', 'sdk/lib/lcd/display.s', 'games/common/runtime.s',
                     'games/tools/check.mjs', 'sdk/examples/lcd/07-relic-dive/main.s',
                     'tools/ci.py', 'new-runtime/input.cpp'):
            for preset in ci.PRESETS:
                plan = self.plan(preset, {file: 'changed'})
                self.assertEqual(set(plan['games']), set(plan['inputs']['games']))

    def test_adapter_affects_every_wasm_replay(self):
        self.assertEqual(len(self.plan('wasm-release', {'web/wasm-machine.mjs': 'new'})['games']), 3)

    def test_missing_cache_and_forced_run_retest_everything(self):
        current = ci.fingerprints(self.files, self.programs, 'wasm-release')
        cold = ci.select(current, {}, self.cache)
        self.assertTrue(cold['build'] and cold['shared'])
        self.assertEqual(len(cold['games']), 3)
        self.assertEqual(self.plan('wasm-release', force=True)['games'], cold['games'])

    def test_corrupt_game_retests_only_that_game(self):
        plan = self.plan('wasm-release', damage='box-shift')
        self.assertEqual(plan['games'], ['box-shift'])
        self.assertFalse(plan['build'])

    def test_corrupt_engine_invalidates_all_validation(self):
        plan = self.plan('wasm-release', damage='engine')
        self.assertTrue(plan['build'] and plan['shared'])
        self.assertEqual(len(plan['games']), 3)

    def test_receipt_paths_cannot_escape_product_directory(self):
        self.assertFalse(ci.valid_artifacts(self.cache, {'../secret': 'hash'}))
        self.assertFalse(ci.valid_artifacts(self.cache, {'/tmp/secret': 'hash'}))

    def test_cancelled_or_skipped_intermediate_commits_cannot_hide_changes(self):
        # The receipt remains at the last success, regardless of an intermediate
        # game change followed by a docs-only commit or an unchanged diff HEAD~1.
        plan = self.plan('wasm-release', {'games/box-shift/main.s': 'unverified-previous-commit',
                                         'docs/games/wiki/Home.md': 'latest-commit'})
        self.assertEqual(plan['games'], ['box-shift'])

    def test_game_removal_and_metadata_rebuild_catalog_without_replaying_others(self):
        before = ci.fingerprints(self.files, self.programs, 'wasm-release')
        after = ci.fingerprints(self.files, self.programs[:-1], 'wasm-release')
        self.assertNotEqual(before['bundle'], after['bundle'])
        self.assertEqual(before['games']['box-shift'], after['games']['box-shift'])
        plan = self.plan('wasm-release', {'games/catalog.json': 'new-title'})
        self.assertTrue(plan['bundle'])
        self.assertEqual(plan['games'], [])

    def test_ctest_selection_includes_matching_native_replay_and_dependencies(self):
        names = ['game_box-shift', 'native_replay_box-shift', 'game_mirror-link',
                 'native_replay_mirror-link', 'web_ui_contract']
        listing = json.dumps({'tests': [{'name': n} for n in names]}).encode()
        with patch.object(ci.subprocess, 'check_output', return_value=listing), patch.object(ci, 'run') as run:
            selected = ci.ctest(ROOT, 'wasm-release', ['box-shift'], False)
            self.assertEqual(selected, names[:2])
            self.assertNotIn('mirror-link', run.call_args.args[-1])
            self.assertNotIn('web_ui_contract', run.call_args.args[-1])
            with self.assertRaises(ValueError):
                ci.ctest(ROOT, 'wasm-release', ['unknown'], False)

    def test_failed_checks_do_not_seal_receipt(self):
        plan = {'work': True, 'build': False, 'games': [], 'shared': True}
        with patch.object(ci, 'restore'), patch.object(ci, 'configure'), \
             patch.object(ci, 'ctest', side_effect=subprocess.CalledProcessError(1, 'ctest')), \
             patch.object(ci, 'seal') as seal:
            with self.assertRaises(subprocess.CalledProcessError):
                ci.execute(ROOT, self.cache, 'native-release', plan)
            seal.assert_not_called()

    def test_real_catalog_and_web_allowlist(self):
        programs = build_games.catalog(ROOT)
        self.assertGreaterEqual(len(programs), 51)
        self.assertIn('app.mjs', [p.name for p in ci.web_assets(ROOT)])
        self.assertTrue(all(p.is_file() for p in ci.web_assets(ROOT)))


class BuildGamesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'games').mkdir()
        (self.root / 'games/catalog.json').write_text(json.dumps({'programs':
            [{'id': 'box-shift'}, {'id': 'mirror-link'}]}))
        for game in ('box-shift', 'mirror-link'):
            directory = self.root / 'build/games' / game
            directory.mkdir(parents=True)
            (directory / (game + '.j8a')).write_bytes(game.encode())
        for name in ('tools/jr8as', 'tools/jr8ld', 'tests/game_replay_test'):
            path = self.root / 'build/native-release' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        self.output = self.root / 'output'

    def test_only_selected_game_is_built_but_catalog_contains_all_games(self):
        with patch.object(build_games.subprocess, 'run') as run:
            build_games.build_games(self.root, self.output, ['box-shift'])
            self.assertEqual(run.call_count, 1)
            self.assertTrue(run.call_args.args[0][-1].endswith('/box-shift'))
        catalog = json.loads((self.output / 'program-catalog.json').read_text())
        self.assertEqual(len(catalog['programs']), 2)
        for program in catalog['programs']:
            self.assertEqual(program['sha256'], ci.digest((self.output / program['file']).read_bytes()))

    def test_empty_selection_reuses_all_games_without_build_tools(self):
        with patch.object(build_games.subprocess, 'run') as run:
            build_games.build_games(self.root, self.output, [])
            run.assert_not_called()

    def test_absent_reusable_artifact_cannot_emit_catalog(self):
        (self.root / 'build/games/mirror-link/mirror-link.j8a').unlink()
        with self.assertRaisesRegex(ValueError, 'Missing reusable game artifact'):
            build_games.build_games(self.root, self.output, [])
        self.assertFalse((self.output / 'program-catalog.json').exists())

    def test_unknown_game_cannot_run_make(self):
        with patch.object(build_games.subprocess, 'run') as run:
            with self.assertRaises(ValueError):
                build_games.build_games(self.root, self.output, ['../bad'])
            run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
