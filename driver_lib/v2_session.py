"""Opt-in v2 instructional session. All durable state uses drive.save_progress."""
import datetime
import hashlib
import re
import time
from pathlib import Path

try:
    from . import tutorial_format as fmt
except ImportError:
    import tutorial_format as fmt


def _refs(doc, card):
    refs = fmt.concept_refs(doc, card)
    if refs:
        return refs
    legacy = fmt.resolve_legacy_source(doc, card)
    return [legacy] if legacy else []


def _requires(doc, concept):
    if not concept.get('Requires'):
        return []
    return fmt.concept_refs(doc, {'Concepts': concept['Requires'].splitlines()[0], 'line': concept['line']})


def _concept_text(concept):
    # Prompts and their keys are separate actions, never part of first teaching.
    lines = concept['raw'].splitlines()
    out = []
    skipping = False
    for line in lines:
        if re.match(r'^(Elaborate|ElaborateKey|HandsOn|HandsOnKey):', line):
            skipping = True
        elif re.match(r'^(Figure|Source|Requires):', line):
            skipping = False
        if not skipping and not line.startswith('FigureText:'):
            out.append(line)
    rendered = '\n'.join(out).strip()
    figure = (concept.get('Figure', '').splitlines() or [''])[0].strip()
    if re.fullmatch(r'\S+\.(?:png|jpe?g|gif|svg|webp)', figure, re.I):
        rendered = rendered.replace('Figure: ' + figure,
                                    'Figure (text equivalent): ' + concept.get('FigureText', ''))
    return _image_text(rendered)


def _image_text(value):
    return re.sub(r'!\[([^\]]+)\]\([^)]+\)', r'Figure (text equivalent): \1', value)


def _prose_text(raw):
    return re.sub(r'<!--.*?-->', '', raw, flags=re.S).strip()


def parse_confidence(value):
    """Explicit response probability or an unscored qualitative flag."""
    value = value.strip().lower()
    if not value: return None, None
    if value in ('sure', 'unsure'): return None, value
    try:
        number = float(value[:-1]) / 100 if value.endswith('%') else float(value)
    except ValueError as exc:
        raise ValueError('confidence must be 0–1, 0–100%, sure, unsure, or blank') from exc
    if not 0 <= number <= 1:
        raise ValueError('confidence must be between 0 and 1')
    return number, None


def _validate_visuals(docs):
    for doc in docs:
        for alt, target in re.findall(r'!\[([^\]]*)\]\(([^)]+)\)', doc['text']):
            if not alt.strip():
                raise fmt.FormatError(f"{doc['path']}: image {target} needs a text equivalent")
            if not (doc['path'].parent / target).is_file():
                raise fmt.FormatError(f"{doc['path']}: missing image {target}")
        for concept in doc['concepts']:
            figure = (concept.get('Figure', '').splitlines() or [''])[0].strip()
            if re.fullmatch(r'\S+\.(?:png|jpe?g|gif|svg|webp)', figure, re.I):
                if not (doc['path'].parent / figure).is_file():
                    raise fmt.FormatError(f"{doc['path']}:{concept['line']}: missing figure {figure}")
                if not concept.get('FigureText', '').strip():
                    raise fmt.FormatError(f"{doc['path']}:{concept['line']}: figure {figure} needs FigureText")


def build_actions(docs):
    _validate_visuals(docs)
    known = {c['identity'] for doc in docs for c in doc['concepts']}
    actions = []
    for doc in docs:
        cards = doc['cards']
        for concept in doc['concepts']:
            for ref in _requires(doc, concept):
                if ref not in known:
                    raise fmt.FormatError(f"{doc['path']}:{concept['line']}: unknown Requires reference {ref}")
        for card in cards:
            for ref in _refs(doc, card):
                if ref not in known:
                    raise fmt.FormatError(f"{doc['path']}:{card['line']}: unknown Concepts reference {ref}")
        # Keep prose and normal cards in authored order. Pretests are moved only
        # as far as needed to precede teaching of their linked concept.
        emitted_pretests = set()
        leading = []
        for block in doc['blocks']:
            if block['kind'] != 'prose':
                break
            leading.append(block)
            raw = _prose_text(block['raw'])
            if raw:
                actions.append({'kind': 'content', 'id': f"{doc['relative']}:{block['line']}", 'text': raw})
        for p in cards:
            if p['once']:
                refs = _refs(doc, p)
                actions.append({'kind': 'card', 'id': p['id'], 'card': p,
                                'concept': refs[0] if len(refs) == 1 else None,
                                'prerequisites': []})
                emitted_pretests.add(p['id'])
        for block in doc['blocks']:
            if block in leading:
                continue
            if block['kind'] == 'prose':
                raw = _prose_text(block['raw'])
                if raw:
                    actions.append({'kind': 'content', 'id': f"{doc['relative']}:{block['line']}", 'text': raw})
            elif block['kind'] == 'concept':
                c = block
                identity = c['identity']
                actions.append({'kind': 'teach', 'id': ':'.join(map(str, identity)),
                                'text': _concept_text(c), 'concept': identity,
                                'new': True, 'requires': _requires(doc, c)})
                for name, key in (('Elaborate', 'ElaborateKey'), ('HandsOn', 'HandsOnKey')):
                    if c.get(name):
                        actions.append({'kind': name.lower(), 'id': ':'.join(map(str, identity)) + ':' + name,
                                        'prompt': c[name], 'key': c.get(key, []), 'concept': identity})
            elif block['kind'] == 'card' and not block['once']:
                refs = _refs(doc, block)
                actions.append({'kind': 'card', 'id': block['id'], 'card': block,
                                'requires': refs or [c['identity'] for c in doc['concepts']]})
        for card in cards:
            if card['once'] and card['id'] not in emitted_pretests:
                actions.append({'kind': 'card', 'id': card['id'], 'card': card,
                                'requires': _refs(doc, card)})
    return actions


def paper(docs):
    """A complete offline artifact with answer keys after all attempts."""
    _validate_visuals(docs)
    lines = ['# Study guide', '', 'Time: use short sessions; stop after any step and resume at the next unchecked item.',
             'For each pretest, write a guess, check its key at the end, then return to the next step.',
             'For other questions, write an answer before opening the answer section.',
             'After a core answer and its key, try each optional stretch before viewing its key;',
             'a skipped or attempted stretch does not change the four-box schedule.',
             'Manual review: four boxes; after a hit review in 1, 2, 7, then 21 days.',
             'A miss returns to box 1 and is due tomorrow. Record the date and box on paper.',
             'To redo everything, first date and copy your tracking sheet and attempt log,',
             'then start a blank sheet and return to the first pretest below. To redo one',
             "lesson, keep that dated copy and all old attempts, clear only that lesson's",
             'pretest marks, checked teaching and practice steps, boxes, due dates and',
             'unfinished answer, then restart at that lesson\'s first pretest. Leave every',
             'other lesson\'s marks and due dates as they are.', '']
    keys = []
    for doc in docs:
        lines += [f"## {doc['relative']}", '']
        leading = []
        for block in doc['blocks']:
            if block['kind'] != 'prose':
                break
            leading.append(block)
        ordered = leading + [c for c in doc['blocks'] if c['kind'] == 'card' and c['once']]
        ordered += [b for b in doc['blocks'] if b not in leading and
                    not (b['kind'] == 'card' and b['once'])]
        for block in ordered:
            if block['kind'] == 'prose':
                lines += [_image_text(_prose_text(block['raw'])), '']
            elif block['kind'] == 'concept':
                lines += [_concept_text(block), '']
                for name, key in (('Elaborate', 'ElaborateKey'), ('HandsOn', 'HandsOnKey')):
                    if block.get(name):
                        label = f"{block['id']} {name}"
                        lines += [f"**{label}:** {_image_text(block[name])}", '']
                        keys += [(label, block.get(key, []))]
            elif block['kind'] == 'card':
                lines += [f"### {block['id']} [{block['type']}]", _image_text(block['Q']), '']
                if block.get('Hint'):
                    lines += [f"Hint (optional): {_image_text(block['Hint'])}", '']
                keys += [(block['id'], block['Key'])]
                if block.get('Stretch'):
                    lines += [f"Stretch (optional): {_image_text(block['Stretch'])}", '']
                    keys += [(block['id'] + ' stretch', block['StretchKey'])]
    lines += ['# Answer keys', 'Compare only after writing an attempt.', '']
    for label, bullets in keys:
        lines += [f'## {label}'] + [f'- {_image_text(b)}' for b in bullets] + ['']
    return '\n'.join(lines).rstrip() + '\n'


def run(lessons_dir, progress_path, today, replay=None, new_limit=6, max_cards=None,
        max_minutes=None, clock=None, wall_clock=None, ask_confidence=False):
    try:
        from . import drive
    except ImportError:
        import drive
    docs = fmt.parse_lessons(lessons_dir)
    actions = build_actions(docs)
    cards = [c for doc in docs for c in doc['cards']]
    if any(c['id'] == '_runtime' for c in cards):
        raise ValueError('_runtime is reserved for progress metadata')
    progress = drive.load_progress(progress_path)
    runtime = dict(progress.get('_runtime', {'box': 0, 'next': drive.NEVER, 'version': 2}))
    if runtime.get('version') not in (1, 2):
        raise ValueError('unsupported runtime version')
    runtime.update({'box': 0, 'next': drive.NEVER, 'version': 2})
    runtime.setdefault('completed_steps', [])
    runtime.setdefault('attempts_by_date', {})
    runtime.setdefault('introduced_by_date', {})
    runtime.setdefault('answered_steps', [])
    runtime.setdefault('feedback_steps', [])
    runtime.setdefault('attempt_records', [])
    runtime.setdefault('exposures', {})
    # Older v2 progress used bare labels. Preserve an unambiguous exposure;
    # repeated labels cannot safely be assigned to one lesson.
    for label, value in list(runtime['exposures'].items()):
        if ':' not in label:
            matches = [c['identity'] for doc in docs for c in doc['concepts'] if c['id'] == label]
            if len(matches) == 1:
                runtime['exposures'].setdefault(':'.join(map(str, matches[0])), value)
    runtime.setdefault('card_exposures', {})
    runtime.setdefault('assistance', {})
    runtime.setdefault('retry_by_date', {})
    runtime.pop('retry', None)  # old cross-date retry state is not valid
    session_exposures = dict(runtime['exposures'])
    completed = set(runtime['completed_steps'])
    monotonic = clock or time.monotonic
    start = monotonic()
    answer_count = 0
    replay_lines = None
    if replay:
        with open(replay, encoding='utf-8') as f:
            replay_lines = iter(f.read().splitlines())
    now = wall_clock or (lambda: datetime.datetime.now(datetime.timezone.utc))
    def stamp():
        return now().isoformat()
    manifest = Path(lessons_dir).resolve().parent / 'MANIFEST.md'
    manifest_text = manifest.read_text(encoding='utf-8') if manifest.exists() else ''
    estimate = re.search(r'^-\s*\*\*First pass:\*\*\s*(.+)$', manifest_text, re.M)
    print(f'# Session {today}')
    if estimate:
        print('First-pass time estimate: ' + estimate.group(1).strip())
    if max_minutes is not None:
        print(f'Time budget: {max_minutes:g} min; an in-progress answer may overrun it.')
    if max_cards is not None:
        print(f'Answer-prompt budget: {max_cards}.')

    def persist():
        updated = dict(progress)
        updated['_runtime'] = runtime
        drive.save_progress(progress_path, updated)
        progress.clear(); progress.update(updated)

    def ask(tag, prompt):
        nonlocal answer_count
        if tag in ('elaborate', 'hands_on', 'stretch'):
            if max_cards is not None and answer_count >= max_cards:
                raise drive.SessionStopped('Answer budget reached; resume later.')
            answer_count += 1
        if replay_lines is not None:
            try:
                line = next(replay_lines)
            except StopIteration:
                raise drive.SessionStopped('Replay exhausted; pending work remains saved.')
            actual, sep, value = line.partition('\t')
            if not sep or actual != tag:
                raise ValueError(f'replay tag mismatch: expected {tag}, got {actual}')
            value = value.replace('\\n', '\n') if tag == 'answer' else value
            print(prompt + value)
        else:
            try:
                value = input(prompt)
                if tag == 'answer' and (value.lstrip().startswith('```') or value.rstrip().endswith(':')):
                    lines = [value]
                    fence = value.lstrip().startswith('```')
                    while True:
                        more = input('... ')
                        if fence and more.strip().startswith('```'):
                            lines.append(more); break
                        if not fence and not more.strip():
                            break
                        lines.append(more)
                    value = '\n'.join(lines)
            except (EOFError, KeyboardInterrupt):
                raise drive.SessionStopped('Interrupted; completed work remains saved.')
        if value.strip() == ':quit':
            raise drive.SessionStopped('Stopped; completed work remains saved.')
        if tag == 'answer' and value.strip() not in (':hint', ':stretch'):
            answer_count += 1
        return value

    def confidence():
        nonlocal replay_lines
        if replay_lines is not None:
            line = next(replay_lines, None)
            if line is None: return None, None
            if line.startswith('confidence\t'):
                value = line.partition('\t')[2]
                print('Response confidence: ' + value)
                return parse_confidence(value)
            replay_lines = iter([line, *replay_lines])
            return None, None
        if ask_confidence:
            while True:
                try:
                    return parse_confidence(ask('confidence', 'Response confidence [0–1, %, sure/unsure, blank]: '))
                except ValueError as exc:
                    print(exc)
        return None, None

    def time_ok():
        if max_minutes is not None and monotonic() - start >= max_minutes * 60:
            raise drive.SessionStopped('Time budget reached; resume later. An in-progress answer may overrun the estimate.')

    def key(bullets):
        print('  Key:')
        for b in bullets:
            print('    - ' + _image_text(b))

    def finish_pending(resumed=True):
        pending = runtime.get('pending')
        if not pending:
            return
        card = next((c for c in cards if c['id'] == pending['card']), None)
        if not card:
            raise ValueError(f"pending card {pending['card']} is absent from lessons")
        if resumed:
            print(f"Resuming [{card['id']}] after answer reveal; exposed answer cannot count as a new attempt.")
        key(card['Key'])
        if not card['once'] and pending.get('grade') is None:
            while True:
                grade = ask('grade', 'Self-grade [h]it / [p]artial / [m]iss: ').strip().lower()
                if grade in ('h', 'p', 'm'):
                    break
                print('Enter h, p or m.')
            pending['grade'] = grade
            persist()
        if card.get('Stretch'):
            print('  Stretch (optional): ' + _image_text(card['Stretch']))
            if 'stretch_answer' not in pending:
                pending['stretch_answer'] = ask('stretch', 'Stretch attempt [Enter to skip]: ')
                persist()
            if card.get('StretchKey'):
                key(card['StretchKey'])
        ask('continue', 'Continue [Enter]: ')
        old = dict(progress.get(card['id'], {'box': card['box']}))
        token = pending['step_token']
        if token not in runtime['feedback_steps']:
            runtime['feedback_steps'].append(token)
        if card['once']:
            old.update({'box': card['box'], 'next': drive.NEVER})
        else:
            grade = pending['grade']
            old['last_grade'] = grade
            attempt_date = pending['step_token'].split(':', 1)[0]
            attempts = runtime['attempts_by_date'].setdefault(attempt_date, {})
            info = dict(attempts.get(card['id'], {'count': 0, 'missed': False, 'promoted': False}))
            info['count'] += 1
            info['missed'] = info['missed'] or grade == 'm'
            box = old['box']
            if grade == 'm':
                box = 1
                info['last_miss_card_count'] = runtime.get('card_count', 0) + 1
                info['last_miss_concept'] = pending.get('primary_concept')
                retry = runtime['retry_by_date'].setdefault(attempt_date, [])
                if card['id'] not in retry and info['count'] < 3:
                    retry.append(card['id'])
            elif grade == 'h' and not info['missed'] and not info['promoted']:
                # Fresh hit enters box 1. Promotion requires a later calendar day.
                if card['id'] in progress and old.get('last_hit') != attempt_date:
                    box = min(4, box + 1)
                    info['promoted'] = True
            if grade == 'h':
                old['last_hit'] = attempt_date
            if grade == 'h' or info['count'] >= 3:
                retry = runtime['retry_by_date'].setdefault(attempt_date, [])
                retry[:] = [cid for cid in retry if cid != card['id']]
            old['box'] = box
            if info['missed'] or grade != 'h':
                old['next'] = (datetime.date.fromisoformat(attempt_date) + datetime.timedelta(days=1)).isoformat()
            else:
                old['next'] = drive.next_date(attempt_date, box)
            attempts[card['id']] = info
            runtime['card_count'] = runtime.get('card_count', 0) + 1
            runtime['last_attempt_concept'] = pending.get('primary_concept')
        progress[card['id']] = old
        if not any(event['id'] == token for event in runtime['attempt_records']):
            objective = sorted({':'.join(map(str, r)) for doc in docs for c in doc['cards']
                                if c['id'] == card['id'] for r in _refs(doc, c)})
            runtime['attempt_records'].append({
                'id': token, 'card': card['id'],
                'card_version': hashlib.sha256(card['raw'].encode()).hexdigest()[:12],
                'objective': objective, 'mode': 'T1', 'grade': pending.get('grade'),
                'grading_source': ('pretest' if card['once'] else
                                   ('check' if card['AcceptIf'].strip().startswith('check(') else 'predicate')
                                   if card.get('AcceptIf') and pending.get('variant') == 'core' else 'self'),
                'assistance': pending.get('assistance', []),
                'feedback_exposed': True, 'answered_at': pending.get('answered_at'),
                'feedback_at': stamp(), 'confidence': pending.get('confidence'),
                'qualitative_confidence': pending.get('qualitative_confidence'),
                'exposure_at': pending.get('exposure_at'),
                'delay_seconds': pending.get('delay_seconds'),
                'variant': pending.get('variant', 'core')})
        for objective in runtime['attempt_records'][-1]['objective']:
            runtime['exposures'][objective] = stamp()
        runtime['card_exposures'][card['id']] = stamp()
        runtime['assistance'].pop(card['id'], None)
        if pending.get('action'):
            completed.add(pending['action'])
            runtime['completed_steps'] = sorted(completed)
        runtime.pop('pending', None)
        persist()
        print(f"  next review: {old['next']} (box {old['box']})")

    def finish_pending_step(resumed=True):
        pending = runtime.get('pending_step')
        if not pending:
            return
        action = next((a for a in actions if a['kind'] + ':' + a['id'] == pending['action']), None)
        if action is None:
            raise ValueError(f"pending instructional step {pending['action']} is absent")
        if resumed:
            print('Resuming after your response; feedback was exposed.')
        key(action['key'])
        runtime['exposures'][':'.join(map(str, action['concept']))] = stamp()
        session_exposures[':'.join(map(str, action['concept']))] = runtime['exposures'][':'.join(map(str, action['concept']))]
        ask('continue', 'Continue [Enter]: ')
        completed.add(pending['action'])
        runtime['completed_steps'] = sorted(completed)
        runtime.pop('pending_step', None)
        persist()

    def card_action(card, action_id=None):
        if card['id'] in runtime['attempts_by_date'].get(today, {}) and runtime['attempts_by_date'][today][card['id']]['count'] >= 3:
            return
        linked = ((card.get('SuppressedBy') or '').splitlines() or [''])[0]
        if linked and progress.get(linked, {}).get('last_grade') == 'h':
            return
        question = _image_text(card['Q'])
        print(f"[{card['id']}] {question}")
        assistance = runtime['assistance'].setdefault(card['id'], [])
        while True:
            answer = ask('answer', 'Your answer (from memory; :hint): ')
            if answer.strip() == ':hint':
                assistance.append('hint')
                persist()
                print('Hint: ' + _image_text(card.get('Hint') or '(no authored hint)'))
                continue
            if answer.strip() == ':stretch':
                print('Answer the core question first; optional stretch follows its feedback.')
                continue
            break
        answered_at = stamp()
        objectives = {':'.join(map(str, r)) for doc in docs for c in doc['cards']
                      if c['id'] == card['id'] for r in _refs(doc, c)}
        exposures = [session_exposures[o] for o in objectives if o in session_exposures]
        if card['id'] in runtime['card_exposures']:
            exposures.append(runtime['card_exposures'][card['id']])
        exposure_at = max(exposures) if exposures else None
        delay_seconds = None
        if exposure_at:
            elapsed = (datetime.datetime.fromisoformat(answered_at) -
                       datetime.datetime.fromisoformat(exposure_at)).total_seconds()
            if elapsed >= 0: delay_seconds = elapsed
        numeric_confidence, qualitative_confidence = confidence()
        grade = None
        if not card['once'] and card.get('AcceptIf'):
            base = str(Path(lessons_dir).resolve().parent)
            rejected = card.get('RejectIf') and drive.eval_predicate(card['RejectIf'], answer, base)
            grade = 'm' if rejected or not drive.eval_predicate(card['AcceptIf'], answer, base) else 'h'
            print('  -> ' + ('HIT' if grade == 'h' else 'MISS') + ' (auto-graded)')
        attempt_number = runtime['attempts_by_date'].get(today, {}).get(card['id'], {}).get('count', 0) + 1
        used_tokens = {event['id'] for event in runtime['attempt_records']}
        used_tokens.update(runtime['answered_steps'])
        step_token = f"{today}:{card['id']}:{attempt_number}"
        while step_token in used_tokens:
            attempt_number += 1
            step_token = f"{today}:{card['id']}:{attempt_number}"
        runtime['answered_steps'].append(step_token)
        runtime['pending'] = {'card': card['id'], 'action': action_id, 'answer': answer,
                              'step_token': step_token,
                              'grade': grade, 'variant': 'core', 'answer_exposed': True,
                              'answered_at': answered_at, 'confidence': numeric_confidence,
                              'qualitative_confidence': qualitative_confidence,
                              'assistance': assistance, 'exposure_at': exposure_at,
                              'delay_seconds': delay_seconds,
                              'primary_concept': sorted(objectives)[0] if objectives else None}
        persist()
        finish_pending(resumed=False)

    def prereqs_met(refs):
        if any('teach:' + ':'.join(map(str, r)) not in completed for r in refs):
            return False
        prereq_cards = [c for doc in docs for c in doc['cards']
                        if not c['once'] and
                        not (c.get('SuppressedBy') and progress.get(c['SuppressedBy'].strip(), {}).get('last_grade') == 'h')
                        and any(_refs(doc, c) == [r] for r in refs)]
        return all(progress.get(c['id'], {}).get('last_grade') == 'h' for c in prereq_cards)

    try:
        finish_pending()
        finish_pending_step()
        # Spaced reviews are asked before further new material. Fresh initial
        # actions are tracked separately so a stopped quiz stays reachable.
        reviewed = set()
        for card in drive.due_order(cards, progress, today):
            if card['id'] in progress and not card['once'] and card['id'] not in reviewed:
                if max_cards is not None and answer_count >= max_cards:
                    raise drive.SessionStopped('Answer budget reached; resume later.')
                time_ok()
                card_action(card)
                reviewed.add(card['id'])
        for action in actions:
            aid = action['kind'] + ':' + action['id']
            if aid in completed:
                continue
            if max_cards is not None and answer_count >= max_cards:
                raise drive.SessionStopped('Answer budget reached; resume later.')
            time_ok()
            if action.get('prerequisites') and not prereqs_met(action['prerequisites']):
                continue
            if action['kind'] == 'teach' and action.get('requires') and not prereqs_met(action['requires']):
                continue
            concept = action.get('concept')
            if (concept and action['kind'] != 'teach'
                    and not (action['kind'] == 'card' and action['card']['once'])
                    and 'teach:' + ':'.join(map(str, concept)) not in completed):
                continue
            if action.get('new') and concept:
                introduced = runtime['introduced_by_date'].setdefault(today, [])
                identity_key = ':'.join(map(str, concept))
                if identity_key not in introduced and len(introduced) >= new_limit:
                    continue
                if identity_key not in introduced:
                    introduced.append(identity_key)
                    persist()
            if action['kind'] == 'card':
                card = action['card']
                linked = ((card.get('SuppressedBy') or '').splitlines() or [''])[0]
                if linked and progress.get(linked, {}).get('last_grade') == 'h':
                    completed.add(aid); runtime['completed_steps'] = sorted(completed); persist()
                    continue
                if card['id'] in progress and not card['once']:
                    completed.add(aid); runtime['completed_steps'] = sorted(completed); persist(); continue
                requires = action.get('requires', [])
                if requires and any('teach:' + ':'.join(map(str, r)) not in completed for r in requires):
                    continue
                card_action(card, aid)
            elif action['kind'] in ('content', 'teach'):
                print(action['text'])
                ask('continue', 'Continue [Enter]: ')
                completed.add(aid); runtime['completed_steps'] = sorted(completed); persist()
                if action['kind'] == 'teach':
                    identity = ':'.join(map(str, action['concept']))
                    runtime['exposures'][identity] = stamp()
                    session_exposures[identity] = runtime['exposures'][identity]
                    persist()
            else:
                print(action['prompt'])
                answer = ask('elaborate' if action['kind'] == 'elaborate' else 'hands_on', 'Your response: ')
                runtime['pending_step'] = {'action': aid, 'answer': answer}
                persist()
                finish_pending_step(resumed=False)
        # Retry only after a different card has been attempted since the miss.
        for cid in list(runtime['retry_by_date'].get(today, [])):
            info = runtime['attempts_by_date'].get(today, {}).get(cid, {})
            if (info.get('count', 0) >= 3 or
                    runtime.get('card_count', 0) <= info.get('last_miss_card_count', 0) or
                    runtime.get('last_attempt_concept') == info.get('last_miss_concept')):
                continue
            if max_cards is not None and answer_count >= max_cards:
                raise drive.SessionStopped('Answer budget reached; resume later.')
            time_ok()
            card = next(c for c in cards if c['id'] == cid)
            card_action(card)
        print('Session complete. Return on the next due date after a sleep interval.')
    except drive.SessionStopped as exc:
        print('\n' + str(exc))
    return progress
