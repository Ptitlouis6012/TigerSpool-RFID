#!/usr/bin/env python3
"""Every character that can reach the panel must exist in a compiled font.

LVGL draws a glyph it does not have as a blank box, through its placeholder
path, and logs nothing. So nothing fails at build time, nothing fails at run
time, and the first report comes from a user looking at a word with a hole in
it. That is the whole reason this check exists.

Two live instances found the way users would have found them: two brand names in
the TigerTag reference data carried a no-break space, and i18n.cpp's own header
comment claimed Montserrat covers Latin-1 - it does not, and the table below it
had been written without a single diacritic as a result.

The allowed set is not written down here. scripts/font_range.py reads it from
the '-r' option recorded in LVGL's generated font source, which is the only
place that fact is true. When a Latin subset font ships, this check widens by
itself.

SCOPE, deliberately:

  - firmware/src/**, firmware/include/tigertag_db.h - strings that can be drawn.
  - EXCLUDED: net/portal_page.h and webcfg.cpp. Everything they emit is HTML
    rendered by a phone's browser, not by LVGL. Their accented characters are
    correct and must not be stripped; running this check over them would demand
    exactly the wrong fix.
  - EXCLUDED: firmware/lib/ - vendored third-party code.
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cxx_scan  # noqa: E402
import font_range  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parent.parent

EXCLUDED = {
    "firmware/src/net/portal_page.h": "HTML served to a browser, not drawn by LVGL",
    "firmware/src/webcfg.cpp": "emits HTML for the setup portal and the legacy page",
}

LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')




def sources():
    yield from sorted((REPO / "firmware/src").rglob("*.cpp"))
    yield from sorted((REPO / "firmware/src").rglob("*.h"))
    yield REPO / "firmware/include/tigertag_db.h"


def main() -> int:
    try:
        allowed, spec = font_range.compiled_range(REPO)
    except font_range.FontRangeUnavailable as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    problems = []
    scanned = 0

    for path in sources():
        rel = path.relative_to(REPO).as_posix()
        if rel in EXCLUDED or "/lib/" in f"/{rel}":
            continue
        if not path.exists():
            continue
        scanned += 1
        text = path.read_text(encoding="utf-8", errors="replace")
        for kind, lineno, literal in cxx_scan.scan(text):
            if kind == "string":
                for _, cp in font_range.offending(literal, allowed):
                    problems.append(
                        f"{rel}:{lineno}: {font_range.describe(cp)} in a string "
                        f"literal - the compiled font covers {spec}, so this "
                        "draws as a blank box and logs nothing")

    if scanned == 0:
        print("error: scanned no source files - this check is checking nothing",
              file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"scanned {scanned} files against font range {spec}, "
          f"{len(problems)} violations "
          f"({len(EXCLUDED)} files excluded as browser-rendered)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
