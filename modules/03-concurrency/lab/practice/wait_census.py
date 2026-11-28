# wait_census.py - classify futex WAIT and WAKE addresses in the traces that wait_address.sh writes.
# Usage: python3 wait_census.py <folder with int/char .strace and .stderr files>
# Ignores resumed lines for counting, separates plain WAIT from the thread-join WAIT_BITSET, and requires a
# completed WAIT plus a WAKE that found a waiter on the same address; otherwise the parking probe is inconclusive.
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
for kind in ('int', 'char'):
    obj = re.search(r'OBJECT=(0x[0-9a-f]+)', (root/(kind+'.stderr')).read_text())[1]
    calls = []
    pending = {}
    completed_waits = []
    wakes_with_waiter = set()
    for line in (root/(kind+'.strace')).read_text().splitlines():
        m = re.search(r'^(\d+)\s+futex\((0x[0-9a-f]+), (FUTEX_[A-Z_|]+)', line)
        if m:
            tid, addr, op = m.groups()
            calls.append((addr, op))
            if op in ('FUTEX_WAIT', 'FUTEX_WAIT_PRIVATE'):
                if '<unfinished ...>' in line:
                    pending[tid] = addr
                elif re.search(r'= 0\s*$', line):
                    completed_waits.append(addr)
            if op.startswith('FUTEX_WAKE') and re.search(r'= [1-9][0-9]*\s*$', line):
                wakes_with_waiter.add(addr)
        else:
            m = re.search(r'^(\d+)\s+<\.\.\. futex resumed>.*= 0\s*$', line)
            if m and m[1] in pending:
                completed_waits.append(pending.pop(m[1]))
    waits = [(a, op) for a, op in calls if op in ('FUTEX_WAIT', 'FUTEX_WAIT_PRIVATE')]
    parked = set(completed_waits) & wakes_with_waiter
    print(kind, 'object='+obj, 'wait_entries='+str(len(waits)),
          'wait_on_object='+str(sum(a == obj for a, _ in waits)),
          'wake_on_object='+str(sum(a == obj and op.startswith('FUTEX_WAKE') for a, op in calls)),
          'confirmed_wait_addresses='+str(sorted(parked)))
    assert parked, 'INCONCLUSIVE: need a successful WAIT and a WAKE that found a waiter; inspect the trace and repeat'
    assert all((a == obj) == (kind == 'int') for a, _ in waits)
