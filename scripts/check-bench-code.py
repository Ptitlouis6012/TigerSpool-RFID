#!/usr/bin/env python3
"""Bench-only code cannot reach a release.

Diagnostics added to chase a bug - payload dumps, counters, logs kept or sent -
belong inside `#if TIGERSPOOL_BENCH ... #endif` (or BENCH_LOG()), which only the
`tigerspool-bench` environment compiles (firmware/src/bench.h). This checks the
two ways that promise can be broken:

- A MARKER outside a bench block. `BENCH:`, `TEMP:`, `TEMPORARY:`,
  `DO NOT SHIP` and `REMOVE BEFORE RELEASE` label code that must not ship; one
  of them in code the production build compiles is exactly that code shipping.
  Upper case with its colon, on purpose: "measured on the bench" is prose.
- The SWITCH turned on for production: TIGERSPOOL_BENCH defined to anything but
  0 in the `tigerspool` environment, or defined in a source file.

It exits 2 when there is no source to scan - a guard reading nothing passes
everything.
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC = [REPO / "firmware" / "src", REPO / "firmware" / "include"]
INI = REPO / "firmware" / "platformio.ini"

MARKER = re.compile(r"\b(BENCH|TEMP|TEMPORARY)\s*:|DO NOT SHIP|REMOVE BEFORE RELEASE")
IF_BENCH = re.compile(r"^\s*#\s*if\s+TIGERSPOOL_BENCH\b")
IF_ANY = re.compile(r"^\s*#\s*if(n?def)?\b")
ELSE = re.compile(r"^\s*#\s*(else|elif)\b")
ENDIF = re.compile(r"^\s*#\s*endif\b")
DEFINE = re.compile(r"^\s*#\s*define\s+TIGERSPOOL_BENCH\s+(\S+)")


def scan(path, problems):
    # A stack of the #if blocks we are in: True where the branch is bench-only.
    stack = []
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if IF_BENCH.match(line):
            stack.append(True)
            continue
        if IF_ANY.match(line):
            stack.append(False)
            continue
        if ELSE.match(line) and stack:
            stack[-1] = False            # the #else of a bench block is production
            continue
        if ENDIF.match(line) and stack:
            stack.pop()
            continue
        d = DEFINE.match(line)
        if d and path.name != "bench.h":
            problems.append(f"{path.relative_to(REPO)}:{n}: TIGERSPOOL_BENCH defined in a source "
                            "file - it is set by the tigerspool-bench environment only")
        # bench.h is where the markers are described, not used.
        if MARKER.search(line) and not any(stack) and path.name != "bench.h":
            problems.append(f"{path.relative_to(REPO)}:{n}: bench marker outside "
                            f"#if TIGERSPOOL_BENCH: {line.strip()[:90]}")


def production_env_flags():
    """The build_flags of [env:tigerspool] - the environment CI releases."""
    text = INI.read_text(encoding="utf-8")
    m = re.search(r"^\[env:tigerspool\]\s*$(.*?)(?=^\[)", text + "\n[", re.S | re.M)
    return m.group(1) if m else None


def main():
    problems = []
    files = [p for d in SRC for p in d.rglob("*") if p.suffix in (".cpp", ".h", ".c")
             and not p.name.startswith("font_ui_")]
    if not files:
        print("no firmware source found - nothing checked")
        return 2
    for f in files:
        scan(f, problems)

    env = production_env_flags()
    if env is None:
        print("[env:tigerspool] not found in platformio.ini - nothing to check it against")
        return 2
    m = re.search(r"-D\s*TIGERSPOOL_BENCH(?:=(\S+))?", env)
    if m and (m.group(1) or "1") != "0":
        problems.append("platformio.ini: [env:tigerspool] turns TIGERSPOOL_BENCH on - "
                        "that is the environment CI releases")

    for p in problems:
        print(p)
    print(f"scanned {len(files)} source files, {len(problems)} violation(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
