"""Lossless lesson reader. Raw lesson text and block bodies remain available to every tier.

Card IDs identify progress globally; concept identities are (lesson-relative path,
concept ID, occurrence). Repeated concept labels inside a lesson are retained, not
silently merged. Legacy Source: CONCEPT Cn resolves only when that label is unique
in the lesson; otherwise consumers must teach the whole lesson before its quiz.
"""
from pathlib import Path
import json
import re

CARD = re.compile(r'^###\s+(?:CARD|Card)\s+(\S+)\s*(.*)$')
CONCEPT = re.compile(r'^###\s+CONCEPT\s+(\S+)\s*(.*)$')
FIELD = re.compile(r'^(Q|Hint|Key|AcceptIf|RejectIf|Stretch|StretchKey|Source|Concepts|Requires|Elaborate|ElaborateKey|HandsOn|HandsOnKey|Figure|FigureText|SuppressedBy):\s*(.*)$')
SINGLE_LINE_FIELDS = {'Requires', 'Concepts', 'Source', 'Figure', 'FigureText',
                      'SuppressedBy', 'AcceptIf', 'RejectIf'}
TAG = re.compile(r'\[([^\]]+)\]')


class FormatError(ValueError):
    pass


def format_version(tutorial_dir):
    """An absent manifest version is the legacy contract (version 1)."""
    manifest = Path(tutorial_dir) / 'MANIFEST.md'
    if not manifest.exists():
        return 1
    for number, line in enumerate(manifest.read_text(encoding='utf-8').splitlines(), 1):
        if line.startswith('format-version:'):
            match = re.fullmatch(r'format-version:\s*(\d+)\s*', line)
            if not match or int(match.group(1)) not in (1, 2):
                raise FormatError(f'{manifest}:{number}: unsupported format-version')
            return int(match.group(1))
    return 1


def _fences(lines):
    """Mark lines inside Markdown fences, including the opening and closing lines."""
    marked = []
    fence = None
    for line in lines:
        match = re.match(r'^\s*(?:-\s*)?(`{3,}|~{3,})', line)
        if match:
            token = match.group(1)
            if fence is None:
                fence = (token[0], len(token))
            elif token[0] == fence[0] and len(token) >= fence[1]:
                marked.append(True)
                fence = None
                continue
        marked.append(fence is not None)
    return marked


def _body_fields(lines, fenced):
    fields = {}
    current = None
    for line, in_fence in zip(lines, fenced):
        # Disclosure wrappers are for paper reading, not part of a field.
        # A tag between Q and Key must not be printed as question text.
        if not in_fence and re.fullmatch(r'\s*(?:<details>|</details>|<summary>.*?</summary>|<details><summary>.*?</summary>)\s*', line):
            continue
        # Indented Q: inside a fenced example is a continuation, never a field.
        match = None if in_fence else FIELD.match(line)
        if match:
            current = match.group(1)
            fields[current] = [match.group(2)] if match.group(2) else []
            if current in SINGLE_LINE_FIELDS:
                current = None
        elif current is not None:
            fields[current].append(line)
    out = {}
    for name, parts in fields.items():
        while parts and not parts[-1].strip():
            parts.pop()
        if name in ('Key', 'StretchKey', 'ElaborateKey', 'HandsOnKey'):
            bullets = []
            for line in parts:
                m = re.match(r'^\s*- (.*)$', line)
                if m:
                    bullets.append(m.group(1))
                elif bullets:
                    bullets[-1] += '\n' + line
            out[name] = bullets
        else:
            out[name] = '\n'.join(parts)
    return out


def parse_lesson(path, relative=None):
    path = Path(path)
    text = path.read_text(encoding='utf-8')
    lines = text.splitlines()
    fenced = _fences(lines)
    starts = []
    for i, line in enumerate(lines):
        if fenced[i]:
            continue
        cm, qm = CONCEPT.match(line), CARD.match(line)
        if cm or qm:
            starts.append((i, 'concept' if cm else 'card', cm or qm))
    cards, concepts, blocks = [], [], []
    seen_cards = {}
    label_counts = {}
    lesson_id = str(relative if relative is not None else path.name)
    cursor = 0
    for j, (start, kind, match) in enumerate(starts):
        if cursor < start:
            blocks.append({'kind': 'prose', 'line': cursor + 1,
                           'raw': '\n'.join(lines[cursor:start])})
        end = starts[j + 1][0] if j + 1 < len(starts) else len(lines)
        # A following level-two section or thematic break ends this block.
        for n in range(start + 1, end):
            if not fenced[n] and (lines[n].startswith('## ') or lines[n].strip() == '---'):
                end = n
                break
        raw = '\n'.join(lines[start:end])
        item = {'id': match.group(1), 'lesson': lesson_id, 'line': start + 1,
                'raw': raw, 'body': '\n'.join(lines[start + 1:end])}
        if kind == 'concept':
            label = item['id']
            label_counts[label] = label_counts.get(label, 0) + 1
            item['identity'] = (lesson_id, label, label_counts[label])
            item['title'] = match.group(2).strip()
            item.update(_body_fields(lines[start + 1:end], fenced[start + 1:end]))
            concepts.append(item)
        else:
            cid = item['id']
            if cid in seen_cards:
                raise FormatError(f'{path}:{start + 1}: duplicate card ID {cid}; first at line {seen_cards[cid]}')
            seen_cards[cid] = start + 1
            tags = match.group(2)
            tag_values = {}
            for tm in TAG.finditer(tags):
                key, sep, value = tm.group(1).partition(':')
                tag_values[key.strip()] = value.strip() if sep else True
            fields = _body_fields(lines[start + 1:end], fenced[start + 1:end])
            if not fields.get('Q', '').strip():
                raise FormatError(f'{path}:{start + 1}: card {cid} missing Q:')
            item.update({'tags': tags, 'type': tag_values.get('type', ''),
                         'topic': tag_values.get('topic', path.stem),
                         'box': int(tag_values.get('box', 1)),
                         'once': bool(tag_values.get('once')) or tag_values.get('type') == 'pretest',
                         'Q': '', 'Hint': None, 'Key': [], 'StretchKey': [],
                         'AcceptIf': None, 'RejectIf': None, 'Stretch': None,
                         'Source': '', 'Concepts': ''})
            item.update(fields)
            cards.append(item)
        blocks.append({'kind': kind, **item})
        cursor = end
    if cursor < len(lines):
        blocks.append({'kind': 'prose', 'line': cursor + 1,
                       'raw': '\n'.join(lines[cursor:])})
    # Preserve all prose between blocks, including hands-on instructions.
    return {'path': path, 'relative': lesson_id, 'text': text, 'blocks': blocks,
            'cards': cards, 'concepts': concepts}


def concept_refs(doc, card):
    """Resolve v2 `Concepts:` tokens to identities in the parsed lesson set.

    Local C1 refers to this lesson. Cross-lesson lesson-02.md#C1 is explicit.
    Duplicated labels within one lesson require #C1@2 to select occurrence 2.
    """
    value = card.get('Concepts', '')
    if not value:
        return []
    refs = []
    for token in re.split(r'[,\s]+', value.strip()):
        if not token:
            continue
        lesson, separator, label = token.rpartition('#')
        if not separator:
            lesson, label = doc['relative'], token
        name, occurrence_sep, occurrence_text = label.partition('@')
        occurrence = int(occurrence_text) if occurrence_sep and occurrence_text.isdigit() else 1
        if not name or (occurrence_sep and not occurrence_text.isdigit()):
            raise FormatError(f"{doc['path']}:{card['line']}: invalid Concepts reference {token}")
        refs.append((lesson, name, occurrence))
    return refs


def resolve_legacy_source(doc, card):
    """Return one lesson-local concept identity, or None for whole-lesson fallback."""
    match = re.fullmatch(r'CONCEPT\s+(\S+)', card.get('Source', '').strip())
    if not match:
        return None
    matches = [c['identity'] for c in doc['concepts'] if c['id'] == match.group(1)]
    return matches[0] if len(matches) == 1 else None


def parse_lessons(lessons_dir):
    root = Path(lessons_dir)
    docs = []
    ids = {}
    for path in sorted(root.glob('*.md')):
        doc = parse_lesson(path, path.relative_to(root))
        for card in doc['cards']:
            if card['id'] in ids:
                raise FormatError(f"{path}:{card['line']}: duplicate card ID {card['id']}; first at {ids[card['id']]}")
            ids[card['id']] = f"{path}:{card['line']}"
        docs.append(doc)
    manifest_path = root.parent / 'cards.json'
    if manifest_path.exists():
        try:
            manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as exc:
            raise FormatError(f'{manifest_path}: invalid JSON: {exc}') from exc
        if not isinstance(manifest, dict) or manifest.get('schema_version') != 1:
            raise FormatError(f'{manifest_path}: unsupported card schema')
        rows = manifest.get('cards')
        if not isinstance(rows, dict):
            raise FormatError(f'{manifest_path}: cards must be an object')
        actual = {card['id'] for doc in docs for card in doc['cards']}
        missing, extra = actual - rows.keys(), rows.keys() - actual
        if missing or extra:
            raise FormatError(f'{manifest_path}: missing card rows {sorted(missing)}; extra card rows {sorted(extra)}')
        lesson_rows = manifest.get('lessons')
        if not isinstance(lesson_rows, dict) or set(lesson_rows) != {d['relative'] for d in docs}:
            raise FormatError(f'{manifest_path}: lesson rows must match lesson files')
        for doc in docs:
            for card in doc['cards']:
                row = rows[card['id']]
                if not isinstance(row, dict) or set(row) != {'lesson', 'type', 'topic', 'objective', 'minutes', 'box', 'once', 'grading', 'source', 'concepts'}:
                    raise FormatError(f'{manifest_path}: malformed row {card["id"]}')
                if (row['lesson'] != doc['relative'] or not isinstance(row['type'], str) or not row['type']
                        or not isinstance(row['topic'], str) or not row['topic']
                        or type(row['objective']) is not int or row['objective'] < 1
                        or type(row['minutes']) is not int or row['minutes'] < 0
                        or type(row['box']) is not int or row['box'] not in (1, 2, 3, 4)
                        or type(row['once']) is not bool or not isinstance(row['source'], str) or not row['source']
                        or not isinstance(row['concepts'], list)
                        or any(not isinstance(x, str) for x in row['concepts'])):
                    raise FormatError(f'{manifest_path}: malformed row {card["id"]}')
                grading = row['grading']
                if not isinstance(grading, dict) or grading.get('mode') not in ('self', 'regex', 'check'):
                    raise FormatError(f'{manifest_path}: malformed grading for {card["id"]}')
                if grading['mode'] == 'self' and set(grading) != {'mode'}:
                    raise FormatError(f'{manifest_path}: malformed grading for {card["id"]}')
                if grading['mode'] != 'self' and (not isinstance(grading.get('accept'), str)
                        or not grading['accept'] or set(grading) - {'mode', 'accept', 'reject'}
                        or ('reject' in grading and not isinstance(grading['reject'], str))):
                    raise FormatError(f'{manifest_path}: malformed grading for {card["id"]}')
                if grading['mode'] == 'check' and not grading['accept'].startswith('check('):
                    raise FormatError(f'{manifest_path}: malformed check for {card["id"]}')
                if grading['mode'] == 'regex' and not grading['accept'].startswith('regex('):
                    raise FormatError(f'{manifest_path}: malformed regex for {card["id"]}')
                identities = {c['identity'] for other in docs for c in other['concepts']}
                if any(ref not in identities for ref in
                       concept_refs(doc, {**card, 'Concepts': ', '.join(row['concepts'])})):
                    raise FormatError(f'{manifest_path}: unknown concept for {card["id"]}')
                for field in ('AcceptIf', 'RejectIf', 'Source', 'Concepts'):
                    if card.get(field):
                        raise FormatError(f'{doc["path"]}:{card["line"]}: {field} belongs in cards.json')
                card.update({'type': row['type'], 'topic': row['topic'], 'objective': row['objective'],
                             'minutes': row['minutes'], 'box': row['box'], 'once': row['once'],
                             'AcceptIf': grading.get('accept'), 'RejectIf': grading.get('reject'),
                             'Source': row['source'], 'Concepts': ', '.join(row['concepts'])})
            for block in doc['blocks']:
                if block['kind'] == 'card':
                    block.update(next(c for c in doc['cards'] if c['id'] == block['id']))
        for name, row in lesson_rows.items():
            if not isinstance(row, dict) or set(row) != {'minutes'} or type(row['minutes']) is not int or row['minutes'] < 1:
                raise FormatError(f'{manifest_path}: malformed lesson row {name}')
            if sum(c['minutes'] for d in docs for c in d['cards'] if d['relative'] == name) != row['minutes']:
                raise FormatError(f'{manifest_path}: card minutes do not total lesson {name}')
            doc = next(d for d in docs if d['relative'] == name)
            stated = re.search(r'Time.{0,15}?about\s*(\d+)\s*min', doc['text'][:1500], re.I)
            if not stated or int(stated.group(1)) != row['minutes']:
                raise FormatError(f'{manifest_path}: lesson minutes disagree with {name}')
        concepts = manifest.get('concepts', {})
        if not isinstance(concepts, dict):
            raise FormatError(f'{manifest_path}: concepts must be an object')
        expected_concepts = {f"{d['relative']}#{c['id']}" for d in docs for c in d['concepts']}
        if set(concepts) != expected_concepts:
            raise FormatError(f'{manifest_path}: concept rows must match lesson concepts')
        for doc in docs:
            for concept in doc['concepts']:
                row = concepts[f"{doc['relative']}#{concept['id']}"]
                if not isinstance(row, dict) or set(row) != {'source'} or not isinstance(row['source'], str):
                    raise FormatError(f'{manifest_path}: malformed concept source {concept["id"]}')
                if concept.get('Source'):
                    raise FormatError(f'{doc["path"]}:{concept["line"]}: Source belongs in cards.json')
                concept['Source'] = row['source']
    elif (root.parent / 'MANIFEST.md').exists():
        raise FormatError(f'{manifest_path}: missing card manifest')
    return docs
