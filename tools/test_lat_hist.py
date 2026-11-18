"""Shared latency histogram (modules/06-hft-patterns/lab/measure/lat_hist.hpp).

Builds and runs its C++ unit test, then shows that the test catches two
plausible wrong implementations (a floor-rank percentile and a too-small
minimum sample count). Nothing here depends on a measured time.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MEASURE = ROOT / 'modules/06-hft-patterns/lab/measure'


@unittest.skipIf(shutil.which('clang++') is None, 'clang++ is missing')
class LatHistTests(unittest.TestCase):
    def build_and_run(self, header_text):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            (folder / 'lat_hist.hpp').write_text(header_text)
            shutil.copy(MEASURE / 'test_lat_hist.cpp', folder / 'test_lat_hist.cpp')
            binary = folder / 'test_lat_hist'
            build = subprocess.run(['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-O2',
                                    str(folder / 'test_lat_hist.cpp'), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stderr)
            return subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)

    def test_reference_passes(self):
        run = self.build_and_run((MEASURE / 'lat_hist.hpp').read_text())
        self.assertEqual(run.returncode, 0, run.stdout)
        self.assertIn('lat_hist: ALL PASS', run.stdout)

    def test_wrong_implementations_fail(self):
        original = (MEASURE / 'lat_hist.hpp').read_text()
        mutants = {
            'floor rank': ('std::ceil(q * static_cast<double>(n_)',
                           'std::floor(q * static_cast<double>(n_)'),
            'one tail sample': ('kTailSamples = 10;', 'kTailSamples = 1;'),
        }
        for label, (old, new) in mutants.items():
            with self.subTest(mutant=label):
                self.assertIn(old, original)
                run = self.build_and_run(original.replace(old, new))
                self.assertNotEqual(run.returncode, 0, run.stdout)
                self.assertIn('FAILED', run.stdout)

    def test_documented_build_line(self):
        line = (MEASURE / 'test_lat_hist.cpp').read_text().splitlines()[3]
        self.assertTrue(line.startswith('//   clang++ '), line)
        with tempfile.TemporaryDirectory() as tmp:
            run = subprocess.run(line.removeprefix('//   '), shell=True, cwd=ROOT,
                                 env={**os.environ, 'TMPDIR': tmp},
                                 capture_output=True, text=True, timeout=120)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('lat_hist: ALL PASS', run.stdout)


if __name__ == '__main__':
    unittest.main()
