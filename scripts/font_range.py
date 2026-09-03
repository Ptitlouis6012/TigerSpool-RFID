"""Which characters the compiled UI font can actually draw.

One source of truth for a fact that two places need: `firmware/tools/gen_db.py`
validating its input, and `scripts/check-ui-fonts.py` validating every string
that reaches the panel. Writing the range out twice means that the day a Latin
subset font ships, one of them is wrong and nothing says so.

The range is not our choice — it is recorded in the generated font source that
LVGL ships, on the `Opts:` line the font converter writes:

    Opts: ... --font Montserrat-Medium.ttf -r 0x20-0x7F,0xB0,0x2022 ...

So we read it from there rather than restating it. LVGL draws a character it has
no glyph for as a blank box and logs nothing, which is why this needs checking at
all: nothing fails until a user sees it.
"""

import pathlib
import re


class FontRangeUnavailable(RuntimeError):
    """The font source could not be found, so the range is unknown.

    Raised rather than guessed. A checker that assumes a range it did not read
    passes on characters the panel cannot draw, which is the failure it exists
    to prevent.
    """


def _font_sources(repo_root: pathlib.Path):
    """Generated LVGL font sources, newest library checkout first."""
    libdeps = repo_root / "firmware" / ".pio" / "libdeps"
    return sorted(libdeps.glob("*/lvgl/src/font/lv_font_montserrat_*.c"))


def compiled_range(repo_root: pathlib.Path):
    """Return (codepoints, human_description) for the compiled UI font.

    The union across every Montserrat size the build enables: they are generated
    with the same range, and a character missing from one is missing from all.
    """
    sources = _font_sources(repo_root)
    if not sources:
        raise FontRangeUnavailable(
            "no lv_font_montserrat_*.c under firmware/.pio/libdeps — "
            "run 'pio pkg install' in firmware/ so the font source is present"
        )

    allowed: set[int] = set()
    spec = ""
    for src in sources:
        # Only the head: the Opts line is in the file's banner, and these
        # sources are hundreds of kilobytes of glyph data.
        head = src.read_text(errors="replace")[:4096]
        m = re.search(r"-r\s+([0-9A-Fa-fx,\-]+)", head)
        if not m:
            continue
        spec = m.group(1)
        for part in spec.split(","):
            if "-" in part:
                lo, hi = part.split("-", 1)
                allowed.update(range(int(lo, 16), int(hi, 16) + 1))
            else:
                allowed.add(int(part, 16))

    if not allowed:
        raise FontRangeUnavailable(
            f"found {len(sources)} font source(s) but no '-r' range in any "
            "banner — the font converter's option line has changed shape"
        )
    return allowed, spec


def offending(text: str, allowed: set[int]):
    """Characters in `text` the font cannot draw, as (index, codepoint)."""
    return [(i, ord(c)) for i, c in enumerate(text) if ord(c) not in allowed]


def describe(codepoint: int) -> str:
    import unicodedata
    try:
        name = unicodedata.name(chr(codepoint))
    except ValueError:
        name = "unnamed control character"
    return f"U+{codepoint:04X} {name}"
