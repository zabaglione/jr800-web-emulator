# SPDX-License-Identifier: MIT
"""Regression checks for CI selection, reusable artifacts and individual programs."""
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
            'sdk/examples/lcd/07-relic-dive/Makefile',
            'sdk/examples/lcd/14-polygon-fighter/Makefile',
            'sdk/examples/lcd/14-polygon-fighter/main.s',
            'sdk/examples/lcd/common/sample.mk', 'sdk/examples/lcd/common/memory.j8l',
            'sdk/examples/lcd/check.mjs', 'sdk/examples/lcd/Makefile',
            'sdk/examples/write-watch/Makefile', 'sdk/examples/write-watch/main.s',
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
        after = {p: h for p, h in after.items() if h is not None}
        current = ci.fingerprints(after, self.programs, preset)
        return ci.select(current, old, self.cache, force)

    def test_docs_and_wiki_do_not_build_or_test(self):
        for preset in ci.PRESETS:
            for file in ('docs/games/wiki/BOX-SHIFT.md', 'docs/games/screenshots/box-shift/title.png',
                         'games/box-shift/README.md', 'core/README.md', 'games/tools/wiki.py'):
                with self.subTest(preset=preset, file=file):
                    self.assertFalse(self.plan(preset, {file: 'changed'})['work'])

    def test_one_sample_does_not_retest_games_engine_or_other_samples(self):
        for preset in ci.PRESETS:
            for file in ('main.s', 'renderer.s', 'model.s', 'generate_model.py', 'check.mjs', 'Makefile'):
                with self.subTest(preset=preset, file=file):
                    plan = self.plan(preset, {'sdk/examples/lcd/14-polygon-fighter/' + file: 'changed'})
                    self.assertEqual(plan['samples'], ['lcd/14-polygon-fighter'])
                    self.assertEqual(plan['games'], [])
                    self.assertTrue(plan['work'])
                    self.assertFalse(plan['build'] or plan['shared'] or plan['bundle'])

    def test_new_sample_and_aggregate_list_only_select_new_sample(self):
        for preset in ci.PRESETS:
            plan = self.plan(preset, {'sdk/examples/lcd/15-new-demo/Makefile': 'new',
                                     'sdk/examples/lcd/15-new-demo/main.s': 'new',
                                     'sdk/examples/lcd/Makefile': 'updated-list'})
            self.assertEqual(plan['samples'], ['lcd/15-new-demo'])
            self.assertEqual(plan['games'], [])
            self.assertFalse(plan['build'] or plan['shared'] or plan['bundle'])

    def test_sample_deletion_updates_receipt_without_testing_other_programs(self):
        for preset in ci.PRESETS:
            plan = self.plan(preset, {'sdk/examples/lcd/14-polygon-fighter/Makefile': None,
                                     'sdk/examples/lcd/14-polygon-fighter/main.s': None,
                                     'sdk/examples/lcd/Makefile': 'updated-list'})
            self.assertTrue(plan['work'])
            self.assertEqual(plan['samples'], [])
            self.assertEqual(plan['games'], [])
            self.assertNotIn('lcd/14-polygon-fighter', plan['inputs']['samples'])
            self.assertFalse(plan['build'] or plan['shared'] or plan['bundle'])

    def test_sample_docs_preview_and_aggregate_list_do_not_invalidate_checks(self):
        for preset in ci.PRESETS:
            for file in ('sdk/examples/lcd/14-polygon-fighter/README.md',
                         'sdk/examples/lcd/14-polygon-fighter/preview.svg', 'sdk/examples/lcd/Makefile'):
                self.assertFalse(self.plan(preset, {file: 'changed'})['work'])

    def test_lcd_common_files_only_invalidate_lcd_samples(self):
        for preset in ci.PRESETS:
            for file in ('sdk/examples/lcd/common/sample.mk', 'sdk/examples/lcd/check.mjs',
                         'sdk/examples/lcd/common/memory.j8l'):
                plan = self.plan(preset, {file: 'changed'})
                self.assertEqual(plan['samples'], ['lcd/07-relic-dive', 'lcd/14-polygon-fighter'])
                self.assertEqual(plan['games'], [])
                self.assertFalse(plan['build'] or plan['bundle'])
                self.assertEqual(plan['shared'], preset.startswith('native') and file.endswith('memory.j8l'))

    def test_reused_sample_only_invalidates_its_dependent_game(self):
        for preset in ci.PRESETS:
            plan = self.plan(preset, {'sdk/examples/lcd/07-relic-dive/main.s': 'changed'})
            self.assertEqual(plan['samples'], ['lcd/07-relic-dive'])
            self.assertEqual(plan['games'], [] if preset.startswith('native') else ['relic-dive-gfx'])
            self.assertFalse(plan['build'] or plan['shared'])

    def test_write_watch_selects_its_native_workflow_only(self):
        for preset in ci.PRESETS:
            for file in ('sdk/examples/write-watch/main.s', 'tests/sample_make_test.py'):
                plan = self.plan(preset, {file: 'changed'})
                self.assertEqual(plan['samples'], ['write-watch'] if preset.startswith('native') else [])
                self.assertEqual(plan['games'], [])
                self.assertFalse(plan['build'] or plan['shared'] or plan['bundle'])

    def test_cancelled_sample_checks_cannot_be_hidden_by_later_docs(self):
        plan = self.plan('wasm-release', {'sdk/examples/lcd/14-polygon-fighter/main.s': 'unverified',
                                         'sdk/examples/lcd/README.md': 'later-commit'})
        self.assertEqual(plan['samples'], ['lcd/14-polygon-fighter'])

    def test_plan_exports_sdk_requirement_for_selected_work(self):
        for preset in ci.PRESETS:
            cases = [
                (self.plan(preset), False),
                (self.plan(preset, {'sdk/examples/lcd/14-polygon-fighter/main.s': 'new'}), False),
                (self.plan(preset, {'web/styles.css': 'new'}), True),
                (self.plan(preset, {'games/box-shift/main.s': 'new'}), True),
                (self.plan(preset, damage='engine'), True),
                (self.plan(preset, force=True), True),
                (ci.select(ci.fingerprints(self.files, self.programs, preset), {}, self.cache), True),
            ]
            for index, (plan, requires_sdk) in enumerate(cases):
                with self.subTest(preset=preset, case=index):
                    output = self.cache / f'{preset}-{index}.txt'
                    with patch.object(ci, '__file__', str(self.cache / 'tools/ci.py')), \
                         patch.object(ci, 'source_files', return_value=self.files), \
                         patch.object(ci, 'catalog', return_value=self.programs), \
                         patch.object(ci, 'select', return_value=plan), \
                         patch.dict(ci.os.environ, {'GITHUB_STEP_SUMMARY': str(self.cache / 'summary.md')}), \
                         patch.object(sys, 'argv', ['ci.py', 'plan', '--preset', preset,
                                                   '--github-output', str(output)]), \
                         patch.object(sys, 'stdout'):
                        ci.main()
                    values = dict(line.split('=', 1) for line in output.read_text().splitlines())
                    expected = requires_sdk and preset.startswith('wasm')
                    self.assertEqual(values['emsdk'], str(expected).lower())
                    saved = json.loads((self.cache / 'build' / f'ci-plan-{preset}.json').read_text())
                    self.assertEqual(saved['emsdk'], expected)

    def test_cached_samples_and_bundles_run_without_cmake_or_ctest(self):
        for preset in ci.PRESETS:
            for samples in (['lcd/14-polygon-fighter'], []):
                with self.subTest(preset=preset, samples=samples):
                    plan = {'work': True, 'build': False, 'games': [], 'samples': samples,
                            'shared': False, 'bundle': not samples, 'inputs': {'verified': 'inputs'}}
                    checks = ['sample_lcd/14-polygon-fighter:test'] if samples else []
                    with patch.object(ci, 'restore'), patch.object(ci, 'package'), \
                         patch.object(ci, 'configure', side_effect=AssertionError('Unexpected CMake')), \
                         patch.object(ci, 'ctest', side_effect=AssertionError('Unexpected CTest')), \
                         patch.object(ci, 'check_samples', return_value=checks) as check_samples, \
                         patch.object(ci, 'source_files'), patch.object(ci, 'catalog'), \
                         patch.object(ci, 'fingerprints', return_value=plan['inputs']), \
                         patch.object(ci, 'seal') as seal:
                        ci.execute(ROOT, self.cache, preset, plan)
                        check_samples.assert_called_once_with(ROOT, preset, samples)
                        seal.assert_called_once_with(ROOT, self.cache, preset, plan, checks)

    def test_registered_checks_still_require_configuration(self):
        for preset in ci.PRESETS:
            cases = [(True, [], []), (False, ['box-shift'], [])]
            if preset.startswith('native'):
                cases.append((False, [], ['write-watch']))
            for shared, games, samples in cases:
                with self.subTest(preset=preset, shared=shared, games=games, samples=samples):
                    plan = {'work': True, 'build': False, 'shared': shared,
                            'games': games, 'samples': samples}
                    with patch.object(ci, 'restore'), patch.object(ci, 'package'), \
                         patch.object(ci.shutil, 'rmtree'), patch.object(ci, 'configure') as configure, \
                         patch.object(ci, 'check_samples', return_value=[]), \
                         patch.object(ci, 'ctest', side_effect=subprocess.CalledProcessError(1, 'ctest')):
                        with self.assertRaises(subprocess.CalledProcessError):
                            ci.execute(ROOT, self.cache, preset, plan)
                        configure.assert_called_once_with(ROOT, preset)

    def test_real_sample_inventory_and_dependency_scope(self):
        files = ci.source_files(ROOT)
        self.assertIn('lcd/14-polygon-fighter', ci.sample_names(files))
        self.assertNotIn('lcd', ci.sample_names(files))
        for preset in ci.PRESETS:
            before = ci.fingerprints(files, build_games.catalog(ROOT), preset)
            after = ci.fingerprints({**files, 'sdk/examples/lcd/14-polygon-fighter/main.s': 'changed'},
                                    build_games.catalog(ROOT), preset)
            self.assertEqual(before['games'], after['games'])
            self.assertEqual([name for name in before['samples'] if
                              before['samples'][name] != after['samples'][name]], ['lcd/14-polygon-fighter'])

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
                     'games/tools/check.mjs',
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
        self.assertEqual(cold['samples'], list(current['samples']))
        self.assertEqual(self.plan('wasm-release', force=True)['samples'], cold['samples'])

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
            selected = ci.ctest(ROOT, 'wasm-release', ['box-shift'], [], False)
            self.assertEqual(selected, names[:2])
            self.assertNotIn('mirror-link', run.call_args.args[-1])
            self.assertNotIn('web_ui_contract', run.call_args.args[-1])
            with self.assertRaises(ValueError):
                ci.ctest(ROOT, 'wasm-release', ['unknown'], [], False)

    def test_write_watch_ctest_is_independent_of_shared_checks(self):
        listing = json.dumps({'tests': [{'name': n} for n in ['sample_make_test', 'native_smoke']]}).encode()
        with patch.object(ci.subprocess, 'check_output', return_value=listing), patch.object(ci, 'run'):
            self.assertEqual(ci.ctest(ROOT, 'native-release', [], ['write-watch'], False), ['sample_make_test'])
            self.assertEqual(ci.ctest(ROOT, 'native-release', [], [], True), ['native_smoke'])

    def test_failed_checks_do_not_seal_receipt(self):
        plan = {'work': True, 'build': False, 'games': [], 'samples': [], 'shared': True}
        with patch.object(ci, 'restore'), patch.object(ci, 'configure'), \
             patch.object(ci, 'ctest', side_effect=subprocess.CalledProcessError(1, 'ctest')), \
             patch.object(ci, 'seal') as seal:
            with self.assertRaises(subprocess.CalledProcessError):
                ci.execute(ROOT, self.cache, 'native-release', plan)
            seal.assert_not_called()

    def test_failed_sample_does_not_seal_receipt(self):
        plan = {'work': True, 'build': False, 'games': [], 'samples': ['lcd/14-polygon-fighter'], 'shared': False}
        with patch.object(ci, 'restore'), patch.object(ci, 'configure'), \
             patch.object(ci, 'check_samples', side_effect=subprocess.CalledProcessError(1, 'make')), \
             patch.object(ci, 'seal') as seal:
            with self.assertRaises(subprocess.CalledProcessError):
                ci.execute(ROOT, self.cache, 'native-release', plan)
            seal.assert_not_called()

    def test_only_current_receipt_format_is_accepted(self):
        for version in (1, 2):
            (self.cache / 'verified.json').write_text(json.dumps({'version': version}))
            self.assertEqual(ci.read_receipt(self.cache), {'version': 2} if version == 2 else {})

    def test_real_catalog_and_web_allowlist(self):
        programs = build_games.catalog(ROOT)
        self.assertGreaterEqual(len(programs), 51)
        self.assertIn('app.mjs', [p.name for p in ci.web_assets(ROOT)])
        self.assertTrue(all(p.is_file() for p in ci.web_assets(ROOT)))


class CheckSamplesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sample = 'lcd/14-polygon-fighter'
        self.files = {'sdk/examples/' + self.sample + '/Makefile': 'hash'}

    def test_only_selected_output_is_rebuilt_with_the_selected_tools(self):
        for preset in ci.PRESETS:
            output = self.root / 'build/ci-samples' / preset / self.sample
            output.mkdir(parents=True)
            (output / 'stale.jro').touch()
            other = self.root / 'build/ci-samples' / preset / 'lcd/01-hello/keep.jro'
            other.parent.mkdir(parents=True)
            other.touch()
            with patch.object(ci, 'source_files', return_value=self.files), patch.object(ci, 'run') as run:
                ci.check_samples(self.root, preset, [self.sample])
                self.assertFalse(output.exists())
                self.assertTrue(other.is_file())
                self.assertEqual(run.call_count, 1)
                args = [str(a) for a in run.call_args.args]
                target = 'all' if preset.startswith('native') else ('run' if preset == 'wasm-debug' else 'test')
                self.assertEqual(args[5], target)
                tools = preset if preset.startswith('native') else 'native-release'
                self.assertIn('JR8AS=' + str(self.root / 'build' / tools / 'tools/jr8as'), args)
                self.assertIn('WASM_DIR=' + str(self.root / 'build' / preset / 'web-module'), args)

    def test_unknown_sample_cannot_run_make(self):
        with patch.object(ci, 'source_files', return_value=self.files), patch.object(ci, 'run') as run:
            with self.assertRaises(ValueError):
                ci.check_samples(self.root, 'wasm-release', ['../bad'])
            run.assert_not_called()


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
