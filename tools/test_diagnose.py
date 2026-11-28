"""Read-only diagnose mode tests. Run with unittest discover."""
import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
sys.path.insert(0, str(ROOT))
import drive
from driver_lib import diagnose, tutorial_format

MODULES = ROOT / 'modules'
DAY = '2030-01-10'


def course_groups():
    return [(doc, [c for c in doc['cards'] if diagnose.eligible(c)])
            for lessons in diagnose.lesson_dirs(MODULES)
            for doc in tutorial_format.parse_lessons(lessons)]


def script_for(chosen):
    """First card "don't know"; the rest a wrong answer, unsure, then a miss grade."""
    lines = [':skip']
    for _, card in chosen[1:]:
        lines += ['wrong', 'u']
        if not card.get('AcceptIf') or drive.needs_self_grade(card):
            lines.append('m')
    return lines


def snapshot():
    return {p: p.read_bytes() for p in ROOT.rglob('progress*.json')}


class DiagnoseTests(unittest.TestCase):
    def test_set_is_deterministic_spread_and_interleaved(self):
        groups = course_groups()
        first = diagnose.select(groups, DAY, 10)
        ids = [card['id'] for _, card in first]
        self.assertEqual(ids, [card['id'] for _, card in diagnose.select(course_groups(), DAY, 10)])
        self.assertEqual(len(set(ids)), 10)
        self.assertNotEqual(ids, [c['id'] for _, c in diagnose.select(groups, '2030-01-11', 10)])
        lessons = [str(doc['path']) for doc, _ in first]
        self.assertEqual(len(set(lessons)), 10)  # one card per lesson when lessons outnumber N
        self.assertGreaterEqual(len({doc['path'].parent.parent for doc, _ in first}), 5)
        everything = diagnose.select(groups, DAY, 10000)
        self.assertEqual(len(everything), sum(len(cards) for _, cards in groups))
        self.assertTrue(all(not c['once'] and not (c.get('AcceptIf') or '').startswith('check(')
                            for _, c in everything))
        self.assertTrue(any(diagnose.is_bank(doc['path']) for doc, _ in everything))
        pairs = list(zip(everything, everything[1:]))
        self.assertTrue(all(a[0] is not b[0] for a, b in pairs[:len(diagnose.units(groups)) - 1]))

    def test_bank_parts_fit_a_sitting_and_weigh_as_one_lesson(self):
        groups = course_groups()
        banks = [doc for doc, _ in groups if diagnose.is_bank(doc['path'])]
        self.assertGreater(len(banks), 7)  # banks over 30 min are split into parts
        for doc in banks:
            self.assertLessEqual(diagnose.lesson_minutes(doc), 30, doc['path'].name)
            self.assertRegex(doc['path'].name, r'^lesson-\d\d-practice-bank-\d\d-[a-z0-9-]+\.md$')
        bank_units = [u for u in diagnose.units(groups) if diagnose.is_bank(u[0][0]['path'])]
        self.assertEqual(len(bank_units), 7)  # one selection unit per module bank
        self.assertEqual(sum(map(len, bank_units)),
                         sum(len(cards) for doc, cards in groups if diagnose.is_bank(doc['path'])))

    def test_module_and_lessons_paths_are_accepted(self):
        one = MODULES / '01-foundations'
        self.assertEqual(diagnose.lesson_dirs(one), [one / 'lessons'])
        self.assertEqual(diagnose.lesson_dirs(one / 'lessons'), [one / 'lessons'])
        self.assertEqual(len(diagnose.lesson_dirs(MODULES)), 7)

    def test_recommendations_group_by_lesson_and_rank_worst_first(self):
        groups = [g for g in course_groups() if len(g[1]) >= 2][:3]
        (a, ac), (b, bc), (c, cc) = groups
        results = [(a, ac[0], 'p', False), (b, bc[0], 'm', True),
                   (a, ac[1], 'h', False), (b, bc[1], 'h', True),
                   (c, cc[0], 'm', False), (c, cc[1], 'h', True)]
        reads, solid = diagnose.recommend(results)
        self.assertEqual([doc['path'] for doc, _, _ in reads], [b['path'], a['path'], c['path']])
        self.assertEqual([score for _, score, _ in reads], [4, 3, 3])
        self.assertEqual([card['id'] for _, card, _, _ in reads[1][2]], [ac[0]['id'], ac[1]['id']])
        self.assertEqual([card['id'] for card in solid], [bc[1]['id'], cc[1]['id']])
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            diagnose.report(results)
        text = output.getvalue()
        self.assertEqual(text.count(str(a['path'])), 1)
        self.assertIn(f"(about {diagnose.lesson_minutes(a)} min)", text)
        self.assertIn(diagnose.section(b, bc[0]) + ': ' + bc[0]['id'] + ' missed while sure', text)
        self.assertRegex(text, r'\nSolid: .*' + cc[1]['id'])

    def test_section_names_concept_or_bank_card(self):
        for doc, cards in course_groups():
            for card in cards:
                name = diagnose.section(doc, card)
                if diagnose.is_bank(doc['path']):
                    self.assertEqual(name, 'Card ' + card['id'])
                else:
                    self.assertTrue(name.startswith('CONCEPT ') or name in doc['text'], name)

    def test_run_writes_no_progress_file(self):
        before = snapshot()
        chosen = diagnose.select(course_groups(), DAY, 4)
        with tempfile.TemporaryDirectory() as temp:
            script = Path(temp) / 'answers.txt'
            script.write_text('\n'.join(script_for(chosen)) + '\n')
            with patch.object(drive, 'save_progress', side_effect=AssertionError('wrote progress')), \
                    patch.object(drive, 'load_progress', side_effect=AssertionError('read progress')), \
                    contextlib.redirect_stdout(io.StringIO()):
                results = diagnose.run(MODULES, DAY, 4, str(script))
            self.assertEqual(sorted(os.listdir(temp)), ['answers.txt'])
        self.assertEqual(len(results), 4)
        self.assertEqual(snapshot(), before)

    def test_auto_cli_end_to_end(self):
        before = snapshot()
        chosen = diagnose.select(course_groups(), DAY, 3)
        with tempfile.TemporaryDirectory() as temp:
            script = Path(temp) / 'answers.txt'
            script.write_text('\n'.join(script_for(chosen)) + '\n')
            result = subprocess.run(
                [sys.executable, str(ROOT / 'drive.py'), 'modules', '--today', DAY,
                 '--diagnose', '--set', '3', '--auto', str(script)],
                cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        out = result.stdout
        self.assertIn('Diagnose: 3 questions from 3 lessons, about 6 min. Read-only', out)
        for number, (doc, card) in enumerate(chosen, 1):
            self.assertIn(f"[{number}/3 · {card['id']}]", out)
            self.assertIn(f"{doc['path'].relative_to(ROOT)} (about {diagnose.lesson_minutes(doc)} min)", out)
        self.assertIn('Diagnosis: 0/3 hits. No progress was recorded.', out)
        self.assertLess(out.index('Read next'), out.index('Solid: none yet'))
        self.assertEqual(snapshot(), before)

    def test_help_and_bad_set(self):
        help_text = subprocess.run([sys.executable, str(ROOT / 'drive.py'), '--help'],
                                   text=True, capture_output=True).stdout
        for text in ('--diagnose', '--set', 'never writes'):
            self.assertIn(text, ' '.join(help_text.split()))
        bad = subprocess.run([sys.executable, str(ROOT / 'drive.py'), 'modules', '--today', DAY,
                              '--diagnose', '--set', '0'], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(bad.returncode, 2)
        self.assertIn('--set must be at least 1', bad.stderr)


if __name__ == '__main__':
    unittest.main()
