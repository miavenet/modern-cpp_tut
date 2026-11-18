"""Regression tests for the tail-lesson checks: known counter-examples stay fixed.

Card regexes are evaluated exactly as drive.py grades them; checks are run through their
.impl.sh. Run with: python3 -m unittest discover -s tools -p 'test_*.py'
"""
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
sys.path.insert(0, str(ROOT))
import drive  # noqa: E402


def card(module, cid):
    data = json.loads((ROOT / 'modules' / module / 'cards.json').read_text())
    return data['cards'][cid]['grading']


def regex_ok(module, cid, answer):
    return drive.eval_predicate(card(module, cid)['accept'], answer, str(ROOT / 'modules' / module))[0]


def run_impl(module, name, answer):
    path = ROOT / 'modules' / module / 'checks' / f'{name}.impl.sh'
    result = subprocess.run(['bash', str(path)], input=answer, text=True,
                            capture_output=True, timeout=120)
    return result.returncode, result.stdout + result.stderr


class CardRegexTests(unittest.TestCase):
    def test_percentile_cards_accept_equivalent_spellings(self):
        for module, cid in (('04-stl-modern', 'M4-LPA4'), ('05-memory-perf', 'M5-LPA4')):
            for ok in ('p99.9', '99.9', 'p99.9th', '`p99.9`', 'p99.9.', '99.9th percentile',
                       'the 99.9th percentile', '99.9%', ' P99.9 \n'):
                with self.subTest(card=cid, answer=ok):
                    self.assertTrue(regex_ok(module, cid, ok))
            for bad in ('p99', 'p99.99', '99.99', 'p50', 'p99.9 and p99.99', '999', 'p99.9x'):
                with self.subTest(card=cid, answer=bad):
                    self.assertFalse(regex_ok(module, cid, bad))

    def test_m6_lpa2_is_derived_from_the_card_not_the_machine(self):
        # 10 ticks x 42 ns / 8 ns = 52.5 -> 53; the old machine-specific answer 84 is wrong.
        for ok in ('53', '`53`', '53 calls', ' 53. '):
            self.assertTrue(regex_ok('06-hft-patterns', 'M6-LPA2', ok), ok)
        for bad in ('84', '52', '54', '52.5', '420', ''):
            self.assertFalse(regex_ok('06-hft-patterns', 'M6-LPA2', bad), bad)

    def test_m6_lpa6_accepts_service_or_strategy(self):
        for ok in ('service', 'Service.', '`service`', 'strategy', 'the strategy', 'STRATEGY'):
            self.assertTrue(regex_ok('06-hft-patterns', 'M6-LPA6', ok), ok)
        for bad in ('feed', 'queue', 'service queue', 'services', ''):
            self.assertFalse(regex_ok('06-hft-patterns', 'M6-LPA6', bad), bad)

    def test_m7_lpa4_equivalents_and_order(self):
        good = ('symbols: flat_map; orders: map', 'symbols: sorted vector; orders: map',
                'symbols: std::flat_map, orders: std::map', 'symbols: flat_map; orders: map;',
                'orders: map; symbols: flat_map', 'orders: map, symbols: sorted vector.',
                'Symbols: `flat_map`; Orders: `map`')
        bad = ('symbols: map; orders: flat_map', 'symbols: flat_map; orders: flat_map',
               'symbols: map; orders: map', 'symbols: flat_map', 'symbols: flat_map; orders: map; extra',
               'symbols: vector; orders: map')
        for a in good:
            self.assertTrue(regex_ok('07-architecture', 'M7-LPA4', a), a)
        for a in bad:
            self.assertFalse(regex_ok('07-architecture', 'M7-LPA4', a), a)

    def test_comment_words_are_no_longer_blanket_rejected_on_cards(self):
        for module, cid in (('03-concurrency', 'M3-LPA3'), ('04-stl-modern', 'M4-LPA3')):
            self.assertNotIn('reject', card(module, cid), cid)


class CheckAcceptTests(unittest.TestCase):
    def assertHit(self, module, name, answer):
        code, out = run_impl(module, name, answer)
        self.assertEqual(code, 0, f'{name}: {answer!r}\n{out}')

    def assertMiss(self, module, name, answer):
        code, out = run_impl(module, name, answer)
        self.assertEqual(code, 1, f'{name}: {answer!r}\n{out}')

    def test_m7_lpa3_needs_a_real_index_and_takes_a_semicolon(self):
        for ok in ('s[(s.size() * 9999 + 9999) / 10000 - 1];', '`s[(s.size() * 9999 + 9999) / 10000 - 1]`'):
            self.assertHit('07-architecture', 'm7-lpa3', ok)
        for bad in ('(s.size() * 9999 + 9999) / 10000', 'std::ceil(0.9999 * s.size())',
                    's.size() - s.size() / 10000'):
            self.assertMiss('07-architecture', 'm7-lpa3', bad)

    def test_m3_lpa3_forms(self):
        for ok in ('while (turn.load(std::memory_order_acquire) != 1) {} // spin, never wait',
                   'for (; turn.load(std::memory_order_acquire) != 1; ) {}',
                   'while (turn.load(std::memory_order_acquire) != 1) {};'):
            self.assertHit('03-concurrency', 'm3-lpa3', ok)
        for bad in ('while (turn.load(std::memory_order_consume) != 1) {}',
                    'while (turn.load(std::memory_order_relaxed) != 1) {}'):
            self.assertMiss('03-concurrency', 'm3-lpa3', bad)

    def test_m4_lpa3_forms(self):
        for ok in ('{ return std::unexpected(reject::bad_qty); }',
                   'return std::unexpected(reject::bad_qty); // never throw',
                   'return std::unexpected{reject::bad_qty};'):
            self.assertHit('04-stl-modern', 'm4-lpa3', ok)
        for bad in ('return {std::unexpect}', 'return std::unexpected{reject{}}',
                    'return std::unexpected(reject{});'):
            self.assertMiss('04-stl-modern', 'm4-lpa3', bad)

    def test_m5_lpa5_forms(self):
        self.assertHit('05-memory-perf', 'm5-lpa5', 'std::pmr::null_memory_resource();')
        self.assertMiss('05-memory-perf', 'm5-lpa5',
                        'new std::pmr::monotonic_buffer_resource(std::pmr::null_memory_resource())')

    def test_m1_lpa4_forms(self):
        for ok in ("live.reserve(500'000);", 'live.reserve(500000)', "live.reserve(500'000u);",
                   "live.reserve(std::size_t{500'000});",
                   "live.rehash(std::ceil(500'000 / live.max_load_factor()));"):
            self.assertHit('01-foundations', 'm1-lpa4', ok)
        for bad in ("live.reserve(500 000);", "live.rehash(500'000);", "live.reserve(250'000);"):
            self.assertMiss('01-foundations', 'm1-lpa4', bad)

    def test_m2_lpa2_forms(self):
        for ok in ('constinit thread_local std::uint64_t seq = 0;',
                   'std::uint64_t constinit thread_local seq = 0;',
                   'constinit thread_local std::uint64_t seq(0);',
                   'constinit inline thread_local std::uint64_t seq = 0;',
                   'thread_local constinit std::uint64_t seq{0};'):
            self.assertHit('02-type-system', 'm2-lpa2', ok)
        for bad in ('constinit thread_local constinit std::uint64_t seq = 0;',
                    'thread_local std::uint64_t seq = 0;', 'constinit std::uint64_t seq = 0;'):
            self.assertMiss('02-type-system', 'm2-lpa2', bad)

    def test_round2_counter_examples(self):
        self.assertHit('07-architecture', 'm7-lpa3', 's[(s.size() * 9999 + 9999) / 10000 - 1] // p99.99')
        self.assertHit('07-architecture', 'm7-lpa3', 's[(s.size() * 9999 + 9999) / 10000 - 1] /* p99.99 */')
        self.assertMiss('01-foundations', 'm1-lpa4', "(live.max_load_factor(1e9f), live.rehash(1))")
        self.assertMiss('01-foundations', 'm1-lpa4', "live.max_load_factor(100.0f), live.reserve(500'000)")
        for bad in ('thread_local std::uint64_t seq = (sizeof("constinit") == 0);',
                    'thread_local std::uint64_t seq = []{ return sizeof("constinit") - 8; }();'):
            self.assertMiss('02-type-system', 'm2-lpa2', bad)
        self.assertHit('02-type-system', 'm2-lpa2', 'thread_local constinit std::uint64_t seq(0);')
        for bad in ('while (turn.load(std::memory_order_relaxed) != 1) {} if (0) std::atomic_thread_fence(std::memory_order_acquire);',
                    'while (turn.load(std::memory_order_relaxed) != 1) { std::atomic_thread_fence(std::memory_order_acquire); }',
                    'while (turn.load(std::memory_order_relaxed) != 1) {} std::atomic_thread_fence(std::memory_order_acquire);'):
            self.assertMiss('03-concurrency', 'm3-lpa3', bad)
        for bad in ('comment', 'wrongvar'):
            p = f'modules/02-type-system/checks/fixtures/constinit_{bad}/constinit_guard.cpp'
            self.assertMiss('02-type-system', 'constinit_lab', p)

    def test_tampered_counters_do_not_fool_the_prepare_checks(self):
        import re
        import shutil
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            for module, name, sdir, base in (('01-foundations', 'push_tail', '01-foundations/lab/starter', 'push_tail.cpp'),
                                             ('05-memory-perf', 'pool_tail', '05-memory-perf/lab/starter', 'pool_tail.cpp')):
                d = Path(tmp) / name
                d.mkdir()
                for f in (ROOT / 'modules' / sdir).glob('*.cpp'):
                    shutil.copy(f, d)
                if name == 'pool_tail':
                    shutil.copy(ROOT / 'modules/05-memory-perf/lab/solution/pool_lab.cpp', d)
                target = d / base
                text = target.read_text()
                text = re.sub(r'\+\+g_(global_news|copies)\s*;', '', text)   # counters that never count
                target.write_text(text)
                self.assertMiss(module, name, str(target))

    def test_file_path_checks_trim_whitespace_and_quotes(self):
        for module, name, path in (
                ('01-foundations', 'push_tail', 'modules/01-foundations/lab/solution/push_tail.cpp'),
                ('05-memory-perf', 'pool_tail', 'modules/05-memory-perf/lab/solution/pool_tail.cpp'),
                ('06-hft-patterns', 'tail_budget', 'modules/06-hft-patterns/lab/budget/solution/tail_budget.cpp'),
                ('02-type-system', 'constinit_lab', 'modules/02-type-system/lab/solution/constinit_guard.cpp')):
            with self.subTest(check=name):
                self.assertHit(module, name, f'  "{path}"  \n')


class DriverFixTests(unittest.TestCase):
    """Defects 5 and 6 of the third scoring: lab path default and sure_miss clearing."""

    def setUp(self):
        import tempfile
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)
        self.progress = self.dir / 'progress.json'

    def run_drive(self, lessons, answers, date, max_cards=1):
        import contextlib
        import io
        script = self.dir / 'answers.txt'
        script.write_text('\n'.join(answers) + '\n')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            result = drive.run(lessons, self.progress, date, str(script), max_cards=max_cards)
        return out.getvalue(), result

    def test_workspace_default_finds_modules_lab_work_path(self):
        lessons = self.dir / 'lessons'
        checks = self.dir / 'checks'
        lessons.mkdir()
        checks.mkdir()
        path = 'modules/02-type-system/lab/work/constinit_guard.cpp'
        (lessons / 'lesson-01.md').write_text(
            '# Lesson 01 - Lab\n\nTime: 5 min\n\n'
            '### CARD LAB-1  [type: lab]  [box: 1]\n'
            f'Q: Fix `{path}`, then submit that path.\n'
            'Key:\n  - The default workspace passes.\n'
            'AcceptIf: check(checks/lab.check.sh)\nSource: local lab\n')
        check = checks / 'lab.check.sh'
        check.write_text('#!/bin/bash\nread -r path\n'
                         f'if [ "$path" = {path} ]; then echo HIT; exit 0; fi\n'
                         'echo "MISS: got $path"; exit 1\n')
        check.chmod(0o755)
        out, result = self.run_drive(lessons, ['', 'u'], '2030-01-10')
        self.assertIn(f'press Enter to run the check on {path}', out)
        self.assertEqual(result['LAB-1']['box'], 2)

    def test_sure_miss_cleared_on_later_hit_and_old_files_load(self):
        lessons = self.dir / 'lessons'
        lessons.mkdir()
        (lessons / 'lesson-01.md').write_text(
            '# Lesson 01\n\nTime: 5 min\n\n### CARD R-1 [type: recall]\n'
            'Q: Explain ownership.\nKey:\n  - unique ownership transfers.\nSource: local\n')
        _, result = self.run_drive(lessons, ['wrong', 's', 'm'], '2030-01-10')
        self.assertTrue(result['R-1']['sure_miss'])
        self.assertEqual(result['R-1']['box'], 1)
        _, result = self.run_drive(lessons, ['right', 'u', 'h'], '2030-01-11')
        self.assertNotIn('sure_miss', result['R-1'])
        self.assertEqual(result['R-1']['box'], 2)
        # A progress file written before the field existed still loads and commits.
        state = drive.load_progress(self.progress)
        self.assertNotIn('sure_miss', state['R-1'])
        _, result = self.run_drive(lessons, ['right', 'u', 'h'], '2030-01-13')
        self.assertEqual(result['R-1']['box'], 3)
        self.assertNotIn('sure_miss', result['R-1'])


if __name__ == '__main__':
    unittest.main()
