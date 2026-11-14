#!/usr/bin/env python3
"""Run regression_long/{expressions,deep-expressions} programs on a VM.

Usage:
  run-long.py [-j N] [--limit N] [--vm PATH] [expressions|deep-expressions ...]
Programs are compiled with Driver -b into .build/tests/long/<dir>/.
"""
import argparse, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
D = ROOT / ".build/dune/default"
ap = argparse.ArgumentParser()
ap.add_argument("-j", type=int, default=os.cpu_count())
ap.add_argument("--limit", type=int, default=0)
ap.add_argument("--vm", default=str(ROOT / ".build/cmake/release/lamai"))
ap.add_argument("dirs", nargs="*", default=["expressions", "deep-expressions"])
args = ap.parse_args()
args.vm = str(Path(args.vm).resolve())

def cases(d):
    """Parse (name, expected output) pairs from the .t files"""
    for t in sorted((ROOT / "regression_long" / d).glob("*.t")):
        name, exp = None, []
        for line in t.read_text().splitlines() + ["  $ END"]:
            m = re.match(r"  \$ .* -i (\S+)\.lama < ", line)
            if line.startswith("  $ "):
                if name is not None:
                    yield name, "\n".join(exp) + "\n"
                name, exp = (m.group(1) if m else None), []
            elif name is not None and line.startswith("  "):
                exp.append(line[2:])

def run(d, name, expected):
    src = ROOT / "regression_long" / d
    out = ROOT / ".build/tests/long" / d
    out.mkdir(parents=True, exist_ok=True)
    c = subprocess.run([str(D / "src/Driver.exe"), "-runtime", str(D / "runtime"), "-I", str(D / "stdlib/x64"),
                        "-b", str(src / f"{name}.lama")], cwd=out, capture_output=True, text=True)
    if c.returncode != 0:
        return "skip", name, c.stderr[:200]
    try:
        r = subprocess.run([args.vm, f"{name}.bc"], cwd=out, stdin=open(src / f"{name}.input"),
                           capture_output=True, text=True, timeout=30)
        got = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        return "fail", name, "timeout"
    return ("pass", name, "") if got == expected else ("fail", name, f"expected {expected!r} got {got[:200]!r}")

ok = True
for d in args.dirs:
    todo = list(cases(d))
    if args.limit:
        todo = todo[: args.limit]
    with ThreadPoolExecutor(args.j) as ex:
        res = list(ex.map(lambda c: run(d, *c), todo))
    n = {k: sum(r[0] == k for r in res) for k in ("pass", "fail", "skip")}
    print(f"{d}: {len(res)} programs, passed {n['pass']}, failed {n['fail']}, skipped (compile error) {n['skip']}")
    for r in [r for r in res if r[0] == "fail"][:10]:
        print("  FAIL", r[1], r[2])
    ok &= n["fail"] == 0
sys.exit(0 if ok else 1)
