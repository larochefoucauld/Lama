#!/usr/bin/env python3
"""Run the regression/vm/*.t programs on a VM and compare with the recorded output.
Programs that the compiler rejects in -b mode are reported as skipped.

Usage: run-tests.py [-v] [-j N] [--vm PATH] [test-name ...]
  --vm   VM binary (default: .build/cmake/release/lamai)
  -v     show a diff for every failed test
"""
import argparse, difflib, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
D = ROOT / ".build/dune/default"
OUT = ROOT / ".build/tests/run"

ap = argparse.ArgumentParser()
ap.add_argument("-v", "--verbose", action="store_true")
ap.add_argument("-j", type=int, default=os.cpu_count())
ap.add_argument("--vm", default=str(ROOT / ".build/cmake/release/lamai"))
ap.add_argument("names", nargs="*", help="test names, e.g. test001 (default: all)")
args = ap.parse_args()
vm = str(Path(args.vm).resolve())
names = args.names or sorted(t.stem for t in (ROOT / "regression/vm").glob("*.t"))
unknown = [n for n in names if not (ROOT / "regression/vm" / f"{n}.t").exists()]
if unknown:
    sys.exit(f"run-tests.py: no such test: {' '.join(unknown)}")
OUT.mkdir(parents=True, exist_ok=True)


def expected_output(name):
    """Output lines of the .t file"""
    lines = (ROOT / "regression/vm" / f"{name}.t").read_text().splitlines()
    return "".join(l[2:] + "\n" for l in lines if l.startswith("  ") and not l.startswith("  $ "))


def run(name):
    c = subprocess.run(
        [str(D / "src/Driver.exe"), "-runtime", str(D / "runtime"), "-I", str(D / "stdlib/x64"), "-b",
         str(ROOT / "regression" / f"{name}.lama")],
        cwd=OUT, capture_output=True, text=True)
    if c.returncode != 0:
        return "skip", name, ""
    try:
        with open(ROOT / "regression" / f"{name}.input") as stdin:
            r = subprocess.run([vm, f"{name}.bc"], cwd=OUT, stdin=stdin, capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return "fail", name, "timeout\n"
    got, want = r.stdout + r.stderr, expected_output(name)
    if got == want:
        return "pass", name, ""
    diff = "".join(list(difflib.unified_diff(want.splitlines(True), got.splitlines(True), "expected", "actual"))[:12])
    return "fail", name, diff


with ThreadPoolExecutor(args.j) as ex:
    results = list(ex.map(run, names))
count = {k: sum(r[0] == k for r in results) for k in ("pass", "fail", "skip")}
failed = [r for r in results if r[0] == "fail"]
if args.verbose:
    for _, name, diff in failed:
        print(f"--- {name}\n{diff}", end="")
print(f"passed: {count['pass']}, failed: {count['fail']}, skipped (compile error): {count['skip']}")
if failed:
    print("failed:", " ".join(r[1] for r in failed))
sys.exit(1 if failed else 0)
