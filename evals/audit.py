#!/usr/bin/env python3
"""Author-time structural eval for lesson structure.

Audits a tutorial directory (with lessons/*.md cards) for the structural
properties durable learning requires, WITHOUT needing a learner or a model.
Structural failures are binary; style/item heuristics are warnings. This does
not judge content quality.

Usage:  ./audit.py <tutorial_dir>
Exit 0 if all structural checks pass, 1 otherwise.
"""
import hashlib, json, os, re, sys
from pathlib import Path
try:
    from driver_lib import tutorial_format
except ImportError:
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'driver_lib'))
    import tutorial_format

CARD_RE = re.compile(r'^###\s+CARD\s+(\S+)\s*(.*)$')

def parse(lessons_dir):
    return [card for doc in tutorial_format.parse_lessons(lessons_dir)
            for card in doc['cards']]


def find_protocol(tutorial_dir):
    from pathlib import Path
    path = Path(tutorial_dir).resolve()
    for directory in (path, *path.parents):
        candidate = directory / 'PROTOCOL.md'
        if candidate.is_file():
            return candidate
    return None


def _coverage(manifest, cards):
    """Read objective rows from any coverage heading/table column order."""
    objectives = _objective_numbers(manifest)
    if cards and all('objective' in card for card in cards.values()):
        problems = []
        known = {int(number) for number in objectives}
        for cid, card in cards.items():
            if card['objective'] not in known:
                problems.append(f'{cid}: unknown objective {card["objective"]}')
        for number in known:
            mapped = [card for card in cards.values() if card['objective'] == number]
            if not mapped:
                problems.append(f'objective {number} has no mapped card')
                continue
            kinds = {card['type'] for card in mapped}
            if 'recall' not in kinds:
                problems.append(f'objective {number} missing recall card')
            if not kinds.intersection({'apply', 'transfer', 'analyze', 'synthesize', 'lab', 'judgement'}):
                problems.append(f'objective {number} missing application/transfer card')
        return problems
    card_ids = set(cards)
    table_rows = [[cell.strip() for cell in line.strip().strip('|').split('|')]
                  for line in manifest.splitlines() if line.startswith('|')]
    header = next((row for row in table_rows if row and row[0].lower() == 'objective' and
                   any(cell.lower() == 'cards' for cell in row)), None)
    problems = []
    if not header:
        return ['coverage map needs Objective and Cards columns']
    cards_column = next(i for i, cell in enumerate(header) if cell.lower() == 'cards')
    for objective in objectives:
        matches = [row for row in table_rows if row and re.match(r'^' + re.escape(objective) + r'(?:\b|\s)', row[0])]
        if not matches:
            problems.append(f'objective {objective} absent from coverage map')
            continue
        row_text = ' '.join(row[cards_column] for row in matches if len(row) > cards_column)
        row_text = re.sub(r'\([^)]*\)', '', row_text)
        listed = set(re.split(r'[,;\s]+', row_text.strip())) - {''}
        mapped = card_ids & listed
        if not mapped:
            problems.append(f'objective {objective} has no mapped card')
        else:
            kinds = {cards[item]['type'] for item in mapped}
            if 'recall' not in kinds:
                problems.append(f'objective {objective} missing recall card')
            if not kinds.intersection({'apply', 'transfer', 'analyze'}):
                problems.append(f'objective {objective} missing application/transfer card')
        unknown = {item for item in listed if item not in card_ids}
        problems.extend(f'unknown card {item} in objective {objective}' for item in sorted(unknown))
    if not objectives:
        problems.append('no numbered learning objectives')
    return problems


def _objective_numbers(manifest):
    section = re.search(r'^## Learning objectives(?:\s*\([^\n]*\))?\s*$\n(.*?)(?=^## |\Z)',
                        manifest, re.M | re.S | re.I)
    return re.findall(r'^\s*(\d+)\.\s+', section.group(1), re.M) if section else []


def _v2_checks(tutorial_dir, docs, manifest):
    root = Path(tutorial_dir).resolve()
    failures, warnings = [], []
    try:
        from driver_lib import v2_session
    except ImportError:
        import v2_session
    try:
        v2_session._validate_visuals(docs)
    except tutorial_format.FormatError as exc:
        failures.append(str(exc))
    concepts = {concept['identity'] for doc in docs for concept in doc['concepts']}
    cards = {card['id']: card for doc in docs for card in doc['cards']}
    card_ids = set(cards)
    for doc in docs:
        for item in doc['concepts']:
            requires = item.get('Requires', '').strip()
            for token in re.split(r'[,\s]+', requires):
                if not token:
                    continue
                try:
                    identity = tutorial_format.concept_refs(doc, {'Concepts': token, 'line': item['line']})[0]
                except (tutorial_format.FormatError, IndexError) as exc:
                    failures.append(str(exc)); continue
                if identity not in concepts:
                    failures.append(f"{doc['relative']}:{item['line']}: unresolved Requires {token}")
            for prompt, key in (('Elaborate', 'ElaborateKey'), ('HandsOn', 'HandsOnKey')):
                if item.get(prompt, '').strip() and not item.get(key):
                    failures.append(f"{doc['relative']}:{item['line']}: {prompt} needs {key}")
            figure = item.get('Figure', '').strip()
            if figure and re.search(r'\.(?:png|jpe?g|gif|svg|webp)$', figure, re.I):
                if not (doc['path'].parent / figure).is_file():
                    failures.append(f"{doc['relative']}:{item['line']}: missing figure {figure}")
                if not item.get('FigureText', '').strip():
                    failures.append(f"{doc['relative']}:{item['line']}: figure needs FigureText")
        for card in doc['cards']:
            if not card.get('Concepts', '').strip():
                failures.append(f"{doc['relative']}:{card['line']}: {card['id']} missing Concepts")
            else:
                try:
                    refs = tutorial_format.concept_refs(doc, card)
                    for ref in refs:
                        if ref not in concepts:
                            failures.append(f"{doc['relative']}:{card['line']}: {card['id']} unresolved Concepts {ref}")
                except tutorial_format.FormatError as exc:
                    failures.append(str(exc))
            if (card.get('Stretch') or '').strip() and not card.get('StretchKey'):
                failures.append(f"{card['id']}: Stretch needs StretchKey")
            if card.get('SuppressedBy', '').strip() and card['SuppressedBy'].strip() not in card_ids:
                failures.append(f"{card['id']}: unknown SuppressedBy card {card['SuppressedBy']}")
            if re.search(r'\bnot\b', card['Q'], re.I):
                warnings.append(f"{card['id']}: negative stem; check for ambiguity")
    failures.extend(_coverage(manifest, cards))
    if not (root / 'assessment').is_dir():
        warnings.append('no assessment/ form (optional for v2 tutorials)')
    else:
        try:
            try:
                from driver_lib import v2_assessment
            except ImportError:
                import v2_assessment
            form = v2_assessment.load_form(root)
            objective_count = len(_objective_numbers(manifest))
            expected = {(objective, kind) for objective in form['prerequisites']
                        for kind in ('retention', 'transfer')}
            actual_items = {(item['objective'], item['kind']) for item in form['items']}
            labels = {concept['id'] for doc in docs for concept in doc['concepts']}
            if (len(form['prerequisites']) != objective_count or
                    len(set(form['prerequisites'])) != objective_count or
                    not set(form['prerequisites']) <= labels):
                failures.append('assessment prerequisites must map one-to-one to objectives')
            if expected - actual_items:
                failures.append('assessment missing objective/kind pairs: ' + str(sorted(expected - actual_items)))
            paper = root / 'assessment' / 'FORM.md'
            if not paper.is_file() or paper.read_text(encoding='utf-8') != v2_assessment.paper(form):
                failures.append('assessment/FORM.md missing or stale')
        except (ValueError, KeyError, OSError) as exc:
            failures.append(f'invalid assessment form: {exc}')
    if find_protocol(root) is None:
        failures.append('PROTOCOL.md missing (local or course root)')
    if not (root / 'STUDY.md').is_file():
        failures.append('STUDY.md paper artifact missing')
    # Scan authored content, not implementation, progress or negative fixtures.
    authored = [root / 'MANIFEST.md', *sorted((root / 'lessons').glob('*.md'))]
    authored += sorted(p for p in (root / 'assessment').rglob('*') if p.is_file())
    authored += [p for p in (root / 'PROTOCOL.md', root / 'STUDY.md') if p.is_file()]
    for path in authored:
        if 'VERIFY:' in path.read_text(encoding='utf-8'):
            failures.append(f'unresolved VERIFY: in {path.relative_to(root)}')
    for problem in failures:
        print(f'  [FAIL] v2: {problem}')
    for warning in warnings:
        print(f'  [WARN] v2: {warning}')
    if not failures:
        print('  [PASS] v2 dependencies, keys, coverage, assets and coverage')
    return not failures

def main(d):
    lessons = os.path.join(d, 'lessons')
    checks = []  # (ok, message)
    try:
        version = tutorial_format.format_version(d)
    except tutorial_format.FormatError as exc:
        print(f'FAIL: {exc}')
        return 1
    if not os.path.isdir(lessons):
        print(f"FAIL: no lessons/ dir in {d}"); return 1
    try:
        docs = tutorial_format.parse_lessons(lessons)
        cards = [card for doc in docs for card in doc['cards']]
    except tutorial_format.FormatError as exc:
        print(f'FAIL: {exc}')
        return 1
    checks.append((len(cards) > 0, f"{len(cards)} question cards parsed"))

    # Every card has a Key and a Source.
    no_key = [c['id'] for c in cards if not c['Key']]
    checks.append((not no_key, f"all cards have Key bullets" + (f" — MISSING: {no_key}" if no_key else "")))
    no_src = [c['id'] for c in cards if not c['Source']]
    checks.append((not no_src, "all cards have a Source" + (f" — MISSING: {no_src}" if no_src else "")))

    # Count only predicates the deployed driver actually uses automatically.
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    import drive
    t1 = [c['id'] for c in cards if c['AcceptIf'] and not drive.needs_self_grade(c)]
    t1_message = (f"{len(t1)}/{len(cards)} cards T1-auto-gradable (AcceptIf); rest need self-report/T2+"
                  if version == 1 else
                  f"{len(t1)}/{len(cards)} cards T1-auto-gradable (AcceptIf); rest use self-report")
    code_cards = any(re.search(r'`|\bcode\b|\bcommand\b', c['Q'], re.I) for c in cards)
    checks.append((len(t1) > 0 or (version == 2 and not code_cards),
                   t1_message))

    # Recall + application coverage present (transfer items exist).
    types = {c['type'] for c in cards}
    has_recall = any(t in types for t in ('recall',))
    has_apply = any(t in types for t in ('apply', 'transfer', 'analyze'))
    checks.append((has_recall, "has recall-type cards"))
    checks.append((has_apply, "has application/transfer-type cards (varied-context checks)"))

    # LEARN-3: pretest cards exist (predict-before-reveal, asked once, first).
    pretests = [c for c in cards if c['once']]
    checks.append((len(pretests) > 0,
                   f"LEARN-3: {len(pretests)} pretest card(s) present (commit-a-guess before concepts)"))

    # LEARN-4: completion cards exist (faded scaffolding between worked concept
    # and independent free recall — no cliff-edge).
    completions = [c for c in cards if c['type'] == 'completion']
    checks.append((len(completions) > 0,
                   f"LEARN-4: {len(completions)} completion card(s) present (scaffold fades, no cliff)"))

    # A constrained, whole-input answer grammar needs no contradiction guard:
    # extra prose cannot match it. Open-ended keyword expressions still do.
    def closed_regex(expr):
        match = re.fullmatch(r'regex\(/(.*)/[a-z]*\)', expr)
        if not match:
            return False
        pattern = match.group(1)
        return ((pattern.startswith('^') and pattern.endswith('$')) or
                (pattern.startswith(r'\A') and pattern.endswith(r'\Z')))

    unguarded = [c['id'] for c in cards
                 if c['AcceptIf'] and 'check(' not in c['AcceptIf'] and
                 not c['RejectIf'] and not closed_regex(c['AcceptIf'])]
    checks.append((not unguarded,
                   "regex-guard: open-ended regex needs a guard; constrained full-input grammar is allowed"
                   + (f" — UNGUARDED: {unguarded}" if unguarded else "")))

    # Every check() script referenced actually exists and is executable.
    missing_checks = []
    for c in cards:
        for cm in re.finditer(r'check\(([^)]+)\)', c['AcceptIf'] or ''):
            p = os.path.join(d, cm.group(1))
            if not (os.path.isfile(p) and os.access(p, os.X_OK)):
                missing_checks.append(f"{c['id']}→{cm.group(1)}")
    checks.append((not missing_checks,
                   "all check() scripts exist and are executable"
                   + (f" — MISSING: {missing_checks}" if missing_checks else "")))

    # MANIFEST exists and states tiers + time cost.
    man = os.path.join(d, 'MANIFEST.md')
    mtext = Path(man).read_text() if os.path.exists(man) else ''
    checks.append((bool(mtext), "MANIFEST.md present"))
    checks.append(('tiers:' in mtext, "MANIFEST declares tiers (dual-mode)"))
    checks.append((bool(re.search(r'time cost|min\b|First pass', mtext, re.I)),
                   "MANIFEST states a time cost"))

    # ---- ENFORCEABLE anti-hallucination / adversarial-review gates (LEARN-13) ----
    # No unresolved VERIFY: markers anywhere in the learner-facing lessons.
    lesson_text = ""
    for f in os.listdir(lessons):
        if f.endswith('.md'):
            lesson_text += Path(lessons, f).read_text()
    verify_hits = [ln for ln in lesson_text.splitlines() if 'VERIFY:' in ln]
    checks.append((not verify_hits,
                   "no unresolved VERIFY: markers in lessons"
                   + (f" — {len(verify_hits)} FOUND" if verify_hits else "")))
    leaked = [ln for ln in lesson_text.splitlines()
              if re.match(r'^(?:AcceptIf|RejectIf|Source|Story source|Concepts|Requires|SuppressedBy):', ln)
              or re.match(r'^###\s+Card\s+\S+.*\[(?:type|topic|box|objective|minutes|assessment):', ln)]
    checks.append((not leaked, 'lesson bookkeeping lives in cards.json'
                   + (f' — {len(leaked)} FOUND' if leaked else '')))
    if (Path(d) / 'cards.json').is_file():
        checks.append((not _coverage(Path(d, 'MANIFEST.md').read_text(),
                                     {c['id']: c for c in cards}),
                       'card objectives map to numbered module objectives'))

    ok_all = all(ok for ok, _ in checks)
    for ok, msg in checks:
        print(f"  [{'PASS' if ok else 'FAIL'}] {msg}")
    if version == 2:
        ok_all = _v2_checks(d, docs, mtext) and ok_all
    elif version == 1 and not ok_all and not (Path(d) / 'cards.json').is_file():
        print('  [INFO] legacy format: add format-version: 2 for v2 validation')
    print(f"\n{'ALL STRUCTURAL CHECKS PASS' if ok_all else 'STRUCTURAL CHECKS FAILED'}")
    return 0 if ok_all else 1

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("usage: audit.py <tutorial_dir>"); sys.exit(2)
    sys.exit(main(sys.argv[1]))
