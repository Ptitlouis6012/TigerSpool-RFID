#!/usr/bin/env python3
"""The Chinese characters the firmware draws, as one sorted string.

    python3 scripts/cjk-chars.py

One source of truth for that set, read by two places that must agree:
scripts/make-ui-font.sh compiles exactly these glyphs into the UI faces, and
the committed faces then record them - which scripts/gen-font-range.py extracts
and check-ui-fonts.py tests every drawn string against. Collected the same way
the TigerScale does it, so a character only one of the two knew about cannot
reach the panel as a blank box.

The panel draws Chinese from a SUBSET of Noto Sans SC: only the characters our
own translations use, a few hundred glyphs rather than the thousands of a full
face. So the set is re-derived from the source every time the faces are made.

Scanned: every C/C++ source under firmware/src that LVGL draws from - the
translation table, and anything else that writes Chinese directly, such as the
language picker's own name for the language. Not scanned: the pages a phone's
browser renders (portal_page.h, webcfg.cpp), which have the phone's fonts.
"""

import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
BROWSER_ONLY = {"firmware/src/net/portal_page.h", "firmware/src/webcfg.cpp"}


def is_cjk(c: str) -> bool:
    cp = ord(c)
    return (0x3000 <= cp <= 0x303F      # CJK punctuation: 。、
            or 0x4E00 <= cp <= 0x9FFF   # unified ideographs
            or 0xFF00 <= cp <= 0xFFEF)  # full-width forms: ，：？


def collect() -> set:
    chars = set()
    src = REPO / "firmware" / "src"
    for path in sorted(list(src.rglob("*.cpp")) + list(src.rglob("*.h"))):
        if path.relative_to(REPO).as_posix() in BROWSER_ONLY:
            continue
        chars.update(c for c in path.read_text(encoding="utf-8", errors="replace") if is_cjk(c))
    return chars


if __name__ == "__main__":
    chars = collect()
    print(f"{len(chars)} distinct CJK glyphs", file=sys.stderr)
    print("".join(sorted(chars)), end="")
