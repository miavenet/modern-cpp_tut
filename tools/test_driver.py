"""Offline driver and authored-check regression tests. Run with unittest discover."""
import contextlib
import datetime
import io
import json
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import sys
sys.path.insert(0, str(ROOT))
import drive
import launch
from driver_lib import tutorial_format

LESSONS = ROOT / 'modules/01-foundations/lessons'
DAY = '2030-01-10'


class DriverTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)
        self.progress = self.dir / 'progress.json'

    def scripted(self, values, date=DAY, max_cards=12, reviews=4):
        script = self.dir / 'answers.txt'
        script.write_text('\n'.join(values) + '\n')
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            result = drive.run(LESSONS, self.progress, date, str(script),
                               max_cards=max_cards, review_budget=reviews,
                               new_budget=max_cards, predict_worked=False,
                               offer_stretch=False)
        return output.getvalue(), result

    def test_every_lesson_parses(self):
        total = 0
        ids = set()
        for module in launch.discover_modules():
            docs = tutorial_format.parse_lessons(module['lessons_path'])
            self.assertTrue(docs)
            concept_ids = set()
            for doc in docs:
                self.assertTrue(doc['cards'])
                for concept in doc['concepts']:
                    self.assertNotIn(concept['id'], concept_ids)
                    concept_ids.add(concept['id'])
                for card in doc['cards']:
                    self.assertNotIn(card['id'], ids)
                    ids.add(card['id'])
                    total += 1
                    self.assertNotIn('<details>', card['Q'])
                    self.assertNotIn('</details>', '\n'.join(card['Key']))
                    raw = card['raw']
                    self.assertLess(raw.index('<details><summary>Answer</summary>'),
                                    raw.index('Key:'))
                    self.assertNotIn('Source:', raw)
                    self.assertTrue(card['Source'])
                    self.assertRegex(raw.splitlines()[0], r'^### Card M\d+-\S+ · ')
                    if card['type'] not in ('pretest', 'lab'):
                        self.assertIn('Answer format:', card['Q'])
                    for name in __import__('re').findall(r'check\(([^)]+)\)',
                                                         card.get('AcceptIf') or ''):
                        self.assertTrue((Path(module['path']) / name).is_file(),
                                        f"{card['id']} refers to missing {name}")
        self.assertGreater(total, 100)

    def test_manifest_rows_and_grading_are_loaded(self):
        lessons = self.dir / 'lessons'
        lessons.mkdir()
        (self.dir / 'MANIFEST.md').write_text('# Fixture\n')
        (lessons / 'lesson-01.md').write_text(
            '# Lesson\n\n**Time:** about 3 minutes\n\n'
            '### Card X-1 · Apply\nQ: Give the exact word pass.\n'
            '<details><summary>Answer</summary>\nKey:\n  - pass\n</details>\n')
        row = {'lesson': 'lesson-01.md', 'type': 'apply', 'topic': 'sample',
               'objective': 1, 'minutes': 3, 'box': 1, 'once': False,
               'grading': {'mode': 'regex', 'accept': 'regex(/\\A(?:pass|pass deny)\\Z/)',
                           'reject': 'regex(/deny/i)'},
               'source': 'local fixture', 'concepts': []}
        data = {'schema_version': 1, 'lessons': {'lesson-01.md': {'minutes': 3}},
                'cards': {'X-1': row}, 'concepts': {}}
        path = self.dir / 'cards.json'
        path.write_text(json.dumps(data))
        card = tutorial_format.parse_lessons(lessons)[0]['cards'][0]
        self.assertEqual(card['AcceptIf'], row['grading']['accept'])
        self.assertEqual(card['RejectIf'], row['grading']['reject'])
        self.assertTrue(drive.eval_predicate(card['AcceptIf'], 'pass', self.dir)[0])
        self.assertTrue(drive.eval_predicate(card['AcceptIf'], 'pass deny', self.dir)[0])
        self.assertTrue(drive.eval_predicate(card['RejectIf'], 'pass deny', self.dir)[0])
        script = self.dir / 'deny.txt'
        script.write_text('pass deny\nu\ngiveup\n')
        with contextlib.redirect_stdout(io.StringIO()):
            state = drive.run(lessons, self.progress, DAY, str(script), max_cards=1)
        self.assertEqual(state['X-1']['next'], '2030-01-11')
        del data['cards']['X-1']
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(tutorial_format.FormatError, 'missing card rows'):
            tutorial_format.parse_lessons(lessons)
        data['cards'] = {'X-1': row, 'X-2': row}
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(tutorial_format.FormatError, 'extra card rows'):
            tutorial_format.parse_lessons(lessons)
        data['cards'] = {'X-1': {**row, 'minutes': 'three'}}
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(tutorial_format.FormatError, 'malformed row'):
            tutorial_format.parse_lessons(lessons)
        check = self.dir / 'sample.check.sh'
        check.write_text('#!/bin/sh\nread -r value\n[ "$value" = pass ]\n')
        check.chmod(0o755)
        data['cards'] = {'X-1': {**row, 'grading': {'mode': 'check',
                                                 'accept': 'check(sample.check.sh)'}}}
        path.write_text(json.dumps(data))
        card = tutorial_format.parse_lessons(lessons)[0]['cards'][0]
        self.assertTrue(drive.eval_predicate(card['AcceptIf'], 'pass', self.dir)[0])
        self.assertFalse(drive.eval_predicate(card['AcceptIf'], 'fail', self.dir)[0])

    def test_cap_and_pretests_in_current_lesson(self):
        out, progress = self.scripted(['guess', ':skip', 'u'], max_cards=2)
        self.assertEqual(len([k for k in progress if k != '_runtime']), 2)
        self.assertLess(out.index('[M1-P1]'), out.index('[M1-C1]'))
        self.assertNotIn('[M1-P2]', out)
        self.assertIn('CONCEPT C1', out)
        self.assertIn('about 20 min left', out)

    def test_story_and_contrast_appear_before_dependent_cards(self):
        out, _ = self.scripted(['guess'] + [':skip', 'u'] * 5, max_cards=6)
        self.assertLess(out.index('The donor nobody could name'), out.index('[M1-C1]'))
        self.assertLess(out.index('| Return choice |'), out.index('[M1-Q2]'))

    def test_all_card_minutes_and_objectives_registered(self):
        totals = []
        for module in launch.discover_modules():
            data = json.loads((Path(module['path']) / 'cards.json').read_text())
            docs = tutorial_format.parse_lessons(module['lessons_path'])
            self.assertEqual(set(data['cards']),
                             {c['id'] for d in docs for c in d['cards']})
            self.assertTrue(all(c['objective'] > 0 and c['minutes'] >= 0
                                for c in data['cards'].values()))
            totals.append(sum(x['minutes'] for x in data['lessons'].values()))
            for lesson, row in data['lessons'].items():
                self.assertEqual(row['minutes'], sum(c['minutes'] for c in data['cards'].values()
                                                    if c['lesson'] == lesson))
        self.assertEqual(len(totals), 7)
        self.assertGreaterEqual(sum(totals), 921)

    def test_commands_and_typo_reprompt(self):
        out, progress = self.scripted(
            [':help', ':status', ':notacommand', 'guess', ':skip', 'maybe', 's'], max_cards=2)
        self.assertIn('unknown command, type :help', out)
        self.assertIn('Please type', out)
        self.assertEqual(progress['M1-C1']['box'], 1)
        self.assertEqual(progress['M1-C1']['next'], '2030-01-11')

    def test_miss_retry_hit_keeps_first_result(self):
        out, progress = self.scripted(
            ['guess', 'noexcept, &&, move, move', 'u', 'retry',
             '&&, noexcept, move, move'], max_cards=2)
        self.assertIn('MISS', out)
        self.assertIn('HIT on retry', out)
        self.assertIn('Key:', out)
        self.assertEqual(progress['M1-C1']['next'], '2030-01-11')
        self.assertEqual(progress['M1-C1']['box'], 1)

    def test_retry_pause_keeps_first_miss(self):
        self.scripted(['guess', 'wrong', 'u', ':quit'], max_cards=2)
        saved = drive.load_progress(self.progress)
        self.assertEqual(saved['M1-C1']['first_attempt'], DAY)
        self.assertEqual(saved['M1-C1']['next'], '2030-01-11')
        self.assertEqual(saved['_runtime']['pending_retry']['card'], 'M1-C1')
        out, resumed = self.scripted(['retry', '&&, noexcept, move, move'], max_cards=1)
        self.assertIn('HIT on retry', out)
        self.assertEqual(resumed['M1-C1']['next'], '2030-01-11')
        self.assertNotIn('pending_retry', resumed['_runtime'])

    def test_assisted_grade_survives_pause(self):
        self.progress.write_text(json.dumps({
            '_runtime': {'box': 0, 'next': drive.NEVER,
                         'pending_self_grade': {'card': 'M1-Q2', 'answer': 'draft',
                                                'assisted': True}},
            'M1-P1': {'box': 1, 'next': drive.NEVER}}))
        out, resumed = self.scripted(['h'], max_cards=1)
        self.assertIn('Resuming [M1-Q2]', out)
        self.assertEqual(resumed['M1-Q2']['box'], 1)
        self.assertEqual(resumed['M1-Q2']['next'], '2030-01-11')

    def test_hint_persists_before_answer_and_confidence(self):
        drive.save_progress(self.progress, {
            'M1-P1': {'box': 1, 'next': drive.NEVER},
            'M1-C1': {'box': 1, 'next': '2030-01-12'},
            '_runtime': {'box': 0, 'next': drive.NEVER,
                         'grade_policy': drive.GRADE_POLICY}})
        self.scripted([':hint', ':quit'], max_cards=1)
        saved = drive.load_progress(self.progress)
        self.assertEqual(saved['_runtime']['assistance']['card'], 'M1-Q1')
        self.scripted(['my answer', ':quit'], max_cards=1)
        saved = drive.load_progress(self.progress)
        self.assertEqual(saved['_runtime']['assistance']['card'], 'M1-Q1')
        _, result = self.scripted(['s', 'h'], max_cards=1)
        self.assertEqual(result['M1-Q1']['box'], 1)
        self.assertEqual(result['M1-Q1']['next'], '2030-01-11')
        self.assertNotIn('assistance', result['_runtime'])

    def test_reset_lesson_restores_teaching_and_prediction(self):
        self.scripted(['guess', 'prediction', ':skip', 'u'], max_cards=2)
        state = drive.load_progress(self.progress)
        self.assertTrue(state['_runtime'].get('shown_blocks'))
        drive.reset_progress(LESSONS, self.progress, DAY,
                             'lesson-01-move-semantics.md')
        state = drive.load_progress(self.progress)
        self.assertFalse(state['_runtime'].get('shown_blocks'))
        self.assertFalse(state['_runtime'].get('shown_concepts'))
        script = self.dir / 'reset-answers.txt'
        script.write_text('guess\nprediction again\n:quit\n')
        with contextlib.redirect_stdout(io.StringIO()) as output:
            drive.run(LESSONS, self.progress, DAY, str(script), max_cards=2,
                      offer_stretch=False)
        self.assertIn('The donor nobody could name', output.getvalue())
        self.assertIn('Predict the result before revealing it: prediction again',
                      output.getvalue())

    def test_worked_result_waits_for_prediction(self):
        script = self.dir / 'prediction.txt'
        script.write_text('guess\nmy prediction\n:quit\n')
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            drive.run(LESSONS, self.progress, DAY, str(script), max_cards=2,
                      offer_stretch=False)
        shown = output.getvalue()
        self.assertLess(shown.index('Predict the result before revealing it: my prediction'),
                        shown.index('Output (Apple clang 21, libc++):'))
        self.assertLess(shown.index('Output (Apple clang 21, libc++):'),
                        shown.index('[M1-C1]'))

    def test_prediction_pause_resumes_before_reveal(self):
        script = self.dir / 'pause-prediction.txt'
        script.write_text('guess\n:lesson\n:quit\n')
        with contextlib.redirect_stdout(io.StringIO()) as first:
            drive.run(LESSONS, self.progress, DAY, str(script), max_cards=2,
                      offer_stretch=False)
        self.assertNotIn('Output (Apple clang 21, libc++):', first.getvalue())
        self.assertIn('Make a prediction before revealing', first.getvalue())
        script.write_text('my prediction\n:quit\n')
        with contextlib.redirect_stdout(io.StringIO()) as second:
            drive.run(LESSONS, self.progress, DAY, str(script), max_cards=2,
                      offer_stretch=False)
        self.assertIn('Predict the result before revealing it: my prediction',
                      second.getvalue())
        self.assertIn('Output (Apple clang 21, libc++):', second.getvalue())

    def test_pending_optional_extension_resumes_without_regrading(self):
        drive.save_progress(self.progress, {
            'M1-P1': {'box': 1, 'next': drive.NEVER},
            'M1-Q2': {'box': 2, 'next': '2030-01-12'},
            '_runtime': {'box': 0, 'next': drive.NEVER,
                         'pending_stretch': {'card': 'M1-Q2'}}})
        out, resumed = self.scripted(['my extension answer'], max_cards=1)
        self.assertIn('Extension key:', out)
        self.assertNotIn('pending_stretch', resumed['_runtime'])
        self.assertEqual(resumed['M1-Q2']['next'], '2030-01-12')

    def test_six_new_card_budget(self):
        script = self.dir / 'budget.txt'
        script.write_text('guess\nmy prediction\n&&, noexcept, move, move\nu\n')
        with contextlib.redirect_stdout(io.StringIO()):
            state = drive.run(LESSONS, self.progress, DAY, str(script),
                              max_cards=12, new_budget=1, offer_stretch=False)
        self.assertIn('M1-C1', state)
        self.assertNotIn('M1-Q1', state)

    def test_review_topics_interleave_within_box(self):
        cards = [{'id': k, 'topic': t, 'box': 1, 'once': False}
                 for k, t in [('a', 'one'), ('b', 'one'), ('c', 'two'), ('d', 'two')]]
        saved = {c['id']: {'box': 1, 'next': DAY} for c in cards}
        self.assertEqual([c['id'] for c in drive.due_order(cards, saved, DAY)],
                         ['a', 'c', 'b', 'd'])

    def test_offline_mixed_exam_selects_studied_cards_and_delay(self):
        from driver_lib import mixed_exam
        module = {'id': '01', 'path': self.dir, 'lessons_path': str(LESSONS)}
        cards = [c for c in drive.parse_cards(LESSONS) if not c['once']][:12]
        drive.save_progress(self.progress, {
            c['id']: {'box': 1, 'next': DAY, 'first_attempt': '2030-01-08'}
            for c in cards})
        selected = mixed_exam.select([module], DAY)
        self.assertEqual(len(selected), 12)
        self.assertEqual({x['card']['id'] for x in selected},
                         {c['id'] for c in cards})
        self.assertTrue(all(x['delay_days'] == 2 for x in selected))

    def test_mixed_exam_records_latest_prior_exposure(self):
        from driver_lib import mixed_exam
        module = {'id': '01', 'path': self.dir, 'lessons_path': str(LESSONS)}
        cards = [c for c in drive.parse_cards(LESSONS) if not c['once']][:12]
        saved = {c['id']: {'box': 2, 'next': DAY, 'first_attempt': '2030-01-01',
                           'last_review': '2030-01-07'} for c in cards}
        saved[cards[0]['id']].pop('last_review')
        drive.save_progress(self.progress, saved)
        selected = {x['card']['id']: x for x in mixed_exam.select([module], DAY)}
        self.assertEqual(selected[cards[1]['id']]['prior_exposure'], '2030-01-07')
        self.assertEqual(selected[cards[1]['id']]['days_since_review'], 3)
        self.assertEqual(selected[cards[1]['id']]['delay_days'], 9)
        # Old progress without a review date reports the interval as unknown.
        self.assertEqual(selected[cards[0]['id']]['prior_exposure'], 'date unknown')
        self.assertIsNone(selected[cards[0]['id']]['days_since_review'])

    def test_offline_mixed_exam_records_delayed_results(self):
        from driver_lib import mixed_exam
        from unittest.mock import patch
        module = {'id': '01', 'path': self.dir, 'lessons_path': str(LESSONS)}
        cards = [c for c in drive.parse_cards(LESSONS)
                 if c['type'] in ('recall', 'apply', 'gotcha', 'synthesize')][:12]
        drive.save_progress(self.progress, {
            c['id']: {'box': 1, 'next': DAY, 'first_attempt': '2030-01-08'}
            for c in cards})
        answers = iter(['wrong', 'unsure', 'm'] * 12)
        output = self.dir / 'progress-exam.json'
        with patch('builtins.input', side_effect=lambda prompt='': next(answers)):
            with contextlib.redirect_stdout(io.StringIO()) as shown:
                mixed_exam.run([module], DAY, output)
        record = json.loads(output.read_text())
        self.assertEqual(len(record['results']), 12)
        self.assertTrue(all(result['delay_days'] == 2 for result in record['results']))
        self.assertIn('Score: 0/12 hits; 12/12 at least one day', shown.getvalue())
        progress = drive.load_progress(self.progress)
        self.assertTrue(all(progress[card['id']]['next'] == '2030-01-11'
                            for card in cards))

    def test_exam_pause_preserves_revealed_answer_and_updates_review(self):
        from driver_lib import mixed_exam
        from unittest.mock import patch
        module = {'id': '01', 'path': self.dir, 'lessons_path': str(LESSONS)}
        cards = [c for c in drive.parse_cards(LESSONS)
                 if c['type'] in ('recall', 'apply', 'gotcha', 'synthesize')][:12]
        drive.save_progress(self.progress, {
            c['id']: {'box': 4, 'next': '2030-03-01', 'first_attempt': '2030-01-08'}
            for c in cards})
        output = self.dir / 'progress-exam.json'
        with patch('builtins.input', side_effect=['wrong original', 'sure', ':quit']):
            with contextlib.redirect_stdout(io.StringIO()):
                mixed_exam.run([module], DAY, output)
        record = json.loads(output.read_text())
        self.assertEqual(record['pending']['answer'], 'wrong original')
        self.assertTrue(record['pending']['answer_exposed'])
        self.assertEqual(record['results'], [])
        with patch('builtins.input', side_effect=['m'] + ['wrong', 'unsure', 'm'] * 11):
            with contextlib.redirect_stdout(io.StringIO()) as shown:
                mixed_exam.run([module], DAY, output)
        record = json.loads(output.read_text())
        self.assertEqual(record['results'][0]['answer'], 'wrong original')
        self.assertIn('Your original answer: wrong original', shown.getvalue())
        state = drive.load_progress(self.progress)[record['results'][0]['id']]
        self.assertEqual(state['first_attempt'], '2030-01-08')
        self.assertEqual(state['box'], 1)
        self.assertEqual(state['next'], '2030-01-11')
        self.assertTrue(state['sure_miss'])

    def test_pending_answer_shown_on_resume(self):
        self.scripted(['guess', 'my original answer', 'u'], max_cards=2)
        # The self-grade card M1-Q2 can be tested with an explicit compatible record.
        state = drive.load_progress(self.progress)
        state['_runtime']['pending_self_grade'] = {
            'card': 'M1-Q2', 'answer': 'my original answer',
            'sure': False, 'answer_exposed': True}
        drive.save_progress(self.progress, state)
        out, progress = self.scripted(['wrong', 'h'], max_cards=1)
        self.assertIn('Your earlier answer: my original answer', out)
        self.assertNotIn('pending_self_grade', progress['_runtime'])

    def test_review_budget_advances_new_cards(self):
        cards = drive.parse_cards(LESSONS)
        state = {'_runtime': {'box': 0, 'next': '9999-12-31',
                              'version': 1, 'last_date': DAY}}
        state[cards[0]['id']] = {'box': 1, 'next': '9999-12-31'}
        for card in cards[1:6]:
            state[card['id']] = {'box': 1, 'next': '2030-01-11'}
        drive.save_progress(self.progress, state)
        out, result = self.scripted([':skip', 'u', ':skip', 'u',
                                     ':skip', 'u', ':skip', 'u',
                                     ':skip', 'u'], date='2030-01-11',
                                    max_cards=5, reviews=4)
        # Four reviews cannot consume the fifth card slot.
        self.assertIn('M1-G2', result)
        self.assertIn('Sitting limit reached', out)
        self.assertEqual(sum(f'[{c["id"]}]' in out for c in cards[1:6]), 4)

    def test_saved_date_guidance(self):
        self.scripted(['guess'], date='2030-01-11', max_cards=1)
        with self.assertRaisesRegex(ValueError, 'Saved progress is dated 2030-01-11'):
            self.scripted(['guess'], date=DAY)

    def test_old_progress_loads(self):
        self.progress.write_text(json.dumps({'M1-P1': {'box': 1, 'next': '9999-12-31'}}))
        self.assertIn('M1-P1', drive.load_progress(self.progress))

    def test_scripted_eof_stops(self):
        out, progress = self.scripted(['guess'], max_cards=3)
        self.assertIn('Scripted answers exhausted', out)
        self.assertIn('M1-P1', progress)

    def test_multiline_answer_cannot_become_grade(self):
        out, progress = self.scripted(
            ['guess', '&&', 'noexcept', 'move', 'move'], max_cards=2)
        self.assertIn('Please type', out)
        self.assertIn('Scripted answers exhausted', out)
        self.assertNotIn('M1-C1', progress)

    def test_quit_and_daily_return(self):
        out, progress = self.scripted(['guess', ':quit'])
        self.assertIn('Stopped; completed work remains saved.', out)
        self.assertIn('M1-P1', progress)
        self.assertNotIn('M1-C1', progress)
        self.scripted(['&&, noexcept, move, move', 'u'], max_cards=1)
        self.assertEqual(drive.load_progress(self.progress)['M1-C1']['next'], '2030-01-12')
        out, _ = self.scripted([':quit'], date='2030-01-11', max_cards=1)
        self.assertNotIn('[M1-C1]', out)
        out, _ = self.scripted([':skip', 'u', ':skip', 'u'], date='2030-01-12', max_cards=2)
        self.assertIn('[M1-C1]', out)

    def test_full_first_lesson_script(self):
        first = tutorial_format.parse_lessons(LESSONS)[0]
        answers = ['guess', 'noexcept, &&, move, move', 'u', 'retry',
                   '&&, noexcept, move, move'] + [':skip', 'u'] * (len(first['cards']) - 2)
        out, progress = self.scripted(answers, max_cards=13)
        self.assertTrue(all(c['id'] in progress for c in first['cards']))
        self.assertIn('HIT on retry', out)
        self.assertIn('## Remember', first['text'])
        self.assertIn('Remember', out)
        self.assertEqual(progress['M1-C1']['next'], '2030-01-11')

    def test_disclosure_markup_is_not_prompt_text(self):
        doc = tutorial_format.parse_lessons(LESSONS)[0]
        card = doc['cards'][0]
        self.assertNotIn('<details>', card['Q'])
        self.assertNotIn('</details>', '\n'.join(card['Key']))

    def test_cli_help_describes_budgets(self):
        result = subprocess.run([sys.executable, str(ROOT / 'drive.py'), '--help'],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('Example:', result.stdout)
        for option in ('--progress', '--today', '--auto', '--max-cards',
                       '--max-minutes', '--reviews', '--reset', '--reset-lesson'):
            self.assertIn(option, result.stdout)

    def test_runnable_lab_workspace_retry(self):
        lessons = self.dir / 'lessons'
        checks = self.dir / 'checks'
        lessons.mkdir()
        checks.mkdir()
        (lessons / 'lesson-01.md').write_text(
            '# Lesson 01 — Lab\n\nTime: 5 min\n\n'
            '### CARD LAB-1  [type: lab]  [box: 1]\n'
            'Q: Do the lab now in labs/sample/work.\n'
            'Hint: Check the workspace path.\n'
            'Key:\n  - The default workspace passes.\n'
            'AcceptIf: check(checks/lab.check.sh)\n'
            'Source: local lab\n')
        check = checks / 'lab.check.sh'
        check.write_text('#!/bin/bash\nread -r path\n'
                         'if [ "$path" = labs/sample/work ]; then echo HIT; exit 0; fi\n'
                         'echo "MISS: expected labs/sample/work; got $path"; exit 1\n')
        check.chmod(0o755)
        script = self.dir / 'lab-answers.txt'
        script.write_text('labs/sample/wrong\nu\nretry\n\n')
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            result = drive.run(lessons, self.progress, DAY, str(script), max_cards=1)
        out = output.getvalue()
        self.assertIn('press Enter to run the check on labs/sample/work', out)
        self.assertLess(out.index('HIT on retry'), out.index('Key:'))
        self.assertEqual(result['LAB-1']['next'], '2030-01-11')

    def test_paper_separates_keys(self):
        with contextlib.redirect_stdout(io.StringIO()):
            launch.write_paper(launch.discover_modules())
        questions = (ROOT / 'paper/01-questions.md').read_text()
        answers = (ROOT / 'paper/01-answers.md').read_text()
        self.assertIn('M1-C1', questions)
        self.assertNotIn('AcceptIf:', questions)
        self.assertNotIn('RejectIf:', questions)
        self.assertIn('The donor nobody could name', questions)
        self.assertIn('| Return choice |', questions)
        self.assertNotRegex(questions, r'(?m)^Key:')
        self.assertIn('M1-C1', answers)

    def test_keyword_denial_uses_self_grade_and_old_grade_returns(self):
        lessons = self.dir / 'lessons'
        lessons.mkdir()
        (lessons / 'lesson-01.md').write_text(
            '# Lesson 01\n\n### CARD M1-Q3 [type: recall]\n'
            'Q: Explain ownership.\nKey:\n  - unique ownership transfers.\n'
            'AcceptIf: regex(/unique/)\nSource: local\n')
        card = tutorial_format.parse_lessons(lessons)[0]['cards'][0]
        self.assertTrue(drive.needs_self_grade(card))
        old = {'M1-Q3': {'box': 4, 'next': '2030-10-01'},
               '_runtime': {'box': 0, 'next': drive.NEVER, 'version': 1,
                            'last_date': DAY}}
        drive.save_progress(self.progress, old)
        script = self.dir / 'answers.txt'
        script.write_text('I deny every claim: unique\nu\nm\n')
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            state = drive.run(lessons, self.progress, DAY, str(script), max_cards=1)
        out = output.getvalue()
        self.assertIn('Self-grade', out)
        self.assertIn('M1-Q3', state)
        self.assertEqual(state['M1-Q3']['box'], 1)
        self.assertEqual(state['M1-Q3']['next'], '2030-01-11')

    def test_constrained_regex_can_grade_the_whole_answer(self):
        lessons = ROOT / 'modules/06-hft-patterns/lessons'
        card = next(c for d in tutorial_format.parse_lessons(lessons)
                    for c in d['cards'] if c['id'] == 'M6-C3')
        self.assertFalse(drive.needs_self_grade(card))
        self.assertTrue(drive.eval_predicate(card['AcceptIf'], 'interrupt, polling', str(lessons.parent))[0])
        self.assertFalse(drive.eval_predicate(card['AcceptIf'], 'I deny: interrupt, polling', str(lessons.parent))[0])

    def test_rewritten_completion_grading_policy(self):
        all_cards = {c['id']: c for module in launch.discover_modules()
                     for doc in tutorial_format.parse_lessons(module['lessons_path'])
                     for c in doc['cards']}
        for cid in ('M2-C4', 'M3-C4', 'M3-C5', 'M6-C2'):
            self.assertFalse(drive.needs_self_grade(all_cards[cid]), cid)
        for cid in ('M1-C4', 'M5-C4'):
            self.assertTrue(drive.needs_self_grade(all_cards[cid]), cid)

    def test_setup_problem_never_records_a_miss(self):
        lessons = self.dir / 'lessons'
        checks = self.dir / 'checks'
        lessons.mkdir(); checks.mkdir()
        (lessons / 'lesson-01.md').write_text(
            '# Lesson 01\n\n### CARD LAB-1 [type: completion]\n'
            'Q: Run the check.\nKey:\n  - ok\n'
            'AcceptIf: check(checks/fail.check.sh)\nSource: local\n')
        check = checks / 'fail.check.sh'
        check.write_text('#!/bin/sh\necho "SETUP PROBLEM: compiler unavailable"\nexit 2\n')
        check.chmod(0o755)
        script = self.dir / 'answers.txt'
        script.write_text('answer\nu\n')
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            state = drive.run(lessons, self.progress, DAY, str(script), max_cards=1)
        self.assertIn('compiler unavailable', output.getvalue())
        self.assertNotIn('LAB-1', state)

    def test_launcher_resumes_unfinished_interaction(self):
        module = {'id': '01', 'path': str(self.dir), 'lessons_path': str(LESSONS),
                  'title': 'Test'}
        ids = [c['id'] for d in tutorial_format.parse_lessons(LESSONS)
               for c in d['cards']]
        state = {cid: {'box': 1, 'next': '2030-01-20'} for cid in ids}
        state['_runtime'] = {'pending_self_grade': {'card': ids[-1]}}
        drive.save_progress(self.progress, state)
        self.assertEqual(launch.pick_next_module([module], DAY)[1],
                         'unfinished interaction')

    def test_pool_cyclic_fault_rejected_by_behavior(self):
        path = ROOT / 'modules/05-memory-perf/checks/m5-c6.impl.sh'
        for answer, expected in [('next, free_, free_', 0), ('next, n, free_', 1)]:
            with self.subTest(answer=answer):
                result = subprocess.run(['bash', str(path)], input=answer,
                                        text=True, capture_output=True, timeout=45)
                self.assertEqual(result.returncode == 0, expected == 0,
                                 result.stdout + result.stderr)

    def test_checker_accepts_valid_alternatives(self):
        cases = [('m1-c5', 'struct, uint8_t'),
                 ('m5-c5', 'literals'),
                 ('m5-c5', 'literals::chrono_literals'),
                 ('m5-c5', 'chrono'),
                 ('m7-c3', '-fsanitize=address'),
                 ('m2-c2', '`consteval`'),
                 ('m1-c3', 'int, double, const char*, const std::vector<int>&')]
        for name, answer in cases:
            with self.subTest(name=name, answer=answer):
                path = next(ROOT.glob(f'modules/*/checks/{name}.check.sh'))
                result = subprocess.run([str(path)], input=answer, text=True,
                                        capture_output=True, timeout=45)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_optional_check_classifies_broken_compiler_output_as_setup(self):
        # A fake compiler breaks even the known-good specimen.
        bin_dir = self.dir / 'bin'
        bin_dir.mkdir()
        compiler = bin_dir / 'clang++'
        compiler.write_text('#!/bin/sh\nwhile [ "$1" != "-o" ]; do shift; done\n'
                            'shift\nprintf "#!/bin/sh\\nprintf 2\\n" > "$1"\n'
                            'chmod +x "$1"\n')
        compiler.chmod(0o755)
        path = ROOT / 'modules/04-stl-modern/checks/m4-c2.check.sh'
        result = subprocess.run([str(path)], input='optional, nullopt, idx',
                                text=True, capture_output=True,
                                env={**os.environ, 'PATH': str(bin_dir) + ':' + os.environ['PATH']},
                                timeout=45)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn('Known-good specimen failed', result.stdout)

    def test_missing_standard_header_is_setup_problem(self):
        bin_dir = self.dir / 'bin'
        bin_dir.mkdir()
        compiler = bin_dir / 'clang++'
        actual = shutil.which('clang++')
        compiler.write_text('#!/bin/sh\nfor arg do\n case "$arg" in *.cpp) '
                            'if grep -q "^#include <utility>" "$arg"; then '
                            'echo "fatal error: utility file not found" >&2; exit 1; fi;; '
                            'esac\ndone\nexec "' + actual + '" "$@"\n')
        compiler.chmod(0o755)
        path = ROOT / 'modules/01-foundations/checks/m1-c1.check.sh'
        result = subprocess.run([str(path)], input='&&, noexcept, move, move',
                                text=True, capture_output=True,
                                env={**os.environ, 'PATH': str(bin_dir) + ':' + os.environ['PATH']},
                                timeout=45)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn('SETUP PROBLEM', result.stdout)
        self.assertIn('utility file not found', result.stdout)

    def test_missing_asan_runtime_is_setup_problem(self):
        bin_dir = self.dir / 'bin'
        bin_dir.mkdir()
        compiler = bin_dir / 'clang++'
        actual = shutil.which('clang++')
        compiler.write_text('#!/bin/sh\nfor arg do\n'
                            ' if [ "$arg" = "-fsanitize=address" ]; then '
                            'echo "AddressSanitizer runtime unavailable" >&2; exit 1; fi\n'
                            'done\nexec "' + actual + '" "$@"\n')
        compiler.chmod(0o755)
        path = ROOT / 'modules/07-architecture/checks/m7-c3.check.sh'
        result = subprocess.run([str(path)], input='sanitize, address',
                                text=True, capture_output=True,
                                env={**os.environ, 'PATH': str(bin_dir) + ':' + os.environ['PATH']},
                                timeout=45)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn('SETUP PROBLEM', result.stdout)
        self.assertIn('AddressSanitizer runtime unavailable', result.stdout)

    def test_spsc_abort_diagnostic_is_not_learner_feedback(self):
        bin_dir = self.dir / 'bin'
        bin_dir.mkdir()
        compiler = bin_dir / 'clang++'
        compiler.write_text('#!/bin/sh\nwhile [ "$1" != "-o" ]; do shift; done\n'
                            'shift\nprintf "#!/bin/sh\\nulimit -c 0\\nkill -ABRT \\$\\$\\n" > "$1"\n'
                            'chmod +x "$1"\n')
        compiler.chmod(0o755)
        check = ROOT / 'modules/06-hft-patterns/checks/spsc_lab.check.sh'
        result = subprocess.run([str(check)],
                                input='modules/06-hft-patterns/lab/spsc/solution',
                                text=True, capture_output=True,
                                env={**os.environ, 'PATH': str(bin_dir) + ':' + os.environ['PATH']},
                                timeout=20)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn('SETUP PROBLEM', result.stdout)
        self.assertNotIn('Abort trap', result.stdout)

    def test_pool_alignment_oracle_rejects_size_only_fault(self):
        source = ROOT / 'modules/05-memory-perf/lab/solution/pool_lab.cpp'
        if not source.exists():
            self.skipTest('pool lab solution is still being authored')
        original = source.read_text()
        needle = 'return bytes == size_ && align == align_;'
        self.assertIn(needle, original)
        probe = ('#define main pool_example_main\n#include "pool_lab.cpp"\n#undef main\n'
                 'int main() { counting_resource upstream; fixed_pool pool(2, &upstream);\n'
                 '  void* a = pool.allocate(32, 8); pool.deallocate(a, 32, 8);\n'
                 '  void* b = pool.allocate(32, 16);\n'
                 '  bool forwarded = pool.forwarded() == 1;\n'
                 '  pool.deallocate(b, 32, 16); return forwarded ? 0 : 1; }\n')
        for label, body, expect in (
                ('reference', original, True),
                ('size_only_fault', original.replace(needle, 'return bytes == size_;'), False)):
            with self.subTest(label=label):
                folder = self.dir / label
                folder.mkdir()
                (folder / 'pool_lab.cpp').write_text(body)
                (folder / 'probe.cpp').write_text(probe)
                binary = folder / 'probe'
                build = subprocess.run(['clang++', '-std=c++20', '-O1',
                                        str(folder / 'probe.cpp'), '-o', str(binary)],
                                       capture_output=True, text=True, timeout=45)
                self.assertEqual(build.returncode, 0, build.stderr)
                run = subprocess.run([str(binary)], capture_output=True,
                                     text=True, timeout=20)
                self.assertEqual(run.returncode == 0, expect,
                                 run.stdout + run.stderr)


class CheckTableTests(unittest.TestCase):
    def test_module_03_04_lab_build_commands(self):
        labs = [p for module in ('03-concurrency', '04-stl-modern')
                for p in sorted((ROOT / 'modules' / module / 'lab').glob('*.cpp'))]
        self.assertEqual(len(labs), 25)   # 23 + the LPA labs handoff_tail.cpp and expected_tail.cpp
        with tempfile.TemporaryDirectory() as tmp:
            for path in labs:
                lines = path.read_text().splitlines()
                self.assertTrue(lines[2].startswith('//   clang++ '), path)
                build = lines[2].removeprefix('//   ').split(' && ')[0]
                with self.subTest(path=path.name):
                    result = subprocess.run(build, shell=True, cwd=ROOT,
                                            env={**os.environ, 'TMPDIR': tmp},
                                            text=True, capture_output=True,
                                            timeout=60)
                    self.assertEqual(result.returncode, 0, result.stderr)

    def test_every_check_correct_blank_wrong(self):
        cases = json.loads((ROOT / 'tools/check_cases.json').read_text())
        paths = sorted(ROOT.glob('modules/*/checks/*.check.sh'))
        self.assertEqual({p.name.split('.')[0] for p in paths}, set(cases))
        with tempfile.TemporaryDirectory() as tmp:
            env = {**os.environ, 'TMPDIR': tmp, 'PYTHONDONTWRITEBYTECODE': '1'}
            for path in paths:
                name = path.name.split('.')[0]
                trials = [('correct', cases[name][0], True),
                          ('blank', '', False), ('wrong', cases[name][1], False)]
                trials += [(f'adversarial-{i}', answer, False)
                           for i, answer in enumerate(cases[name][2] if len(cases[name]) > 2 else [])]
                for label, answer, success in trials:
                    with self.subTest(name=name, case=label):
                        result = subprocess.run([str(path)], input=answer,
                                                text=True, capture_output=True, env=env,
                                                timeout=45)
                        self.assertEqual(result.returncode == 0, success,
                                         result.stdout + result.stderr)
                        lines = result.stdout.splitlines()
                        data = json.loads(lines[0])
                        self.assertEqual(data['status'] == 'HIT', success)
                        self.assertIn(data['status'], lines[1])
                        if not success:
                            self.assertTrue(data['criteria'])


if __name__ == '__main__':
    unittest.main()
