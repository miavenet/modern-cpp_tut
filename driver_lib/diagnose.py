"""Read-only diagnose mode: answer a short mixed set, then see what to read.

Never loads, writes or changes any progress file. The set is a pure function of
the lesson files and --today, so the same day gives the same questions.
"""
import hashlib
from pathlib import Path

import drive
from driver_lib import tutorial_format

MINUTES_PER_CARD = 2  # answer, read the key, self-grade
GRADES = {'h': 'h', 'hit': 'h', 'p': 'p', 'partial': 'p', 'm': 'm', 'miss': 'm'}


def lesson_dirs(path):
    """A lessons directory, a module directory, or a folder of modules."""
    root = Path(path)
    if (root / 'lessons').is_dir():
        return [root / 'lessons']
    if any(root.glob('*.md')):
        return [root]
    found = sorted(p / 'lessons' for p in root.iterdir() if (p / 'lessons').is_dir())
    if not found:
        raise ValueError(f'no lessons found under {path}')
    return found


def eligible(card):
    """One-line recall cards; pretests, labs and compiled checks stay in sittings."""
    return not card['once'] and not (card.get('AcceptIf') or '').startswith('check(')


def _rank(today, text):
    return hashlib.sha256(f'{today}:{text}'.encode('utf-8')).hexdigest()


def select(groups, today, size):
    """Spread `size` cards evenly over lessons; consecutive cards differ in lesson.

    `groups` is [(doc, cards)] in course order. The day rotates the start lesson
    and the card picked within each lesson, so another day gives another set.
    """
    groups = [(doc, sorted(cards, key=lambda c: _rank(today, c['id'])))
              for doc, cards in groups if cards]
    if not groups:
        return []
    shift = int(_rank(today, 'start'), 16) % len(groups)
    groups = groups[shift:] + groups[:shift]
    chosen, depth = [], 0
    while len(chosen) < size:
        live = [g for g in groups if depth < len(g[1])]
        if not live:
            break
        want = min(size - len(chosen), len(live))
        for k in range(want):
            doc, cards = live[k * len(live) // want]
            chosen.append((doc, cards[depth]))
        depth += 1
    return chosen


def section(doc, card):
    """Where the card is taught: its first listed concept in this lesson, else the
    nearest `## ` or CONCEPT heading above it; a practice-bank card is its own text."""
    if doc['path'].name.endswith('practice-bank.md'):
        return f"Card {card['id']}"
    titles = {c['identity']: f"CONCEPT {c['id']} {c['title']}".strip() for c in doc['concepts']}
    for ref in tutorial_format.concept_refs(doc, card):
        if ref in titles:
            return titles[ref]
    heading = None
    lines = doc['text'].splitlines()[:card['line'] - 1]
    for line, fenced in zip(lines, tutorial_format._fences(lines)):
        match = None if fenced else tutorial_format.CONCEPT.match(line)
        if not fenced and (line.startswith('## ') or match):
            heading = (f'CONCEPT {match.group(1)} {match.group(2)}'.strip() if match
                       else line[3:].strip())
    return heading or f"Card {card['id']}"


def lesson_minutes(doc):
    return sum(card.get('minutes', 0) for card in doc['cards'])


def weight(grade, sure):
    """Higher means read sooner; a sure miss is the costliest misconception."""
    return {'m': 3 + sure, 'p': 2, 'h': 0 if sure else 1}[grade]


def recommend(results):
    """Group missed or shaky cards by lesson, worst lesson first, course order on ties.

    `results` is [(doc, card, grade, sure)]. Returns (reads, solid) where reads is
    [(doc, total weight, [(section, card, grade, sure)])].
    """
    order = {}
    reads = {}
    solid = []
    for doc, card, grade, sure in results:
        key = str(doc['path'])
        order.setdefault(key, (doc['path'].parent.as_posix(), doc['path'].name))
        score = weight(grade, sure)
        if not score:
            solid.append(card)
            continue
        entry = reads.setdefault(key, [doc, 0, []])
        entry[1] += score
        entry[2].append((section(doc, card), card, grade, sure))
    ranked = sorted(reads.items(), key=lambda kv: (-kv[1][1], order[kv[0]]))
    return [tuple(entry) for _, entry in ranked], solid


def report(results):
    reads, solid = recommend(results)
    labels = {'m': 'missed', 'p': 'partial', 'h': 'unsure hit'}
    hits = sum(1 for _, _, grade, _ in results if grade == 'h')
    print(f'\nDiagnosis: {hits}/{len(results)} hits. No progress was recorded.')
    if reads:
        print('Read next (most to fix first):')
        for number, (doc, _, items) in enumerate(reads, 1):
            print(f"  {number}. {doc['path']} (about {lesson_minutes(doc)} min)")
            for name, card, grade, sure in items:
                note = labels[grade] + (' while sure' if grade == 'm' and sure else '')
                print(f"     - {name}: {card['id']} {note}")
    elif results:
        print('Read next: nothing; every answer was a sure hit.')
    print('Solid: ' + (', '.join(f"{c['id']} ({c['topic']})" for c in solid) or 'none yet'))


def run(path, today, size=10, auto=None):
    """Ask the set, reveal each key, take a grade, then print the reading list."""
    groups = []
    for lessons in lesson_dirs(path):
        for doc in tutorial_format.parse_lessons(lessons):
            groups.append((doc, [c for c in doc['cards'] if eligible(c)]))
    chosen = select(groups, today, size)
    answers = Path(auto).read_text(encoding='utf-8').splitlines() if auto else None
    index = 0

    def ask(prompt):
        nonlocal index
        while True:
            if answers is not None:
                if index >= len(answers):
                    raise drive.SessionStopped('Scripted answers exhausted.')
                value = answers[index]
                index += 1
                print(prompt + value)
            else:
                try:
                    value = input(prompt)
                except (EOFError, KeyboardInterrupt):
                    raise drive.SessionStopped('Stopped.')
            raw = value.strip()
            if raw.lower() in (':quit', '/stop', '/quit') or not raw:
                raise drive.SessionStopped('Stopped.')
            if raw.lower() in (':help', '/help'):
                print('Commands: :skip (don\'t know), :quit; nothing is saved either way.')
                continue
            if raw.lower() in (':skip', '/skip'):
                return ':skip'
            if raw.startswith((':', '/')):
                print('unknown command, type :help')
                continue
            return value

    def choice(prompt, allowed):
        while True:
            value = ask(prompt).strip().lower()
            if value == ':skip':
                return 'm'
            if value in allowed:
                return allowed[value]
            print('Please type ' + ', '.join(sorted(allowed)) + '.')

    print(f'Diagnose: {len(chosen)} questions from {len({str(d["path"]) for d, _ in chosen})} lessons, '
          f'about {MINUTES_PER_CARD * len(chosen)} min. Read-only: no progress file is written.')
    print('Answer on one line from memory; :skip means "don\'t know"; :quit stops and shows the list so far.')
    results = []
    try:
        for number, (doc, card) in enumerate(chosen, 1):
            print(f"\n[{number}/{len(chosen)} · {card['id']}] {card['Q']}")
            attempt = ask('Your answer (from memory): ')
            sure = attempt != ':skip' and choice(
                '  Confidence [s]ure / [u]nsure: ',
                {'s': True, 'sure': True, 'u': False, 'unsure': False})
            if attempt == ':skip':
                grade = 'm'
                print('  -> MISS (you chose "don\'t know").')
            elif card.get('AcceptIf') and not drive.needs_self_grade(card):
                base = str(doc['path'].parent.parent)
                ok, _ = drive.eval_predicate(card['AcceptIf'], attempt, base)
                if card.get('RejectIf') and drive.eval_predicate(card['RejectIf'], attempt, base)[0]:
                    ok = False
                grade = 'h' if ok else 'm'
                print(f"  -> {'HIT' if ok else 'MISS'}")
            else:
                grade = None
            print('  Key:')
            for item in card.get('Key', []):
                print('    - ' + item)
            if grade is None:
                grade = choice('  Self-grade [h]it / [p]artial / [m]iss: ', GRADES)
            print(f"  Taught in: {doc['path'].name} § {section(doc, card)}")
            results.append((doc, card, grade, sure))
    except drive.CheckProblem as exc:
        print('\nSETUP PROBLEM: ' + str(exc))
    except drive.SessionStopped as exc:
        print('\n' + str(exc))
    report(results)
    return results
