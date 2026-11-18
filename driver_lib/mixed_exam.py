"""Offline delayed mixed assessment over previously studied published cards."""
import datetime
import json
from pathlib import Path

import drive
from driver_lib import tutorial_format


def select(modules, today, limit=12):
    """Low boxes first; alternate topics within a box when possible."""
    pool = []
    for module in modules:
        path = Path(module['path']) / 'progress.json'
        if not path.exists():
            continue
        progress = drive.load_progress(path)
        for doc in tutorial_format.parse_lessons(module['lessons_path']):
            for card in doc['cards']:
                state = progress.get(card['id'])
                if not state or card['once']:
                    continue
                first = state.get('first_attempt')
                delay = ((datetime.date.fromisoformat(today) -
                          datetime.date.fromisoformat(first)).days if first else None)
                latest = state.get('last_review')
                review_delay = ((datetime.date.fromisoformat(today) -
                                 datetime.date.fromisoformat(latest)).days if latest else None)
                pool.append({'card': card, 'module': module['id'],
                             'base': str(Path(module['path'])),
                             'box': state['box'], 'delay_days': delay,
                             'prior_exposure': latest or 'date unknown',
                             'days_since_review': review_delay})
    pool.sort(key=lambda item: (item['box'], item['module'], item['card']['id']))
    chosen = []
    while pool and len(chosen) < limit:
        box = pool[0]['box']
        group = [item for item in pool if item['box'] == box]
        previous = chosen[-1]['card']['topic'] if chosen else None
        index = next((i for i, item in enumerate(group)
                      if item['card']['topic'] != previous), 0)
        item = group[index]
        pool.remove(item)
        chosen.append(item)
    return chosen


def run(modules, today, output):
    """Record each answer and its elapsed acquisition delay after grading."""
    path = Path(output)
    if path.exists():
        record = json.loads(path.read_text())
        if record.get('today') != today:
            raise ValueError('Exam progress has a different date; choose another --exam-progress file')
    else:
        chosen = select(modules, today)
        if len(chosen) < 12:
            print(f'Need 12 previously studied cards; found {len(chosen)}.')
            return
        record = {'today': today, 'cards': [
            {k: item[k] for k in ('module', 'box', 'delay_days', 'prior_exposure',
                                  'days_since_review')}
            | {'id': item['card']['id']} for item in chosen], 'results': []}
        drive.save_progress(path, record)
    lookup = {card['id']: (card, str(Path(module['path'])))
              for module in modules
              for doc in tutorial_format.parse_lessons(module['lessons_path'])
              for card in doc['cards']}
    def apply_schedule(result):
        if result.get('schedule_applied'):
            return
        item = next(entry for entry in record['cards'] if entry['id'] == result['id'])
        module = next(entry for entry in modules if entry['id'] == item['module'])
        progress_path = Path(module['path']) / 'progress.json'
        progress = drive.load_progress(progress_path)
        state = progress.get(result['id'])
        if state:
            if result['grade'] in ('m', 'p'):
                if result['grade'] == 'm':
                    state['box'] = 1
                    if result['confidence'] == 'sure':
                        state['sure_miss'] = True
                state['next'] = (datetime.date.fromisoformat(today) +
                                 datetime.timedelta(days=1)).isoformat()
            state['last_review'] = today
            drive.save_progress(progress_path, progress)
        result['schedule_applied'] = True
        drive.save_progress(path, record)

    for result in record['results']:
        apply_schedule(result)
    for item in record['cards'][len(record['results']):]:
        card, base = lookup[item['id']]
        pending = record.get('pending')
        if not pending:
            print(f"\n[{item['id']}] {card['Q']}")
            try:
                answer = input('Your answer (from memory; :quit to pause): ')
            except (EOFError, KeyboardInterrupt):
                print('Paused; exam progress saved.')
                return
            if answer.strip() in (':quit', '/stop'):
                print('Paused; exam progress saved.')
                return
            if not answer.strip():
                print('Give an answer, or :quit to pause.')
                return
            try:
                confidence = input('Confidence [sure/unsure; :quit to pause]: ').strip().lower()
            except (EOFError, KeyboardInterrupt):
                print('Paused; exam progress saved.')
                return
            if confidence in (':quit', '/stop'):
                print('Paused; exam progress saved.')
                return
            if confidence not in ('sure', 'unsure', 's', 'u'):
                print('Type sure or unsure; this answer was not recorded.')
                return
            confidence = 'sure' if confidence in ('sure', 's') else 'unsure'
            automatic = bool(card.get('AcceptIf')) and not drive.needs_self_grade(card)
            try:
                if automatic:
                    hit, _ = drive.eval_predicate(card['AcceptIf'], answer, base)
                    if card.get('RejectIf') and drive.eval_predicate(card['RejectIf'], answer, base)[0]:
                        hit = False
                    grade = 'h' if hit else 'm'
                else:
                    grade = None
            except drive.CheckProblem as exc:
                print(f'SETUP PROBLEM: {exc}; this card was not graded.')
                return
            pending = {'id': item['id'], 'answer': answer, 'confidence': confidence,
                       'attempt_date': today, 'answer_exposed': True, 'grade': grade}
            record['pending'] = pending
            drive.save_progress(path, record)
        else:
            print(f"\nResuming [{item['id']}]. Your original answer: {pending['answer']}")
        print('Key:')
        for bullet in card['Key']:
            print('  - ' + bullet)
        grade = pending['grade']
        if grade is None:
            while True:
                try:
                    grade = input('Self-grade [h]it / [p]artial / [m]iss / :quit: ').strip().lower()
                except (EOFError, KeyboardInterrupt):
                    print('Paused; original answer saved. Resume to grade it.')
                    return
                if grade in (':quit', '/stop'):
                    print('Paused; original answer saved. Resume to grade it.')
                    return
                if grade in ('h', 'p', 'm'):
                    break
        record['results'].append({'id': item['id'], 'answer': pending['answer'],
                                  'confidence': pending['confidence'],
                                  'attempt_date': pending['attempt_date'],
                                  'answer_exposed': pending['answer_exposed'],
                                  'grade': grade, 'objective': card['objective'],
                                  'delay_days': item['delay_days'],
                                  'days_since_review': item.get('days_since_review'),
                                  'prior_box': item['box'], 'schedule_applied': False})
        record.pop('pending', None)
        drive.save_progress(path, record)
        apply_schedule(record['results'][-1])
    hits = sum(result['grade'] == 'h' for result in record['results'])
    delayed = sum((result['delay_days'] or 0) >= 1 for result in record['results'])
    print(f'\nScore: {hits}/12 hits; {delayed}/12 at least one day after acquisition.')
    by_objective = {}
    for result in record['results']:
        key = result['id'].split('-')[0] + '/' + str(result['objective'])
        by_objective.setdefault(key, []).append(result['grade'])
    for objective, grades in sorted(by_objective.items()):
        print(f"{objective}: {grades.count('h')}/{len(grades)} hits")
    for module in sorted({item['module'] for item in record['cards']}):
        grades = [result['grade'] for result, item in
                  zip(record['results'], record['cards']) if item['module'] == module]
        print(f"Module {module}: {grades.count('h')}/{len(grades)} hits")
    for result in record['results']:
        if result['grade'] != 'h':
            card, _ = lookup[result['id']]
            print(f"Review {result['id']}: {card['Source']}")
    if delayed < 12:
        print('Same-day hits are practice, not evidence of delayed mastery.')
