"""Fixed, separate v2 assessment. Responses persist before any key is shown."""
import datetime
import hashlib
import json
from pathlib import Path

try:
    from . import drive, v2_session
except ImportError:
    import drive, v2_session


def load_form(tutorial_dir):
    path = Path(tutorial_dir) / 'assessment' / 'form.json'
    form = json.loads(path.read_text(encoding='utf-8'))
    if not form.get('id') or not form.get('prerequisites') or not form.get('items'):
        raise ValueError('assessment needs id, prerequisites and items')
    ids = [item['id'] for item in form['items']]
    if len(ids) != len(set(ids)):
        raise ValueError('duplicate assessment item ID')
    for item in form['items']:
        if item['objective'] not in form['prerequisites'] or item['kind'] not in ('retention', 'transfer') or not item.get('key'):
            raise ValueError(f"invalid assessment item {item['id']}")
    return form


def paper(form):
    lines = [f"# Assessment — {form['id']}", '',
             'Time: about 8 minutes. Stop and resume after any response.',
             'Prerequisites: ' + ', '.join(form['prerequisites']),
             'Write every response before opening the keys. Record the actual date and time.',
             'Reusing this form means its transfer tasks are exposed, not novel.', '']
    for item in form['items']:
        lines += [f"## {item['id']} — {item['kind']} — {item['objective']}", item['question'], '']
    lines += ['# Keys — open after all responses', '']
    for item in form['items']:
        lines += [f"## {item['id']}"] + [f"- {part}" for part in item['key']] + ['']
    return '\n'.join(lines).rstrip() + '\n'


def run(lessons_dir, progress_path, today, replay=None, wall_clock=None,
        ask_confidence=False):
    root = Path(lessons_dir).resolve().parent
    form = load_form(root)
    docs = v2_session.fmt.parse_lessons(lessons_dir)
    for label in form['prerequisites']:
        if sum(c['id'] == label for d in docs for c in d['concepts']) > 1:
            raise ValueError(f'ambiguous assessment prerequisite {label}')
    state = drive.load_progress(progress_path)
    runtime = dict(state.get('_runtime', {'box': 0, 'next': drive.NEVER, 'version': 2}))
    if runtime.get('version') not in (1, 2): raise ValueError('unsupported runtime version')
    runtime.update({'box': 0, 'next': drive.NEVER, 'version': 2})
    completed = set(runtime.get('completed_steps', []))
    missing = [obj for obj in form['prerequisites']
               if not any(step.endswith(':' + obj + ':1') and step.startswith('teach:') for step in completed)]
    if missing: raise ValueError('assessment prerequisites not met: ' + ', '.join(missing))
    runs = runtime.setdefault('assessments', {}).setdefault(form['id'], [])
    active = next((r for r in reversed(runs) if not r.get('complete')), None)
    if active is None:
        active = {'id': f"{form['id']}:{len(runs)+1}", 'form': form['id'],
                  'form_exposed': bool(runs), 'responses': {}, 'grades': {}, 'complete': False}
        runs.append(active)
    clock = wall_clock or (lambda: datetime.datetime.now(datetime.timezone.utc))
    lines = iter(Path(replay).read_text(encoding='utf-8').splitlines()) if replay else None
    def persist():
        state['_runtime'] = runtime
        drive.save_progress(progress_path, state)
    def ask(tag, prompt):
        if lines is not None:
            line = next(lines, None)
            if line is None: raise drive.SessionStopped('Replay exhausted; assessment responses saved.')
            actual, sep, value = line.partition('\t')
            if not sep or actual != tag: raise ValueError(f'replay tag mismatch: expected {tag}, got {actual}')
            print(prompt + value)
        else:
            try: value = input(prompt)
            except (EOFError, KeyboardInterrupt): raise drive.SessionStopped('Interrupted; assessment responses saved.')
        if value.strip() == ':quit': raise drive.SessionStopped('Stopped; assessment responses saved.')
        return value
    def confidence():
        nonlocal lines
        if lines is not None:
            line = next(lines, None)
            if line is None: return None, None
            if line.startswith('confidence\t'):
                value = line.partition('\t')[2]
                print('Response confidence: ' + value)
                return v2_session.parse_confidence(value)
            lines = iter([line, *lines])
            return None, None
        if ask_confidence:
            while True:
                try: return v2_session.parse_confidence(ask('confidence', 'Response confidence [0–1, %, sure/unsure, blank]: '))
                except ValueError as exc: print(exc)
        return None, None
    def timestamp(): return clock().isoformat()
    print(f"Assessment {form['id']} — {'exposed repeated form' if active['form_exposed'] else 'first use'}")
    try:
        for item in form['items']:
            if item['id'] in active['responses']: continue
            print(f"[{item['id']}] {item['question']}")
            answer = ask('assessment_answer', 'Your answer: ')
            numeric_confidence, qualitative_confidence = confidence()
            exposure_times = [value for key, value in runtime.get('exposures', {}).items()
                              if key == item['objective'] or key.split(':')[1:2] == [item['objective']]]
            exposure = max(exposure_times) if exposure_times else None
            at = timestamp()
            delay = None
            if exposure:
                elapsed = (datetime.datetime.fromisoformat(at) - datetime.datetime.fromisoformat(exposure)).total_seconds()
                delay = elapsed if elapsed >= 0 else None
            active['responses'][item['id']] = {'answer': answer, 'answered_at': at,
                'exposure_at': exposure, 'delay_seconds': delay,
                'confidence': numeric_confidence,
                'qualitative_confidence': qualitative_confidence}
            persist()
        print('All responses collected. Keys follow; grade each against every bullet.')
        for item in form['items']:
            if item['id'] in active['grades']: continue
            print(f"Key for {item['id']}:")
            for part in item['key']: print('  - ' + part)
            while True:
                grade = ask('assessment_grade', 'Self-grade [h]it / [p]artial / [m]iss: ').strip().lower()
                if grade in ('h', 'p', 'm'): break
                print('Enter h, p or m.')
            active['grades'][item['id']] = grade
            response = active['responses'][item['id']]
            event = {'id': active['id'] + ':' + item['id'], 'card': item['id'],
                     'card_version': hashlib.sha256(json.dumps(item, sort_keys=True).encode()).hexdigest()[:12],
                     'objective': [item['objective']], 'mode': 'assessment', 'kind': item['kind'],
                     'grade': grade, 'grading_source': 'self', 'assistance': [],
                     'feedback_exposed': True, 'answered_at': response['answered_at'],
                     'feedback_at': timestamp(), 'exposure_at': response['exposure_at'],
                     'delay_seconds': response['delay_seconds'], 'form': form['id'],
                     'form_exposed': active['form_exposed'],
                     'confidence': response['confidence'],
                     'qualitative_confidence': response['qualitative_confidence']}
            runtime.setdefault('attempt_records', []).append(event)
            for concept_key in list(runtime.get('exposures', {})):
                if concept_key == item['objective'] or concept_key.split(':')[1:2] == [item['objective']]:
                    runtime['exposures'][concept_key] = timestamp()
            persist()
        active['complete'] = True
        persist()
        print('Assessment complete.')
    except drive.SessionStopped as exc:
        print(str(exc))
    return state
