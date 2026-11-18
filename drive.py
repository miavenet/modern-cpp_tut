#!/usr/bin/env python3
"""T1 deterministic tutorial driver — NO model, NO network.

Reads the same lesson cards a model-driven session (T2/T3) would read, and runs
the pedagogy mechanically: pretest-first, spaced (Leitner) scheduling, one
question at a time, attempt-before-reveal, box updates, resumable progress. This
is the proof that a tutorial's pedagogy lives in the ARTIFACT, not the model.

Determinism contract: same progress.json + same --today => same card order and
same resulting card boxes/dates for the same answers. The only nondeterminism
is the human's answers.

Usage:
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD
    ./drive.py lessons/ --progress progress.json --today YYYY-MM-DD --auto answers.txt   # for eval/CI

Card grading: constrained executable checks are automatic. Open-ended answers,
including legacy regex and keyword checks, show the Key and require honest
self-grading. A check/tool failure never changes a learner grade.

Pretest cards (`[type: pretest]`/`[once]` tag): asked FIRST, before everything
else, exactly once ever. The learner commits a guess, the Key is revealed, and
the card is never rescheduled — the wrong guess + reveal IS the intervention
(prediction error), not an assessment.

Acquisition is stable: unseen pretests first, then authored card order. Reviews
are capped separately so they cannot starve new cards.
"""
import argparse, datetime, json, os, re, subprocess, sys, tempfile, time
from pathlib import Path
from driver_lib import tutorial_format

BOX_INTERVALS = {1: 1, 2: 2, 3: 7, 4: 21}  # days until next review, by box
MAX_BOX = 4
NEVER = '9999-12-31'  # pretest cards: asked once, never rescheduled
GRADE_POLICY = 3
# Cards formerly graded by keyword or permissive completion checks.
LEGACY_AUTO_IDS = frozenset(' '.join((
    'M1-C1 M1-C2 M1-C3 M1-C4 M1-C5 M1-C6 M1-G1 M1-G10 M1-G11 M1-G12 M1-G13 M1-G2 M1-G3 M1-G4',
    'M1-G5 M1-G6 M1-G7 M1-G8 M1-G9 M1-Q1 M1-Q10 M1-Q3 M1-Q5 M1-Q7 M1-Q9 M2-C1 M2-C2 M2-C3 M2-C4',
    'M2-G1 M2-G2 M2-G3 M2-G4 M2-G5 M2-G6 M2-G7 M2-G9 M2-Q1 M2-Q2 M2-Q3 M2-Q4 M2-Q5 M2-Q6 M2-Q7',
    'M3-C1 M3-C2 M3-C3 M3-C4 M3-C5 M3-G1 M3-G2 M3-G3 M3-G4 M3-G5 M3-G6 M3-G7 M3-G8 M3-G9 M3-Q1',
    'M3-Q2 M3-Q3 M3-Q4 M3-Q5 M3-Q6 M3-Q7 M3-Q8 M4-C1 M4-C2 M4-C3 M4-C4 M4-C5 M4-C6 M4-G1 M4-G2',
    'M4-G3 M4-G4 M4-G5 M4-G6 M4-G7 M4-G8 M4-G9 M4-INT1 M4-Q1 M4-Q10 M4-Q2 M4-Q3 M4-Q4 M4-Q5',
    'M4-Q6 M4-Q7 M4-Q8 M4-Q9 M5-C1 M5-C2 M5-C3 M5-C4 M5-C5 M5-C6 M5-G1 M5-G10 M5-G11 M5-G12',
    'M5-G13 M5-G14 M5-G2 M5-G3 M5-G4 M5-G5 M5-G6 M5-G7 M5-G8 M5-G9 M5-Q1 M5-Q10 M5-Q11 M5-Q12',
    'M5-Q2 M5-Q3 M5-Q5 M5-Q6 M5-Q9 M6-C1 M6-C2 M6-C3 M6-C4 M6-G1 M6-G2 M6-G3 M6-G4 M6-G5 M6-G6',
    'M6-G7 M6-G8 M6-G9 M6-Q1 M6-Q2 M6-Q3 M6-Q4 M6-Q5 M6-Q6 M6-Q7 M6-Q8 M7-C1 M7-C2 M7-C3 M7-C4',
    'M7-G1 M7-G2 M7-G3 M7-G4 M7-G5 M7-G6 M7-G7 M7-G8 M7-Q1 M7-Q2 M7-Q3 M7-Q4 M7-Q5 M7-Q6 M7-Q7',
    'M7-Q8',
)).split())
# These checks ask for open-ended explanations. Keyword scripts cannot establish
# semantic correctness; reveal the published key for honest self-grading.
SELF_GRADE_CHECKS = {'M1-C4', 'M5-C4'}

def needs_self_grade(card):
    expr = card.get('AcceptIf') or ''
    if not expr:
        return False
    if card['id'] in SELF_GRADE_CHECKS:
        return True
    if expr.startswith('check('):
        return False
    match = re.fullmatch(r'regex\(/(.*)/[a-z]*\)', expr)
    if match:
        pattern = match.group(1)
        if ((pattern.startswith('^') and pattern.endswith('$')) or
                (pattern.startswith(r'\A') and pattern.endswith(r'\Z'))):
            return False
    return True

class CheckProblem(Exception):
    """A checker or local tool failed; this is not a learner miss."""

CARD_RE = re.compile(r'^###\s+(?:CARD|Card)\s+(\S+)\s*(.*)$')
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
    result, op, feedback = None, 'AND', ''
    for tok in tokens:
        if tok in ('AND', 'OR'):
            op = tok
            continue
        tok = tok.strip()
        rm = re.match(r'regex\((/(.*)/([a-z]*))\)', tok)
        cm = re.match(r'check\(([^)]+)\)', tok)
        if rm:
            flags = re.I if 'i' in rm.group(3) else 0
            val = bool(re.search(rm.group(2), answer, flags))
        elif cm:
            path = os.path.join(base_dir, cm.group(1))
            try:
                r = subprocess.run([path], input=answer, text=True,
                                   capture_output=True, timeout=30)
                feedback = r.stdout.strip() or r.stderr.strip()
                if r.returncode not in (0, 1):
                    raise CheckProblem(feedback or f'Check exited {r.returncode}; run ./setup.sh.')
                val = (r.returncode == 0)
            except (OSError, subprocess.TimeoutExpired) as exc:
                raise CheckProblem(f'Check could not run ({type(exc).__name__}); run ./setup.sh.') from exc
        else:
            val = tok.lower() in answer.lower()
        result = val if result is None else (result and val if op == 'AND' else result or val)
    return bool(result), feedback

def due_order(cards, progress, today):
    """Due cards: preserve priority, then alternate review topics when possible."""
    due = []
    for c in cards:
        st = progress.get(c['id'])
        if st is None or st['next'] <= today:
            box = st['box'] if st else c['box']
            pri = 0 if (c['once'] and st is None) else 1
            due.append(((pri, box), c))
    due.sort(key=lambda bc: bc[0])
    pretests = [card for (priority, _), card in due if priority == 0]
    unseen = [card for (_, _), card in due
              if card['id'] not in progress and not card['once']]
    reviews = [item for item in due if item[1]['id'] in progress]
    ordered = []
    while reviews:
        priority = reviews[0][0]
        group = [item for item in reviews if item[0] == priority]
        reviews = [item for item in reviews if item[0] != priority]
        previous = ordered[-1]['topic'] if ordered else None
        while group:
            index = next((i for i, (_, card) in enumerate(group)
                          if card['topic'] != previous), 0)
            _, chosen = group.pop(index)
            ordered.append(chosen)
            previous = chosen['topic']
    return pretests + ordered + unseen

def next_date(today, box):
    d = datetime.date.fromisoformat(today) + datetime.timedelta(days=BOX_INTERVALS[box])
    return d.isoformat()

class SessionStopped(Exception):
    """The learner stopped before completing the current prompt."""

def load_progress(path):
    if not os.path.exists(path):
        return {}
    try:
        with open(path, encoding='utf-8') as f:
            progress = json.load(f)
    except (OSError, ValueError) as exc:
        raise ValueError(f'{path}: malformed progress: {exc}') from exc
    if not isinstance(progress, dict):
        raise ValueError(f'{path}: progress must be a JSON object')
    for cid, state in progress.items():
        if (not isinstance(state, dict) or type(state.get('box')) is not int or
                not isinstance(state.get('next'), str) or
                state['box'] not in ((0,) if cid == '_runtime' else tuple(BOX_INTERVALS))):
            raise ValueError(f'{path}: malformed progress record {cid}')
        if state['next'] != NEVER:
            try:
                datetime.date.fromisoformat(state['next'])
            except ValueError as exc:
                raise ValueError(f'{path}: malformed next date for {cid}') from exc
    return progress

def save_progress(path, progress):
    """Write a complete file beside the old one, then atomically replace it."""
    parent = os.path.dirname(os.path.abspath(path))
    fd, temporary = tempfile.mkstemp(prefix='.progress-', dir=parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(progress, f, indent=2, sort_keys=True)
            f.write('\n')
            f.flush()
            os.fsync(f.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def archive_progress(path, today):
    """Keep the exact original bytes, selecting an unused archive name."""
    source = Path(path)
    stem = source.name[:-5] if source.name.endswith('.json') else source.name
    for number in range(1000000):
        suffix = '' if number == 0 else f'.{number}'
        archive = source.with_name(f'{stem}.{today}.bak{suffix}.json')
        try:
            os.link(source, archive)
            return archive
        except FileExistsError:
            continue
        except OSError:
            try:
                with open(source, 'rb') as old, open(archive, 'xb') as backup:
                    backup.write(old.read())
                return archive
            except FileExistsError:
                continue
            except BaseException:
                archive.unlink(missing_ok=True)
                raise
    raise ValueError('too many progress archives for this date')

def selected_lesson(docs, requested):
    valid = [doc['relative'] for doc in docs]
    name = requested.replace('\\', '/').removeprefix('./')
    matches = [doc for doc in docs if name in (doc['relative'], 'lessons/' + doc['relative'])]
    if not matches:
        matches = [doc for doc in docs if Path(doc['relative']).name == name]
    if len(matches) != 1:
        reason = 'ambiguous' if matches else 'unknown'
        raise ValueError(f'{reason} lesson {requested!r}; valid lessons: ' +
                         (', '.join(valid) if valid else '(none)'))
    return matches[0]

def reset_progress(lessons_dir, progress_path, today, lesson=None):
    """Archive first; then clear all state or just one lesson's state."""
    doc = selected_lesson(tutorial_format.parse_lessons(lessons_dir), lesson) if lesson else None
    if not os.path.exists(progress_path):
        return None, None
    if doc is None:
        updated, label = {}, 'all progress'
    else:
        try:
            updated = load_progress(progress_path)
        except ValueError as exc:
            raise ValueError(f'{exc}; use --reset to archive and clear the full file') from exc
        ids = {card['id'] for card in doc['cards']}
        for cid in ids:
            updated.pop(cid, None)
        runtime = updated.get('_runtime')
        if runtime:
            runtime['shown_content'] = [name for name in runtime.get('shown_content', [])
                                        if name != doc['relative']]
            prefix = doc['relative'] + ':'
            for field in ('shown_blocks', 'shown_concepts'):
                runtime[field] = [
                    token for token in runtime.get(field, [])
                    if not token.startswith(prefix)
                ]
            if runtime.get('assistance', {}).get('card') in ids:
                runtime.pop('assistance')
            for field in ('pending_self_grade', 'pending_answer',
                          'pending_retry', 'pending_stretch'):
                if runtime.get(field, {}).get('card') in ids:
                    runtime.pop(field)
        label = f'lesson {doc["relative"]}'
    archive = archive_progress(progress_path, today)
    save_progress(progress_path, updated)
    return archive, label

def run(lessons_dir, progress_path, today, auto=None, max_cards=12,
        max_minutes=20, review_budget=4, new_budget=6, predict_worked=True,
        offer_stretch=True):
    """Run one interruptible sitting. Reviews are capped so new cards advance."""
    docs = tutorial_format.parse_lessons(lessons_dir)
    cards = [c for d in docs for c in d['cards']]
    by_id = {c['id']: (d, c) for d in docs for c in d['cards']}
    if '_runtime' in by_id:
        raise ValueError('_runtime is reserved for progress metadata')
    base_dir = str(Path(lessons_dir).resolve().parent)
    progress = load_progress(progress_path)
    runtime = dict(progress.get('_runtime', {'box': 0, 'next': NEVER, 'version': 1}))
    saved = runtime.get('last_date')
    if saved and today < saved:
        raise ValueError(f"Saved progress is dated {saved}. Use --today {saved} or later, or --reset to start over.")
    answers = Path(auto).read_text(encoding='utf-8').splitlines() if auto else None
    index = 0
    started = time.monotonic()
    used = 0
    new_used = 0
    shown = set(runtime.get('shown_content', []))
    taught = set(runtime.get('shown_concepts', []))
    shown_blocks = set(runtime.get('shown_blocks', []))
    offered_reviews = set()
    assisted = False
    # A prior keyword HIT is not evidence of semantic mastery. Preserve the
    # record and card ID, but ask affected cards again under the new policy.
    if runtime.get('grade_policy', 1) < GRADE_POLICY:
        for card in cards:
            if card['id'] in progress and not card['once'] and card['id'] in LEGACY_AUTO_IDS:
                progress[card['id']]['box'] = 1
                progress[card['id']]['next'] = today
        runtime['grade_policy'] = GRADE_POLICY

    def persist():
        runtime['last_date'] = today
        runtime['shown_content'] = sorted(shown)
        runtime['shown_concepts'] = sorted(taught)
        runtime['shown_blocks'] = sorted(shown_blocks)
        progress['_runtime'] = runtime
        save_progress(progress_path, progress)

    def remaining():
        return max(0, int(max_minutes - (time.monotonic() - started) / 60 + .999))

    def waiting():
        return sum(1 for c in cards if c['id'] in progress and not c['once']
                   and progress[c['id']]['next'] <= today)

    def status(card=None):
        if card:
            doc, _ = by_id[card['id']]
            pos = doc['cards'].index(card) + 1
            total = len(doc['cards'])
            title = doc['text'].splitlines()[0].lstrip('# ').strip()
            print(f"[{pos}/{total} · about {remaining()} min left · {waiting()} reviews waiting] {title}")
        else:
            unseen = next((d for d in docs if any(c['id'] not in progress for c in d['cards'])), None)
            name = unseen['text'].splitlines()[0].lstrip('# ').strip() if unseen else 'all lessons introduced'
            print(f"{name}; about {remaining()} min left; {waiting()} reviews waiting.")

    def ask(prompt, card=None, hint=False, blank_pause=False, prediction=False):
        nonlocal index, assisted
        while True:
            if answers is not None:
                if index >= len(answers):
                    raise SessionStopped('Scripted answers exhausted; completed work remains saved.')
                value = answers[index]
                index += 1
                print(prompt + value)
            else:
                try:
                    value = input(prompt)
                except (EOFError, KeyboardInterrupt):
                    raise SessionStopped('Stopped; completed work remains saved.')
            raw = value.strip()
            command = raw.lower()
            if command in (':quit', '/stop', '/quit'):
                raise SessionStopped('Stopped; completed work remains saved.')
            if command in (':help', '/help'):
                print('Commands: :help, :status, :skip, :hint (when offered), :lesson, :quit; /stop also quits.')
                continue
            if command in (':status', '/status'):
                status(card)
                continue
            if command in (':hint', '/hint') and hint and card:
                print('Hint: ' + str(card.get('Hint') or '(no hint available)'))
                assisted = True
                runtime['assistance'] = {'card': card['id']}
                persist()
                continue
            if command in (':lesson', '/lesson') and card:
                if prediction:
                    print('Make a prediction before revealing the worked result.')
                    continue
                doc, _ = by_id[card['id']]
                teach(doc, card, force=True)
                assisted = True
                runtime['assistance'] = {'card': card['id']}
                persist()
                continue
            if command in (':skip', '/skip'):
                return ':skip'
            if raw.startswith((':', '/')):
                print('unknown command, type :help')
                continue
            if not raw and blank_pause:
                raise SessionStopped('Paused; completed work remains saved.')
            return value

    def choice(prompt, allowed, card):
        while True:
            value = ask(prompt, card).strip().lower()
            if value == ':skip':
                return 'm'
            if value in allowed:
                return allowed[value]
            print('Please type ' + ', '.join(sorted(allowed)) + '.')

    def key(card, label='Key'):
        print('  ' + label + ':')
        for item in card.get('Key', []):
            print('    - ' + item)

    def stretch(card):
        if not card.get('Stretch'):
            return
        runtime['pending_stretch'] = {'card': card['id']}
        persist()
        print('  Optional ungraded extension; excluded from recorded progress.')
        print('  ' + card['Stretch'])
        ask('  Your answer (or :skip): ', card)
        print('  Extension key:')
        for item in card.get('StretchKey', []):
            print('    - ' + item)
        runtime.pop('pending_stretch', None)
        persist()

    def workspace_prompt(card):
        text = card['Q'] + '\n' + str(card.get('HandsOn') or '')
        match = re.search(r'(?<![\w/])(?:modules/[\w-]+/)?(?:labs?|work)/[\w./-]+', text)
        default = match.group(0).rstrip('.,;:)') if match else None
        if default:
            return (f'Do the lab now. When your edit is ready, press Enter to run '
                    f'the check on {default}, or type another workspace path: '), default
        return 'Do the lab now. When your edit is ready, type your workspace path: ', None

    def teach(doc, card, force=False):
        name = doc['relative']
        tokens = []
        for i, block in enumerate(doc['blocks']):
            if block['kind'] == 'card' and block['id'] == card['id']:
                break
            if block['kind'] == 'card':
                continue
            if block['kind'] == 'concept' and card['once']:
                continue
            token = name + ':' + str(i)
            if token in shown_blocks and not force:
                continue
            if block['kind'] == 'prose':
                visible = re.sub(r'<!--.*?-->', '', block['raw'], flags=re.S)
                visible = '\n'.join(line for line in visible.splitlines() if line.strip() != '---')
                if visible.strip():
                    print('\n' + visible.strip() + '\n')
                shown_blocks.add(token)
                continue
            tokens.append(token)
            parts = re.split(r'(<details(?:\s[^>]*)?>.*?</details>)', block['raw'], flags=re.S)
            for part in parts:
                if part.startswith('<details'):
                    summary = re.search(r'<summary>(.*?)</summary>(.*?)</details>', part, re.S)
                    if not summary:
                        continue
                    if predict_worked and not force and 'output' in summary.group(1).lower():
                        answer = ask('  Predict the result before revealing it: ',
                                     card, prediction=True)
                        if not answer.strip():
                            raise SessionStopped('Paused before the worked result; resume to predict it.')
                    print('\n' + summary.group(1).strip() + ':\n' + summary.group(2).strip() + '\n')
                else:
                    visible = re.sub(r'(?m)^(?:Elaborate|HandsOn|ElaborateKey|HandsOnKey):.*$', '', part)
                    if visible.strip():
                        print(visible.strip() + '\n')
            shown_blocks.add(token)
        return tokens

    def commit(card, grade=None, sure=False, concepts=()):
        old = dict(progress.get(card['id'], {'box': card['box']}))
        if card['once']:
            old['next'] = NEVER
        else:
            if grade == 'm' and sure:
                old['sure_miss'] = True
            if grade == 'h':
                old['box'] = min(old['box'] + 1, MAX_BOX)
                old.pop('sure_miss', None)
            elif grade == 'm':
                old['box'] = 1
            old['next'] = next_date(today, old['box']) if grade == 'h' else (
                datetime.date.fromisoformat(today) + datetime.timedelta(days=1)).isoformat()
        progress[card['id']] = old
        old.setdefault('first_attempt', today)
        old['last_review'] = today
        shown.add(by_id[card['id']][0]['relative'])
        taught.update(concepts)
        runtime.pop('pending_self_grade', None)
        runtime.pop('pending_answer', None)
        if runtime.get('assistance', {}).get('card') == card['id']:
            runtime.pop('assistance')
        persist()
        return old

    def retry(card, is_lab=False, default_workspace=None):
        while True:
            action = ask('  retry (edit and check again), :hint, or giveup: ',
                         card, hint=bool(card.get('Hint'))).strip().lower()
            if action == 'retry':
                if is_lab:
                    attempt = ask('  Edit the lab, then press Enter to retry the same workspace or type another path: ',
                                  card, hint=bool(card.get('Hint')))
                    attempt = attempt or default_workspace or ''
                else:
                    attempt = ask('  New answer: ', card, hint=bool(card.get('Hint')))
                if attempt == ':skip':
                    action = 'giveup'
                else:
                    ok, feedback = eval_predicate(card['AcceptIf'], attempt, base_dir)
                    if card.get('RejectIf') and eval_predicate(card['RejectIf'], attempt, base_dir)[0]:
                        ok = False
                    if ok:
                        print('  -> HIT on retry. Scheduling uses your first attempt.')
                        return
                    print('  -> MISS: ' + (feedback.splitlines()[-1] if feedback else 'review the criterion'))
                    continue
            if action in ('giveup', ':skip'):
                return
            print('Type retry or giveup.')

    def summary():
        count = waiting()
        future = [s['next'] for cid, s in progress.items()
                  if cid != '_runtime' and s['next'] != NEVER and s['next'] > today]
        print(f"{count} reviews are waiting.")
        if any(c['id'] not in progress for c in cards):
            print('New cards are ready when you resume.')
        elif count:
            print('Next: run the same command today to review them.')
        elif future:
            print(f"Next: run the same command on or after {min(future)}.")

    print('Commands: :help, :status, :skip, :hint (when offered), :lesson, :quit; /stop also quits.')
    print('Answers are one line. For code, use semicolons where valid; do not paste multiline code. For blanks, use commas in order.')
    try:
        pending = runtime.get('pending_self_grade')
        if pending:
            doc, card = by_id[pending['card']]
            status(card)
            print(f"Resuming [{card['id']}]. Your earlier answer: {pending['answer']}")
            key(card)
            grade = choice('  Self-grade [h]it / [p]artial / [m]iss: ',
                           {'h': 'h', 'hit': 'h', 'p': 'p', 'partial': 'p',
                            'm': 'm', 'miss': 'm'}, card)
            if grade == 'h' and pending.get('assisted', False):
                grade = 'p'
            commit(card, grade, bool(pending.get('sure')), pending.get('concepts', []))
        pending_retry = runtime.get('pending_retry')
        if pending_retry:
            _, card = by_id[pending_retry['card']]
            status(card)
            print(f"Resuming [{card['id']}] after the saved first-attempt MISS.")
            retry(card, pending_retry.get('is_lab', False),
                  pending_retry.get('default_workspace'))
            key(card)
            runtime.pop('pending_retry', None)
            persist()
        pending_stretch = runtime.get('pending_stretch')
        if pending_stretch:
            _, card = by_id[pending_stretch['card']]
            print(f"Resuming optional extension for [{card['id']}].")
            stretch(card)
        review = [c for c in due_order(cards, progress, today)
                  if c['id'] in progress and not c['once']]
        review = review[:review_budget]
        # Ask pretests at the start of their own lesson, then its other cards.
        new = [
            c for d in docs for c in ([_c for _c in d['cards'] if _c['once']] +
                                     [_c for _c in d['cards'] if not _c['once']])
            if c['id'] not in progress]
        # Existing reviews first, then authored lesson order. A review appears once a day.
        pending_answer = runtime.get('pending_answer')
        pending_card = by_id[pending_answer['card']][1] if pending_answer else None
        queue = ([pending_card] if pending_card else []) + [
            card for card in review + new if card is not pending_card]
        for card in queue:
            if used >= max_cards or remaining() <= 0:
                print('Sitting limit reached; your place is saved.')
                break
            if card['id'] not in progress and not card['once'] and new_used >= new_budget:
                print('New-card limit reached; your place is saved.')
                break
            if card['id'] in offered_reviews:
                continue
            if card['id'] in progress:
                offered_reviews.add(card['id'])
            elif not card['once']:
                new_used += 1
            doc, _ = by_id[card['id']]
            concept_tokens = teach(doc, card)
            assisted = runtime.get('assistance', {}).get('card') == card['id']
            if pending_answer and card is pending_card:
                concept_tokens = pending_answer.get('concepts', concept_tokens)
                assisted = assisted or pending_answer.get('assisted', False)
            status(card)
            print(f"[{card['id']}] {card['Q']}")
            if card.get('Hint'):
                print('  Type :hint for a clue.')
            is_lab = (card.get('AcceptIf') or '').startswith('check(') and (
                card.get('type') == 'lab' or 'do the lab' in card['Q'].lower())
            default_workspace = None
            if pending_answer and card is pending_card:
                attempt = pending_answer['answer']
                print(f"Your earlier answer: {attempt}")
            elif is_lab:
                prompt, default_workspace = workspace_prompt(card)
                attempt = ask(prompt, card, hint=bool(card.get('Hint')))
                if not attempt.strip() and default_workspace:
                    attempt = default_workspace
                while not attempt.strip():
                    attempt = ask('Workspace path: ', card, hint=bool(card.get('Hint')))
            else:
                attempt = ask('Your answer (from memory): ', card, hint=bool(card.get('Hint')))
                if not attempt.strip():
                    raise SessionStopped('Paused; completed work remains saved.')
            used += 1
            if card['once']:
                commit(card, concepts=concept_tokens)
                key(card, 'Reveal')
                continue
            if not (pending_answer and card is pending_card):
                runtime['pending_answer'] = {'card': card['id'], 'answer': attempt,
                                             'assisted': assisted,
                                             'concepts': concept_tokens}
                persist()
            sure = choice('  Confidence [s]ure / [u]nsure: ',
                          {'s': 's', 'sure': 's', 'u': 'u', 'unsure': 'u'}, card) == 's'
            if attempt == ':skip':
                grade = 'm'
                print('  -> MISS (you chose “don’t know”).')
                key(card)
            elif card.get('AcceptIf') and not needs_self_grade(card):
                ok, feedback = eval_predicate(card['AcceptIf'], attempt, base_dir)
                if card.get('RejectIf') and eval_predicate(card['RejectIf'], attempt, base_dir)[0]:
                    ok = False
                grade = 'p' if ok and assisted else ('h' if ok else 'm')
                print(f"  -> {'HIT' if grade == 'h' else 'PARTIAL' if grade == 'p' else 'MISS'}")
                if grade == 'p':
                    print('  Assisted success: this card returns tomorrow for unaided recall.')
                if grade == 'm' and feedback:
                    for line in feedback.splitlines():
                        if not line.startswith('{') and line not in ('HIT', 'MISS', 'SETUP PROBLEM'):
                            print('  Why: ' + line)
                if grade == 'm':
                    commit(card, 'm', sure, concept_tokens)
                    runtime['pending_retry'] = {'card': card['id'], 'is_lab': is_lab,
                                                'default_workspace': default_workspace,
                                                'first_attempt': today}
                    persist()
                    retry(card, is_lab, default_workspace)
                    runtime.pop('pending_retry', None)
                    persist()
                key(card)
            else:
                runtime['pending_self_grade'] = {'card': card['id'], 'answer': attempt,
                                                 'sure': sure, 'concepts': concept_tokens,
                                                 'assisted': assisted,
                                                 'answer_exposed': True}
                persist()
                key(card)
                grade = choice('  Self-grade [h]it / [p]artial / [m]iss: ',
                               {'h': 'h', 'hit': 'h', 'p': 'p', 'partial': 'p',
                                'm': 'm', 'miss': 'm'}, card)
                if grade == 'h' and assisted:
                    grade = 'p'
                    print('  Assisted success: this card returns tomorrow for unaided recall.')
            old = commit(card, grade, sure, concept_tokens)
            if offer_stretch and card.get('Stretch'):
                stretch(card)
            if card is doc['cards'][-1] and all(c['id'] in progress for c in doc['cards']):
                tail = doc['blocks'][-1]
                if tail['kind'] == 'prose' and tail['line'] > card['line']:
                    closing = '\n'.join(line for line in tail['raw'].splitlines()
                                        if line.strip() != '---')
                    if closing.strip():
                        print('\n' + closing.strip() + '\n')
            if grade == 'm' and sure:
                print('  High-confidence miss: say why the key differs; this card returns tomorrow.')
            print(f"  Review on {old['next']}.")
    except CheckProblem as exc:
        print('\nSETUP PROBLEM: ' + str(exc))
        print('This card was not graded. Fix the check or tool, then resume.')
    except SessionStopped as exc:
        print('\n' + str(exc))
    summary()
    return progress

if __name__ == '__main__':
    class OneLineParser(argparse.ArgumentParser):
        def error(self, message):
            print(f'error: {message}', file=sys.stderr)
            raise SystemExit(2)

    ap = OneLineParser(description='Short, resumable Modern C++ study sessions.',
                       epilog='Example: python3 drive.py modules/01-foundations/lessons --today $(date +%F)')
    ap.add_argument('lessons_dir', help='Directory containing lesson Markdown files.')
    ap.add_argument('--progress', default='progress.json', help='Path for saved progress (default: progress.json).')
    ap.add_argument('--today', required=True, help='YYYY-MM-DD (explicit for determinism)')
    ap.add_argument('--auto', help='Scripted prompt responses, one per line: answer, confidence, then self-grade when requested; pretests consume only an answer.')
    ap.add_argument('--max-cards', type=int, default=12, help='Maximum cards this sitting (default: 12).')
    ap.add_argument('--max-minutes', type=float, default=20, help='Maximum sitting minutes (default: 20).')
    ap.add_argument('--reviews', type=int, default=4, help='Maximum due reviews before new cards (default: 4).')
    ap.add_argument('--new-cards', type=int, default=6, help='Maximum new recurring cards this sitting (default: 6).')
    resets = ap.add_mutually_exclusive_group()
    resets.add_argument('--reset', action='store_true', help='archive and clear all progress')
    resets.add_argument('--reset-lesson', metavar='LESSON', help='archive and restart one lesson')
    a = ap.parse_args()
    try:
        datetime.date.fromisoformat(a.today)
        if a.reset or a.reset_lesson:
            archive, label = reset_progress(a.lessons_dir, a.progress, a.today, a.reset_lesson)
            if archive is None:
                print(f'No progress file at {a.progress}; nothing archived or reset.')
            else:
                print(f'Archived {a.progress} to {archive}; reset {label}.')
        else:
            if min(a.max_cards, a.max_minutes, a.reviews, a.new_cards) < 0 or a.max_cards == 0 or a.max_minutes == 0:
                raise ValueError('sitting budgets must be positive (reviews may be zero)')
            run(a.lessons_dir, a.progress, a.today, a.auto, a.max_cards, a.max_minutes, a.reviews, a.new_cards)
    except (ValueError, OSError, re.error) as exc:
        print(f'error: {exc}', file=sys.stderr)
        sys.exit(2)
    except KeyboardInterrupt:
        print('\nSession interrupted; completed work remains saved.')
