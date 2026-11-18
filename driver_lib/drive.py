#!/usr/bin/env python3
"""T1 deterministic tutorial driver — NO model, NO network.

Reads the same lesson cards a model-driven session (T2/T3) would read, and runs
the pedagogy mechanically: pretest-first, spaced (Leitner) scheduling, one
question at a time, attempt-before-reveal, box updates, resumable progress. This
is the proof that a tutorial's pedagogy lives in the ARTIFACT, not the model.

Determinism contract: same progress.json + same --today => same eligible order
and resulting progress.json for the same answers. V2 also tracks explicit
instructional steps; unversioned tutorials show their lesson text before new
practice without changing their prompts, replay lines, or box scheduling.

Usage:
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD --auto answers.txt   # for eval/CI
    ./drive.py lessons/ --print-t0 > STUDY.md                         # complete paper guide
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD --max-cards 4
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD --reset
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD --reset-lesson lesson-01.md

Reset commands archive the existing progress file beside it as
<name>.<today>.bak.json (or .bak.N.json if occupied), then atomically save
empty progress or remove one lesson's state. They exit without a session.
V2 lesson resets retain attempt records and record a reset event; full-reset
attempt history remains in the archive. Full reset archives even malformed
progress; lesson reset requires readable progress. A missing file is untouched.
Archives use a byte copy when hard links are unavailable.

Card grading tiers (in order of trustworthiness):
    1. `AcceptIf: check(path.sh)` — an executable structural test (the script
       gets the learner's answer on stdin; exit 0 = HIT). Strongest: it runs
       the learner's code/answer for real.
    2. `AcceptIf:` regex + `RejectIf:` regex — a keyword gate PLUS a
       contradiction guard. RejectIf matching forces a MISS even if AcceptIf
       matches (catches "…[1, 2] but they do NOT share state"). A regex can
       never fully grade semantics — this is a coarse gate, honest about it.
    3. Neither — reveal the Key and ask the learner to self-grade (T0/T1
       contract). Never silently skipped.

Pretest cards (`[type: pretest]`/`[once]` tag): asked FIRST, before everything
else, exactly once ever. The learner commits a guess, the Key is revealed, and
the card is never rescheduled — the wrong guess + reveal IS the intervention
(prediction error), not an assessment.

Legacy sessions print each lesson's prose and uniquely linked concept before
the first new dependent practice card, and the remaining lesson text by its
last new card. Shown text is saved in `_runtime.shown_content`; no input line
is consumed, and old `--auto` box and date results are unchanged. V2 sessions
ask an optional stretch attempt after core feedback, then show its key. A
stretch attempt does not alter the card's box or next date.

Interleaving: cards may carry `[topic: xyz]`; the driver avoids asking two
same-topic cards back-to-back (falls back to lesson filename if no tag).
"""
import argparse, datetime, json, os, re, subprocess, sys, tempfile
from pathlib import Path
try:
    from . import tutorial_format
except ImportError:  # copied standalone tool bundle
    import tutorial_format

BOX_INTERVALS = {1: 1, 2: 2, 3: 7, 4: 21}  # days until next review, by box
MAX_BOX = 4
NEVER = '9999-12-31'  # pretest cards: asked once, never rescheduled

CARD_RE = re.compile(r'^###\s+CARD\s+(\S+)\s*(.*)$')
FIELD_RE = re.compile(r'^(Q|Hint|AcceptIf|RejectIf|Stretch|Source):\s*(.*)$')

def parse_cards(lessons_dir):
    """Parse every lesson-*.md into a list of card dicts, in file+source order."""
    return [card for doc in tutorial_format.parse_lessons(lessons_dir)
            for card in doc['cards']]

def eval_predicate(expr, answer, base_dir):
    """Evaluate an AcceptIf/RejectIf predicate.

    Tokens joined by AND/OR:
      - regex(/.../flags)  — Python re.search on the answer
      - check(path)        — run executable at base_dir/path, answer on stdin;
                             exit 0 counts as True (a structural test)
      - bare text          — substring match (fallback)
    """
    tokens = re.split(r'\s+(AND|OR)\s+', expr)
    result, op = None, 'AND'
    for tok in tokens:
        if tok in ('AND', 'OR'):
            op = tok
            continue
        tok = tok.strip()
        rm = re.match(r'regex\((/(.*)/([a-z]*))\)', tok)
        cm = re.match(r'check\(([^)]+)\)', tok)
        if rm:
            flags = (re.I if 'i' in rm.group(3) else 0) | (re.S if 's' in rm.group(3) else 0)
            val = bool(re.search(rm.group(2), answer, flags))
        elif cm:
            path = os.path.join(base_dir, cm.group(1))
            try:
                r = subprocess.run([path], input=answer, text=True,
                                   capture_output=True, timeout=30)
                val = (r.returncode == 0)
            except (OSError, subprocess.TimeoutExpired):
                val = False
        else:
            val = tok.lower() in answer.lower()
        result = val if result is None else (result and val if op == 'AND' else result or val)
    return bool(result)

def due_order(cards, progress, today):
    """Due cards: pretests first (unseen only), then lowest box first,
    interleaved so no two same-topic cards run back-to-back when avoidable."""
    due = []
    for c in cards:
        st = progress.get(c['id'])
        if st is None or st['next'] <= today:
            box = st['box'] if st else c['box']
            pri = 0 if (c['once'] and st is None) else 1
            due.append(((pri, box), c))
    due.sort(key=lambda bc: bc[0])
    out, pool = [], [c for _, c in due]
    while pool:
        pick = next((c for c in pool if not out or c['topic'] != out[-1]['topic']), pool[0])
        pool.remove(pick); out.append(pick)
    return out

def next_date(today, box):
    d = datetime.date.fromisoformat(today) + datetime.timedelta(days=BOX_INTERVALS[box])
    return d.isoformat()

class SessionStopped(Exception):
    pass


def load_progress(path):
    if not os.path.exists(path):
        return {}
    try:
        with open(path, encoding='utf-8') as f:
            state = json.load(f)
    except (OSError, ValueError) as exc:
        raise ValueError(f'{path}: malformed progress: {exc}') from exc
    if not isinstance(state, dict):
        raise ValueError(f'{path}: progress must be a JSON object')
    for cid, record in state.items():
        if not isinstance(record, dict) or not isinstance(record.get('box'), int) or not isinstance(record.get('next'), str):
            raise ValueError(f'{path}: malformed progress record {cid}')
    return state


def save_progress(path, state):
    """Same-directory replacement leaves the old file intact on write failure."""
    parent = os.path.dirname(os.path.abspath(path))
    fd, temporary = tempfile.mkstemp(prefix='.progress-', dir=parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(state, f, indent=2, sort_keys=True)
            f.write('\n')
            f.flush()
            os.fsync(f.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _archive_progress(path, today):
    """Archive exact old bytes exclusively before replacing progress."""
    source = Path(path)
    stem = source.name[:-5] if source.name.endswith('.json') else source.name
    base = f'{stem}.{today}.bak'
    number = 0
    while True:
        suffix = '' if number == 0 else f'.{number}'
        archive = source.with_name(f'{base}{suffix}.json')
        try:
            os.link(source, archive)
            return archive
        except FileExistsError:
            number += 1
        except OSError:
            try:
                with open(source, 'rb') as old, open(archive, 'xb') as backup:
                    backup.write(old.read())
                return archive
            except FileExistsError:
                number += 1
            except BaseException:
                archive.unlink(missing_ok=True)
                raise


def _selected_lesson(docs, requested):
    """Accept a lesson basename or its path relative to lessons/ or its parent."""
    valid = [doc['relative'] for doc in docs]
    normalized = requested.replace('\\', '/').removeprefix('./')
    exact = [doc for doc in docs if normalized in (doc['relative'],
                                                   'lessons/' + doc['relative'])]
    matches = exact or [doc for doc in docs if Path(doc['relative']).name == normalized]
    if len(matches) != 1:
        reason = 'ambiguous' if matches else 'unknown'
        raise ValueError(f'{reason} lesson {requested!r}; valid lessons: '
                         + (', '.join(valid) if valid else '(none)'))
    return matches[0]


def _reset_lesson_state(progress, doc, today, version):
    lesson = doc['relative']
    cards = {card['id'] for card in doc['cards']}
    for cid in cards:
        progress.pop(cid, None)
    runtime = progress.get('_runtime')
    if not runtime:
        if version == 2:
            progress['_runtime'] = {'box': 0, 'next': NEVER, 'version': 2,
                                    'reset_events': [{'lesson': lesson, 'date': today}]}
        return
    if runtime.get('pending_self_grade', {}).get('card') in cards:
        runtime.pop('pending_self_grade')
    if version != 2:
        runtime['shown_content'] = [item for item in runtime.get('shown_content', [])
                                    if not item.startswith(lesson + ':')]
        return

    prefix = lesson + ':'
    def is_action(action):
        kind, sep, ident = action.partition(':')
        return bool(sep) and (kind == 'card' and ident in cards or
                              kind in ('content', 'teach', 'elaborate', 'handson', 'hands_on')
                              and ident.startswith(prefix))
    def is_card_token(token):
        return (':' in token and any(token.split(':', 1)[1].startswith(cid + ':')
                                     for cid in cards))

    for key in ('completed_steps',):
        if key in runtime:
            runtime[key] = [item for item in runtime[key] if not is_action(item)]
    for key in ('answered_steps', 'feedback_steps'):
        if key in runtime:
            runtime[key] = [item for item in runtime[key] if not is_card_token(item)]
    for key in ('attempts_by_date',):
        for entries in runtime.get(key, {}).values():
            for cid in cards:
                entries.pop(cid, None)
    for date, entries in runtime.get('retry_by_date', {}).items():
        runtime['retry_by_date'][date] = [cid for cid in entries if cid not in cards]
    for date, entries in runtime.get('introduced_by_date', {}).items():
        runtime['introduced_by_date'][date] = [identity for identity in entries
                                               if not identity.startswith(prefix)]
    for key in ('exposures',):
        for identity in list(runtime.get(key, {})):
            if identity.startswith(prefix):
                runtime[key].pop(identity)
    for key in ('card_exposures', 'assistance'):
        for cid in cards:
            runtime.get(key, {}).pop(cid, None)
    if runtime.get('pending', {}).get('card') in cards:
        runtime.pop('pending')
    if is_action(runtime.get('pending_step', {}).get('action', '')):
        runtime.pop('pending_step')
    if str(runtime.get('last_attempt_concept', '')).startswith(prefix):
        runtime.pop('last_attempt_concept')
    runtime.setdefault('reset_events', []).append({'lesson': lesson, 'date': today})


def reset_progress(lessons_dir, progress_path, today, lesson=None):
    """Return (archive, reset label), or (None, None) for missing progress."""
    if not os.path.exists(progress_path):
        return None, None
    if lesson is None:
        updated, label = {}, 'all progress'
    else:
        try:
            progress = load_progress(progress_path)
        except ValueError as exc:
            raise ValueError(f'{exc}; use --reset to archive and clear the full file') from exc
        docs = tutorial_format.parse_lessons(lessons_dir)
        selected = _selected_lesson(docs, lesson)
        updated = progress
        version = tutorial_format.format_version(Path(lessons_dir).resolve().parent)
        _reset_lesson_state(updated, selected, today, version)
        label = f'lesson {selected["relative"]}'
    archive = _archive_progress(progress_path, today)
    save_progress(progress_path, updated)
    return archive, label


def _runtime(progress):
    return progress.setdefault('_runtime', {'box': 0, 'next': NEVER, 'version': 1})


def export_attempts(progress_path, destination):
    """Export a snapshot; progress remains the sole authoritative store."""
    records = load_progress(progress_path).get('_runtime', {}).get('attempt_records', [])
    parent = os.path.dirname(os.path.abspath(destination))
    fd, temporary = tempfile.mkstemp(prefix='.attempts-', dir=parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            for record in records:
                f.write(json.dumps(record, sort_keys=True) + '\n')
            f.flush(); os.fsync(f.fileno())
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)


def _key(card):
    print('  Key:')
    for item in card['Key']:
        print(f'    - {item}')


def run(lessons_dir, progress_path, today, auto=None, *, replay=None,
        new_limit=6, max_cards=None, max_minutes=None, clock=None,
        wall_clock=None, ask_confidence=False):
    base_dir = os.path.dirname(os.path.abspath(str(lessons_dir).rstrip('/')))
    if tutorial_format.format_version(base_dir) == 2:
        if auto:
            raise ValueError('v2 uses tagged --replay; legacy --auto is ambiguous')
        try:
            from . import v2_session
        except ImportError:
            import v2_session
        return v2_session.run(lessons_dir, progress_path, today, replay=replay,
                              new_limit=new_limit, max_cards=max_cards,
                              max_minutes=max_minutes, clock=clock,
                              wall_clock=wall_clock, ask_confidence=ask_confidence)
    if replay:
        raise ValueError('--replay requires format-version: 2')
    cards = parse_cards(lessons_dir)
    if any(c['id'] == '_runtime' for c in cards):
        raise ValueError('_runtime is reserved for progress metadata')
    progress = load_progress(progress_path)
    docs = tutorial_format.parse_lessons(lessons_dir)
    by_lesson = {doc['relative']: doc for doc in docs}
    if auto:
        with open(auto, encoding='utf-8') as replay_file:
            auto_answers = iter(replay_file.read().splitlines())
    else:
        auto_answers = None

    def get_answer(prompt):
        if auto_answers is not None:
            try:
                answer = next(auto_answers)
            except StopIteration:
                raise SessionStopped('Replay exhausted; pending work was not graded.')
            print(f'{prompt}{answer}')
        else:
            try:
                answer = input(prompt)
            except (EOFError, KeyboardInterrupt):
                raise SessionStopped('Interrupted; completed work remains saved.')
        if answer.strip() == ':quit':
            raise SessionStopped('Stopped; completed work remains saved.')
        return answer

    def commit(card, grade, once=False):
        old = progress.get(card['id'], {'box': card['box']})
        record = dict(old)
        if once:
            record.update({'box': card['box'], 'next': NEVER})
        else:
            box = record['box']
            if grade == 'h':
                box = min(box + 1, MAX_BOX)
            elif grade == 'm':
                box = 1
            record.update({'box': box, 'next': next_date(today, box) if grade == 'h'
                           else (datetime.date.fromisoformat(today) + datetime.timedelta(days=1)).isoformat()})
        updated = dict(progress)
        updated[card['id']] = record
        runtime = dict(_runtime(progress))
        runtime.pop('pending_self_grade', None)
        updated['_runtime'] = runtime
        save_progress(progress_path, updated)
        progress.clear()
        progress.update(updated)
        return record

    def show_legacy_content(card):
        if card['once'] or card['id'] in progress:
            return
        doc = by_lesson[card['lesson']]
        runtime = dict(_runtime(progress))
        shown = set(runtime.get('shown_content', []))
        source = tutorial_format.resolve_legacy_source(doc, card)
        new_cards = [c for c in doc['cards'] if not c['once'] and c['id'] not in progress]
        show_all = source is None or len(new_cards) == 1
        for block in doc['blocks']:
            if block['kind'] == 'card':
                continue
            token = f"{doc['relative']}:{block['kind']}:{block['line']}"
            if token in shown:
                continue
            if block['kind'] == 'concept' and not (show_all or block['identity'] == source):
                continue
            if block['raw'].strip():
                print(block['raw'].strip() + '\n')
            shown.add(token)
        runtime['shown_content'] = sorted(shown)
        updated = dict(progress)
        updated['_runtime'] = runtime
        save_progress(progress_path, updated)
        progress.clear(); progress.update(updated)

    try:
        pending = progress.get('_runtime', {}).get('pending_self_grade')
        if pending:
            card = next((c for c in cards if c['id'] == pending.get('card')), None)
            if card is None:
                raise ValueError(f"pending card {pending.get('card')} is absent from lessons")
            print(f"Resuming [{card['id']}] after answer reveal; compare your earlier answer with the key.")
            _key(card)
            while True:
                grade = get_answer('  Self-grade [h]it / [p]artial / [m]iss: ').strip().lower()
                if grade in ('h', 'p', 'm'):
                    break
                print('  Enter h, p or m (or :quit).')
            record = commit(card, grade)
            print(f"  next review: {record['next']} (box {record['box']})\n")

        order = due_order(cards, progress, today)
        if not order:
            print('Nothing due today. Come back on the next due date.')
            return progress
        print(f'# Session {today} — {len(order)} card(s) due\n')
        hits = graded = 0
        for card in order:
            show_legacy_content(card)
            print(f"[{card['id']}] {card['Q']}")
            answer = get_answer('Your answer (from memory): ')
            if card['once']:
                record = commit(card, None, once=True)
                print('  (pretest — your guess is the point, right or wrong)')
                _key(card)
                print()
                continue
            graded += 1
            if card['AcceptIf']:
                rejected = card['RejectIf'] and eval_predicate(card['RejectIf'], answer, base_dir)
                ok = (not rejected) and eval_predicate(card['AcceptIf'], answer, base_dir)
                grade = 'h' if ok else 'm'
                record = commit(card, grade)
                hits += int(ok)
                why = ' (contradiction guard)' if rejected else ''
                print(f"  -> {'HIT' if ok else 'MISS'} (auto-graded){why}")
                _key(card)
            else:
                updated = dict(progress)
                runtime = dict(_runtime(progress))
                runtime['pending_self_grade'] = {'card': card['id'], 'answer': answer,
                                                 'answer_exposed': True}
                updated['_runtime'] = runtime
                save_progress(progress_path, updated)
                progress.clear()
                progress.update(updated)
                _key(card)
                while True:
                    grade = get_answer('  Self-grade [h]it / [p]artial / [m]iss: ').strip().lower()
                    if grade in ('h', 'p', 'm'):
                        break
                    print('  Enter h, p or m (or :quit).')
                record = commit(card, grade)
                hits += int(grade == 'h')
            print(f"  next review: {record['next']} (box {record['box']})\n")
        real_next = [s['next'] for k, s in progress.items() if k != '_runtime' and s['next'] != NEVER]
        if graded:
            print(f'Session done: {hits}/{graded} hits.', end=' ')
        if real_next:
            print(f'Next due: {min(real_next)}')
        return progress
    except SessionStopped as exc:
        print(f'\n{exc}')
        return progress

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('lessons_dir')
    ap.add_argument('--progress', default='progress.json')
    ap.add_argument('--today', help='YYYY-MM-DD (explicit for determinism)')
    ap.add_argument('--auto', help='file of answers, one per line (for eval/CI)')
    ap.add_argument('--replay', help='v2 tagged replay: answer, grade, stretch, continue, elaborate or hands_on + TAB + value')
    ap.add_argument('--new-limit', type=int, default=6, help='max new concepts per date (default 6)')
    ap.add_argument('--max-cards', type=int, help='max answer prompts this run, including pretests and elaborations')
    ap.add_argument('--max-minutes', type=float, help='time budget in minutes; an in-progress answer may overrun')
    ap.add_argument('--print-t0', action='store_true', help='print complete paper study guide without changing progress')
    ap.add_argument('--browse', action='store_true', help='browse all authored content and separated keys')
    ap.add_argument('--export-attempts', metavar='JSONL', help='export committed v2 attempt records')
    ap.add_argument('--ask-confidence', action='store_true', help='ask optional numeric response confidence before feedback (v2)')
    ap.add_argument('--assessment', action='store_true', help='run the separate fixed assessment form (v2)')
    resets = ap.add_mutually_exclusive_group()
    resets.add_argument('--reset', action='store_true', help='archive progress and restart the whole tutorial')
    resets.add_argument('--reset-lesson', metavar='LESSON', help='archive progress and restart one lesson')
    a = ap.parse_args()
    try:
        if a.reset or a.reset_lesson:
            if not a.today:
                ap.error('--today is required for a reset')
            datetime.date.fromisoformat(a.today)
            archive, label = reset_progress(a.lessons_dir, a.progress, a.today,
                                            lesson=a.reset_lesson)
            if archive is None:
                print(f'No progress file at {a.progress}; nothing archived or reset.')
            else:
                print(f'Archived {a.progress} to {archive}; reset {label}.')
        elif a.export_attempts:
            export_attempts(a.progress, a.export_attempts)
        elif a.print_t0 or a.browse:
            docs = tutorial_format.parse_lessons(a.lessons_dir)
            if tutorial_format.format_version(Path(a.lessons_dir).resolve().parent) == 1:
                for doc in docs:
                    print(doc['text'])
            else:
                try:
                    from . import v2_session
                except ImportError:
                    import v2_session
                print(v2_session.paper(docs), end='')
        else:
            if not a.today:
                ap.error('--today is required for a session')
            if a.new_limit < 0 or (a.max_cards is not None and a.max_cards < 0) or (a.max_minutes is not None and a.max_minutes < 0):
                ap.error('budgets must be nonnegative')
            if a.assessment:
                try:
                    from . import v2_assessment
                except ImportError:
                    import v2_assessment
                v2_assessment.run(a.lessons_dir, a.progress, a.today, replay=a.replay,
                                  ask_confidence=a.ask_confidence)
            else:
                run(a.lessons_dir, a.progress, a.today, a.auto, replay=a.replay,
                    new_limit=a.new_limit, max_cards=a.max_cards, max_minutes=a.max_minutes,
                    ask_confidence=a.ask_confidence)
    except (ValueError, OSError) as exc:
        print(f'drive.py: error: {exc}', file=sys.stderr)
        sys.exit(2)
