#!/usr/bin/env python3
"""Run one authored shell check and show stable machine and human feedback."""
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

# Ordered completion answers. A whole-line keyword list cannot pass a swapped slot.
SLOTS = {
    "m1-c1": [r"&&", r"noexcept", r"(?:std::)?move", r"(?:std::)?move"],
    "m1-c3": [r"int", r"double", r"const\s+char\s*\*",
              r"const\s+(?:std::)?vector\s*<\s*int\s*>\s*&"],
    "m1-c5": [r"class|struct", r"(?:std::)?(?:u?int8_t|(?:unsigned |signed )?char)"],
    "m1-c6": [r"(?:std::chrono::)?steady_clock"],
    "m2-c1": [r"concept", r"requires", r"double"],
    "m2-c2": [r"consteval"],
    "m2-c3": [r"args", r"if constexpr", r"T"],
    "m2-c4": [r"Message", r"MarketDataMessage", r"constrained"],
    "m3-c1": [r"jthread", r"stop_token", r"stop_requested"],
    "m3-c2": [r"release", r"acquire"],
    "m3-c3": [r"atomic_ref"],
    "m3-lpa6": [r"(?:std::)?(?:memory_order_)?acquire"],
    "m3-c4": [r"promise_type", r"(?:std::)?coroutine_handle", r"awaitable|awaiter"],
    "m3-c5": [r"generator", r"co_yield"],
    "m4-c1": [r"filter", r"transform"],
    "m4-c2": [r"optional", r"nullopt", r"idx"],
    "m4-c3": [r"visit", r"v"],
    "m4-c4": [r"ranges", r"sort"],
    "m4-c5": [r"string_view", r"string_view"],
    "m4-c6": [r"span", r"byte"],
    "m5-c1": [r"(?:std::pmr::)?monotonic_buffer_resource", r"(?:std::pmr::)?string", r"resource"],
    "m5-c2": [r"alignas", r"alignas"],
    "m5-c3": [r"consteval", r"\[\[unlikely\]\]"],
    "m5-c5": [r"(?:std::)?(?:chrono|chrono_literals|literals|literals::chrono_literals)"],
    "m5-c4": [r"execution", r"execution", r"par|par_unseq"],
    "m5-c6": [r"next", r"free_", r"free_"],
    "m6-c1": [r"(?:std::memory_order_)?release", r"(?:std::memory_order_)?acquire"],
    "m6-c2": [r"array", r"SpscRing"],
    "m6-c4": [r"__builtin_prefetch", r"0", r"0"],
    "m7-c1": [r"export", r"export", r"import"],
    "m7-c2": [r"target_compile_features", r"target_link_libraries", r"PRIVATE|PUBLIC|INTERFACE"],
    "m7-c3": [r"sanitize", r"address"],
    "m7-c4": [r"DoNotOptimize"],
    "m7-lpa6": [r"9999", r"9999"],
}

def slots(answer, name):
    expected = SLOTS.get(name)
    if not expected:
        return []
    if name == 'm7-c3' and re.fullmatch(r'\s*-fsanitize=address\s*', answer):
        return []
    parts = [x.strip() for x in answer.strip().split(',')]
    if name in ('m2-c2', 'm7-c4'):
        parts = [x.strip('`') for x in parts]
    if name == 'm5-c6' and len(parts) == 1:
        parts = answer.strip().split()
    if name == 'm6-c4':
        # A full call is convenient, but its address must be the authored one.
        call = re.fullmatch(r'\s*__builtin_prefetch\s*\(\s*&buckets\[h\]\s*,\s*([01])\s*,\s*([0-3])\s*\)\s*;?\s*', answer)
        if call:
            parts = ['__builtin_prefetch', *call.groups()]
    errors = []
    for i, pattern in enumerate(expected):
        actual = parts[i] if i < len(parts) else "(missing)"
        if not re.fullmatch(pattern, actual):
            if name == 'm1-c5' and i == 1:
                errors.append(f"blank 2: expected a 1-byte underlying type; got {actual} (int is not 1 byte)")
            elif name == 'm5-c3' and i == 1:
                errors.append(f"blank 2: write the attribute with brackets: [[unlikely]]; got {actual}")
            else:
                label = pattern.replace('(?:std::)?', '').replace('(?:std::pmr::)?', '')
                label = label.replace('(?:memory_order_)?', '').replace('\\s+', ' ')
                label = label.replace('\\[', '[').replace('\\]', ']')
                label = label.replace('(?:', '').replace(')?', '').replace('|', ' or ')
                if name == 'm6-c2':
                    label = ('a fixed size array' if i == 0 else 'a lock free queue or ring')
                errors.append(f"blank {i + 1}: expected {label}; got {actual}")
    if len(parts) > len(expected):
        errors.append(f"expected {len(expected)} blanks; got {len(parts)} values")
    return errors

def clean(line):
    line = re.sub(r"\x1b\[[0-9;]*m", "", line)
    line = re.sub(r"^\s*(?:at |#\d+\s+).*(?:\.java:|\.cpp:).*", "", line)
    return line.strip()

def preflight(name, impl=None):
    """Check the real feature with a known-good answer before grading."""
    if name in ('m1-c3', 'm2-c4', 'm3-c4', 'm3-c5'):
        return None  # Terminology checks do not invoke the compiler.
    if name in ('m1-c1', 'm7-c3') and impl is not None:
        problem = preflight(name)
        if problem:
            return problem
    if impl is not None:
        cases = json.loads((Path(__file__).with_name('check_cases.json')).read_text())
        answer = cases[name][0] if name in cases else None
        if name == 'pool_lab':
            answer = 'modules/05-memory-perf/lab/solution/pool_lab.cpp'
        elif name == 'spsc_lab':
            answer = 'modules/06-hft-patterns/lab/spsc/solution'
        if answer is not None:
            try:
                result = subprocess.run(['bash', str(impl)], input=answer, text=True,
                                        capture_output=True, timeout=90)
            except (OSError, subprocess.TimeoutExpired) as exc:
                return f'{type(exc).__name__}: {exc}'
            if result.returncode:
                detail = result.stdout.strip() or result.stderr.strip()
                return 'Known-good specimen failed: ' + (detail or 'check exited without a message')
            return None
    flags = ["-std=c++20", "-Wall", "-Wextra"]
    if name == "spsc_lab":
        flags.append("-fsanitize=thread")
    elif name == "m7-c3":
        flags.append("-fsanitize=address")
    try:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "probe.cpp"
            binary = Path(directory) / "probe"
            source.write_text("#include <utility>\n#include <vector>\n"
                              "int main() { std::vector<int> a{1}; auto b = std::move(a);"
                              " return b.size() == 1 ? 0 : 1; }\n")
            result = subprocess.run(["clang++", *flags, str(source), "-o", str(binary)],
                                    capture_output=True, text=True, timeout=30)
            if result.returncode:
                return result.stderr.strip() or "compiler probe failed"
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            if result.returncode:
                return result.stderr.strip() or "compiler runtime probe failed"
    except (OSError, subprocess.TimeoutExpired) as exc:
        return f"{type(exc).__name__}: {exc}"
    return None

def main():
    impl = Path(sys.argv[1])
    answer = sys.stdin.read()
    name = impl.name.split(".")[0]
    failures = slots(answer, name)
    if re.search(r'\bI deny every claim\b', answer, re.I):
        failures.append('The answer explicitly denies the required claims; compare it with the key.')
    if failures:
        status, detail = "MISS", failures
    else:
        try:
            problem = preflight(name, impl)
            if problem:
                status, detail = "SETUP PROBLEM", [problem]
                raise RuntimeError("preflight failed")
            call = re.fullmatch(r'\s*__builtin_prefetch\s*\(\s*&buckets\[h\]\s*,\s*([01])\s*,\s*([0-3])\s*\)\s*;?\s*', answer) if name == 'm6-c4' else None
            checked_answer = ', '.join(['__builtin_prefetch', *call.groups()]) if call else answer
            if name in ('m2-c2', 'm3-c3', 'm7-c4'):
                checked_answer = answer.strip().strip('"`')
            result = subprocess.run(["bash", str(impl)], input=checked_answer, text=True,
                                    capture_output=True, timeout=40)
            lines = [clean(x) for x in result.stdout.splitlines() + result.stderr.splitlines()]
            lines = [x for x in lines if x]
            status = "HIT" if result.returncode == 0 else "MISS"
            if result.returncode in (2, 126, 127) or any(
                    "command not found" in x or "not found on PATH" in x for x in lines):
                status = "SETUP PROBLEM"
            detail = [re.sub(r"^(?:HIT|MISS|SETUP PROBLEM):?\s*", "", x) for x in lines]
            detail = [x for x in detail if x]
            if not detail and status != "HIT":
                detail = ["Check failed without a message; run ./setup.sh, then retry."]
        except RuntimeError:
            pass
        except (OSError, subprocess.TimeoutExpired) as exc:
            status, detail = "SETUP PROBLEM", [f"Check could not finish: {type(exc).__name__}. Run ./setup.sh, then retry."]
    print(json.dumps({"status": status, "criteria": detail}, ensure_ascii=False))
    print(status)
    for line in detail:
        print(line)
    return 0 if status == "HIT" else (2 if status == "SETUP PROBLEM" else 1)

if __name__ == "__main__":
    sys.exit(main())
