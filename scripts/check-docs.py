#!/usr/bin/env python3
"""Documentation hygiene: internal notes stay unpublished, links resolve.

Both of these lived inside the build workflow, which meant the only way to run
them was to push. They are here so the bench and CI run the same code.

  - _internal/ is French working material for the maintainer - the audit, the
    roadmap, the open questions. It is gitignored, and a force-add would
    publish it. This fails if git is tracking any of it.

  - A relative link that points at nothing is a document that stops being read.
    Only files git tracks are checked: walking the tree also walks .pio/, full
    of vendored libraries whose own READMEs point at images they never shipped,
    and a check that fails on somebody else's documentation is one people learn
    to ignore.

    llms.txt is checked alongside the Markdown. It is the map an agent reads
    first, so a link in it that goes nowhere sends the reader off in the wrong
    direction before they have opened anything - the worst place in the
    repository for a broken link.
"""

import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent


def tracked(pattern):
    return subprocess.run(["git", "ls-files", pattern], cwd=REPO,
                          capture_output=True, text=True, check=True).stdout.split()


def main() -> int:
    problems = []

    if tracked("_internal/*"):
        problems.append("_internal/ is tracked by git. It is maintainer working "
                        "material and must stay ignored: git rm -r --cached _internal")

    docs = [d for d in tracked("*.md") + tracked("llms.txt")
            if not d.startswith("_internal/")]
    if not docs:
        print("error: found no documentation to check", file=sys.stderr)
        return 2

    for name in docs:
        path = REPO / name
        for lineno, line in enumerate(path.read_text().splitlines(), 1):
            for link in re.findall(r"\]\(([^)\s]+)\)", line):
                if link.startswith(("http://", "https://", "mailto:", "#")):
                    continue
                target = link.split("#")[0]
                if target and not (path.parent / target).exists():
                    problems.append(f"{name}:{lineno}: broken link -> {link}")

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"scanned {len(docs)} documents, {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
