#!/usr/bin/env python3
"""Universal launcher for the Modern C++ Durable-Learning Tutorial.

Auto-detects the best available tier and launches the tutorial:
  T0 — Paper mode (just prints lesson paths)
  T1 — Deterministic driver (no model, no network)
  T2/T3 — Suggests launching with agy or claude code

Usage:
    python3 launch.py                     # interactive, auto-detect
    python3 launch.py --module 01         # specific module
    python3 launch.py --tier T1           # force tier
    python3 launch.py --list              # list all modules
    python3 launch.py --status            # show progress across modules
"""
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from driver_lib import tutorial_format

ROOT = os.path.dirname(os.path.abspath(__file__))
MODULES_DIR = os.path.join(ROOT, 'modules')
DRIVE_PY = os.path.join(ROOT, 'drive.py')


def card_states(progress):
    """Driver session metadata is not a learner card."""
    return {cid: state for cid, state in progress.items() if cid != '_runtime'}


def discover_modules():
    """Find all module directories under modules/."""
    if not os.path.isdir(MODULES_DIR):
        return []
    modules = []
    for name in sorted(os.listdir(MODULES_DIR)):
        mod_path = os.path.join(MODULES_DIR, name)
        lessons_path = os.path.join(mod_path, 'lessons')
        if os.path.isdir(lessons_path):
            manifest_path = os.path.join(mod_path, 'MANIFEST.md')
            title = name
            if os.path.exists(manifest_path):
                with open(manifest_path) as f:
                    first_line = f.readline().strip().lstrip('#').strip()
                    if first_line:
                        title = first_line
            modules.append({
                'id': name.split('-')[0] if '-' in name else name,
                'dir_name': name,
                'path': mod_path,
                'lessons_path': lessons_path,
                'title': title,
            })
    return modules


def detect_tier():
    """Auto-detect the best available tier."""
    # Check if we're running inside an AI agent context
    if os.environ.get('CLAUDE_CODE') or os.environ.get('AGY_SESSION'):
        return 'T3'

    # Check if drive.py exists and works
    if os.path.exists(DRIVE_PY):
        return 'T1'

    return 'T0'


def show_modules(modules):
    """Display available modules."""
    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║    Modern C++ for High-Performance Systems                  ║")
    print("║    Durable-Learning Tutorial                                ║")
    print("╚══════════════════════════════════════════════════════════════╝\n")

    if not modules:
        print("  No modules found. Check that modules/ directory exists.\n")
        return

    print("  Available modules:\n")
    for m in modules:
        progress_path = os.path.join(m['path'], 'progress.json')
        status = '○'  # not started
        if os.path.exists(progress_path):
            with open(progress_path) as f:
                progress = card_states(json.load(f))
            if progress:
                status = '◐'  # in progress
                recurring = [c['id'] for d in tutorial_format.parse_lessons(m['lessons_path'])
                             for c in d['cards'] if not c['once']]
                if recurring and all(cid in progress and progress[cid].get('box', 1) >= 3
                                     for cid in recurring):
                    status = '●'

        print(f"    {status}  [{m['id']}] {m['title']}")

    print("\n  Legend: ○ = not started, ◐ = in progress, ● = recurring cards at box ≥3")
    print(f"  Run: python3 launch.py --module <id>\n")


def show_status(modules):
    """Show detailed progress status across all modules."""
    print("\n  Progress Report")
    print("  " + "═" * 58 + "\n")

    total_cards = 0
    total_seen = 0
    total_mastered = 0

    for m in modules:
        progress_path = os.path.join(m['path'], 'progress.json')
        if not os.path.exists(progress_path):
            print(f"  [{m['id']}] {m['title']}")
            print(f"        Not started\n")
            continue

        with open(progress_path) as f:
            progress = card_states(json.load(f))

        seen = len(progress)
        mastered = sum(1 for s in progress.values() if s.get('box', 1) >= 3)
        next_dates = [s['next'] for s in progress.values()
                      if s.get('next', '9999') != '9999-12-31']
        next_due = min(next_dates) if next_dates else 'n/a'

        total_seen += seen
        total_mastered += mastered

        print(f"  [{m['id']}] {m['title']}")
        print(f"        Cards seen: {seen}, recurring cards at box ≥3: {mastered}, "
              f"Next due: {next_due}\n")

    if total_seen > 0:
        print(f"  ─────────────────────────────────────────────────────────")
        print(f"  Total: {total_seen} cards practiced, {total_mastered} recurring cards at box ≥3")
    print()


def launch_t0(module):
    """T0: Just tell the learner which files to open."""
    print(f"\n  T0 — Paper Mode")
    print(f"  Open these files and self-quiz against the Key: bullets:\n")
    lessons_path = module['lessons_path']
    for f in sorted(os.listdir(lessons_path)):
        if f.endswith('.md'):
            print(f"    {os.path.join(lessons_path, f)}")
    print("\n  Cover each key, answer from memory, then check. Record card ID,")
    print("  result, box, and next date. Study unseen and due cards only.\n")


def write_paper(modules):
    """Generate questions and keys separately so paper study stays usable."""
    target = Path(ROOT) / 'paper'
    target.mkdir(exist_ok=True)
    for module in modules:
        docs = tutorial_format.parse_lessons(module['lessons_path'])
        questions = [f"# Module {module['id']} — questions\n"]
        answers = [f"# Module {module['id']} — answers\n"]
        for doc in docs:
            title = doc['text'].splitlines()[0].lstrip('# ').strip()
            # Paper questions contain the chapter, including stories and examples.
            # Folded answers and worked results stay in the separate answer file.
            chapter = re.sub(r'<details(?:\s[^>]*)?>.*?</details>', '',
                             doc['text'], flags=re.S)
            chapter = re.sub(r'(?m)^StretchKey:.*(?:\n  .*|\n\s*- .*)*', '', chapter)
            chapter = re.sub(r'\n{3,}', '\n\n', chapter)
            questions.append(chapter.strip() + '\n')
            answers.append(f"## {title}\n")
            for block in doc['blocks']:
                if block['kind'] == 'concept':
                    for match in re.finditer(r'<details(?:\s[^>]*)?>\s*<summary>(.*?)</summary>(.*?)</details>',
                                             block['raw'], flags=re.S):
                        answers.append(f"### Concept {block['id']} · {match.group(1).strip()}\n")
                        answers.append(match.group(2).strip() + '\n')
                if block['kind'] != 'card':
                    continue
                card = next(c for c in doc['cards'] if c['id'] == block['id'])
                answers.append(f"### {card['id']}\n")
                if card.get('Hint'):
                    answers.append(f"Hint (request after trying): {card['Hint'].strip()}\n")
                answers.extend(f"- {item}" for item in card['Key'])
                if card.get('Stretch'):
                    answers.append(f"Optional extension: {card['Stretch']}")
                    answers.extend(f"- {item}" for item in card['StretchKey'])
                answers.append('')
        (target / f"{module['id']}-questions.md").write_text('\n'.join(questions))
        (target / f"{module['id']}-answers.md").write_text('\n'.join(answers))
    (target / 'review-calendar.md').write_text(
        '# Paper review ledger\n\n'
        'Record one row per attempt: card ID, date, result, box, next date. '
        'Ask each pretest once. A hit advances one box (maximum 4); after a hit '
        'in box 2, 3, or 4, return in 2, 7, or 21 days. A miss resets to box 1 '
        'and a partial keeps its box; both return tomorrow. On each study date, '
        'answer only unseen or due cards.\n\n'
        '| Card ID | Date | Result | Box | Next date |\n'
        '|---|---|---|---:|---|\n'
        '|  |  |  |  |  |\n')
    print(f'Paper questions, answers, and review calendar written to {target.relative_to(ROOT)}/.')


def launch_t1(module, today):
    """T1: Run the deterministic driver."""
    progress_path = os.path.join(module['path'], 'progress.json')

    cmd = [
        sys.executable, DRIVE_PY,
        module['lessons_path'],
        '--progress', progress_path,
        '--today', today,
    ]

    print(f"\n  T1 — Deterministic Driver (no model, no network)")
    print(f"  Module: {module['title']}")
    print(f"  Date: {today}")
    print(f"  Progress: {progress_path}\n")

    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        print("\n\n  Session interrupted — progress saved. Return another day!\n")


def launch_ai_suggestion(module):
    """T2/T3: Suggest how to launch with an AI model."""
    print(f"\n  T2/T3 — AI-Assisted Mode")
    print(f"  Module: {module['title']}\n")
    print(f"  Option 1 — Antigravity (agy):")
    print(f'    agy "Follow PROTOCOL.md to tutor me on {module["title"]}"')
    print(f"\n  Option 2 — Claude Code:")
    print(f'    claude "Follow PROTOCOL.md to run a study session on '
          f'{module["title"]}. Lessons are in {module["lessons_path"]}"')
    print(f"\n  The AI will grade your free-text answers against the pre-authored")
    print(f"  Key: bullets, give Socratic hints, and adapt difficulty.\n")
    print(f"  Or fall back to T1 (no model):")
    print(f"    python3 launch.py --module {module['id']} --tier T1\n")


def pick_next_module(modules, today):
    """Start-or-resume policy: modules in prerequisite order; the first one
    with anything to do today wins — unseen cards (first pass not finished)
    or scheduled reviews that have come due."""
    sys.path.insert(0, ROOT)
    import drive
    for m in modules:
        progress_path = os.path.join(m['path'], 'progress.json')
        progress = {}
        runtime = {}
        if os.path.exists(progress_path):
            with open(progress_path) as f:
                raw_progress = json.load(f)
            runtime = raw_progress.get('_runtime', {})
            progress = card_states(raw_progress)
        if (runtime.get('pending_self_grade') or runtime.get('pending_answer') or
                runtime.get('pending_retry') or
                runtime.get('pending_stretch')):
            return m, 'unfinished interaction'
        ids = {c['id'] for c in drive.parse_cards(m['lessons_path'])}
        unseen = ids - progress.keys()
        due = [s['next'] for s in progress.values()
               if s.get('next', '9999') != '9999-12-31' and s['next'] <= today]
        if unseen and not progress:
            return m, 'first unstarted module'
        if unseen:
            return m, f'{len(unseen)} card(s) left in first pass'
        if due:
            return m, f'{len(due)} review(s) due'
    return None, None


def resume(modules, today, tier):
    """One-command start/resume: pick the right module and launch it."""
    module, why = pick_next_module(modules, today)
    if module is None:
        # Everything started, nothing due — tell the learner when to return.
        upcoming = []
        for m in modules:
            with open(os.path.join(m['path'], 'progress.json')) as f:
                progress = card_states(json.load(f))
            upcoming += [s['next'] for s in progress.values()
                         if s.get('next', '9999') != '9999-12-31']
        nxt = min((d for d in upcoming if d > today), default=None)
        print("\n  Nothing due today — that's the spacing working, not a bug.")
        if nxt:
            print(f"  Next review due: {nxt}. Return on or after that day.\n")
        else:
            print("  No future reviews scheduled.\n")
        return
    print(f"\n  Resuming: [{module['id']}] {module['title']}  ({why})")
    if tier == 'T0':
        launch_t0(module)
    else:
        launch_t1(module, today)


def wipe_progress(modules, target, yes):
    """Delete stored progress (progress.json) for one module or all."""
    victims = []
    for m in modules:
        if target != 'all' and m['id'] != target and not m['dir_name'].startswith(target):
            continue
        p = os.path.join(m['path'], 'progress.json')
        if os.path.exists(p):
            with open(p) as f:
                n = len(card_states(json.load(f)))
            victims.append((m, p, n))
    if not victims:
        print(f"\n  Nothing to wipe — no progress.json found for "
              f"'{target}'. Already at a clean start.\n")
        return
    print("\n  This will permanently delete:")
    for m, p, n in victims:
        print(f"    [{m['id']}] {m['title']} — {n} card(s) of progress ({p})")
    if not yes:
        try:
            confirm = input("\n  Type 'wipe' to confirm: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\n  Aborted — nothing deleted.\n")
            return
        if confirm != 'wipe':
            print("  Aborted — nothing deleted.\n")
            return
    for _, p, _ in victims:
        os.remove(p)
    print(f"\n  Wiped {len(victims)} module(s). The tutorial will start from "
          f"the beginning (pretests included).\n")


def select_module_interactive(modules):
    """Let the learner pick a module interactively."""
    show_modules(modules)

    # Find the first unstarted or in-progress module
    suggested = None
    for m in modules:
        progress_path = os.path.join(m['path'], 'progress.json')
        if not os.path.exists(progress_path):
            suggested = m
            break
        with open(progress_path) as f:
            progress = card_states(json.load(f))
        next_dates = [s['next'] for s in progress.values()
                      if s.get('next', '9999') != '9999-12-31']
        today = datetime.date.today().isoformat()
        if any(d <= today for d in next_dates):
            suggested = m
            break

    if suggested:
        print(f"  Suggested: [{suggested['id']}] {suggested['title']}")

    try:
        choice = input("\n  Enter module ID (or 'q' to quit): ").strip()
    except (EOFError, KeyboardInterrupt):
        print()
        return None

    if choice.lower() == 'q':
        return None

    for m in modules:
        if m['id'] == choice or m['dir_name'] == choice:
            return m

    print(f"\n  Module '{choice}' not found. Available: "
          f"{', '.join(m['id'] for m in modules)}\n")
    return None


def main():
    ap = argparse.ArgumentParser(
        description='Modern C++ Tutorial Launcher',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 launch.py --resume            # start or resume (./start.sh)
  python3 launch.py                     # interactive menu
  python3 launch.py --module 01         # start module 01
  python3 launch.py --module 01 --tier T1  # force offline mode
  python3 launch.py --list              # list all modules
  python3 launch.py --status            # show learning progress
  python3 launch.py --wipe [01]         # wipe progress: one module or all (./wipe.sh)
          """)
    ap.add_argument('--module', help='Module ID to launch (e.g. 01)')
    ap.add_argument('--tier', choices=['T0', 'T1', 'T2', 'T3'],
                    help='Force a specific tier')
    ap.add_argument('--today', default=datetime.date.today().isoformat(),
                    help='Date for determinism (YYYY-MM-DD)')
    ap.add_argument('--list', action='store_true', help='List available modules')
    ap.add_argument('--status', action='store_true', help='Show progress status')
    ap.add_argument('--paper', action='store_true', help='Generate separate questions and answers for paper study')
    ap.add_argument('--exam', action='store_true', help='Run a delayed mixed offline assessment of 12 studied cards')
    ap.add_argument('--exam-modules', help='Comma-separated module IDs for --exam (default: all studied modules)')
    ap.add_argument('--exam-progress', default='progress-exam.json',
                    help='Where to save and resume the offline exam')
    ap.add_argument('--resume', action='store_true',
                    help='Start or resume: auto-pick the module with due reviews '
                         '(or the first unstarted one) and launch the T1 driver')
    ap.add_argument('--wipe', nargs='?', const='all', metavar='MODULE',
                    help="Wipe stored progress for MODULE (e.g. 01), or all "
                         "modules if no argument — restarts the tutorial from zero")
    ap.add_argument('--yes', action='store_true',
                    help='Skip the confirmation prompt for --wipe')
    args = ap.parse_args()

    modules = discover_modules()

    if args.wipe is not None:
        wipe_progress(modules, args.wipe, args.yes)
        return

    if args.list:
        show_modules(modules)
        return

    if args.status:
        show_status(modules)
        return

    if args.paper:
        write_paper(modules)
        return

    if args.exam:
        from driver_lib import mixed_exam
        selected = (modules if not args.exam_modules else
                    [m for m in modules if m['id'] in args.exam_modules.split(',')])
        if not selected:
            ap.error('no modules selected for --exam')
        mixed_exam.run(selected, args.today, args.exam_progress)
        return

    if args.resume:
        # --resume always runs a real session: T1 unless T0 explicitly forced.
        resume(modules, args.today, 'T0' if args.tier == 'T0' else 'T1')
        return

    if not modules:
        print("\n  No modules found. Ensure modules/ directory has lesson content.")
        print("  See README.md for setup instructions.\n")
        sys.exit(1)

    # Select module
    module = None
    if args.module:
        for m in modules:
            if m['id'] == args.module or m['dir_name'].startswith(args.module):
                module = m
                break
        if not module:
            print(f"\n  Module '{args.module}' not found.")
            show_modules(modules)
            sys.exit(1)
    else:
        module = select_module_interactive(modules)
        if module is None:
            return

    # Detect or force tier
    tier = args.tier or detect_tier()

    if tier == 'T0':
        launch_t0(module)
    elif tier == 'T1':
        launch_t1(module, args.today)
    else:
        launch_ai_suggestion(module)


if __name__ == '__main__':
    main()
