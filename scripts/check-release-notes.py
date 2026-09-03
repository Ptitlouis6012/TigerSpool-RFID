#!/usr/bin/env python3
"""The current version must have release notes, and they must be written.

    python3 scripts/check-release-notes.py             # the bench check
    python3 scripts/check-release-notes.py --released  # the tag-time check

WHY THIS EXISTS. WORKLOG.md is where a change is described the moment it is
done, so that release notes are never written from memory at tag time. But a
file nothing consumes stops being maintained - not this week, reliably by
whoever is here in three months. This is its consumer. It makes the work log
load-bearing instead of a habit, by making the release fail without what the
work log is for.

WHERE THE NOTES LIVE, and why it moves. Before a release the entries accumulate
under `## [Unreleased]`. `scripts/bump-version.sh` moves them into
`## [X.Y.Z] - date` and leaves a scaffold line if there was nothing to move.
So:

  - The bench check accepts either: notes filed under the current version, or
    notes still accumulating under Unreleased. Both mean somebody is writing
    them.
  - The tag-time check accepts only the first. By then the bump has happened,
    and notes still sitting in Unreleased mean the release would publish
    someone else's.

Either way, scaffold text counts as no notes at all. A heading with a
"describe this release" line under it is worse than an empty section, because it
looks finished.
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
CHANGELOG = REPO / "CHANGELOG.md"
VERSION_H = REPO / "firmware" / "include" / "version.h"

# Text a human was supposed to replace. Matched case-insensitively.
SCAFFOLD = [
    "describe this release",
    "write the release notes",
    "todo",
    "tbd",
    "fill this in",
    "<!-- notes -->",
]

SCAFFOLD_LINE = "_Describe this release. See WORKLOG.md — that is what it is for._"


def version() -> str:
    m = re.search(r'#define\s+TIGERSPOOL_FW_VERSION\s+"([^"]+)"', VERSION_H.read_text())
    if not m:
        raise RuntimeError(f"TIGERSPOOL_FW_VERSION not found in {VERSION_H.name}")
    return m.group(1)


def section(text: str, heading: str):
    """The body under a `## [heading]` line, up to the next `## ` heading."""
    m = re.search(rf"^## \[{re.escape(heading)}\][^\n]*$(.*?)(?=^## |\Z)",
                  text, re.S | re.M)
    return m.group(1) if m else None


def substantive(body: str) -> list[str]:
    """Lines that say something: not blank, not a sub-heading, not scaffold."""
    out = []
    for line in body.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if any(s in stripped.lower() for s in SCAFFOLD):
            continue
        out.append(stripped)
    return out


def main() -> int:
    released = "--released" in sys.argv
    v = version()
    text = CHANGELOG.read_text()

    versioned = section(text, v)
    unreleased = section(text, "Unreleased")

    if versioned is None and released:
        print(f"error: CHANGELOG.md has no '## [{v}]' section, and a release "
              "cannot publish notes that do not exist.", file=sys.stderr)
        print("       run: scripts/bump-version.sh " + v, file=sys.stderr)
        return 1

    if versioned is None and unreleased is None:
        print("error: CHANGELOG.md has neither a section for the current "
              f"version ({v}) nor an [Unreleased] section - there is nowhere "
              "for notes to be written.", file=sys.stderr)
        return 1

    body = versioned if versioned is not None else unreleased
    where = f"[{v}]" if versioned is not None else "[Unreleased]"
    lines = substantive(body)

    if not lines:
        print(f"error: CHANGELOG.md {where} has no release notes - only "
              "headings, blank lines, or scaffold text left for someone to "
              "replace.", file=sys.stderr)
        print("       WORKLOG.md is what they are written from; it exists so "
              "they are not written from memory.", file=sys.stderr)
        return 1

    scope = "for the release" if released else "so far"
    print(f"release notes {scope}: {len(lines)} line(s) under {where} "
          f"(version {v})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
