#!/usr/bin/env python3
"""Documented reader wiring must match the pins the firmware actually opens.

The most expensive mistake in this project's history was GPIO6/7. On this board
those two are an I2C bus with pull-ups, shared with the IMU and the camera
header. A PN532 wired there powers up, enumerates and answers - and returns
random UIDs with failing reads. It looks like a flaky tag or a bad antenna. It
is neither, and finding that out costs a day.

The pins are declared once, in firmware/src/config.h. This reads them from
there and checks that every wiring table in the documentation says the same
thing, so nobody can be sent to the wrong pin by following our own pages.

WHY IT IS SHAPED THIS WAY. Its predecessor searched prose for a pin number
near a signal name on the same line, and so could not tell a wiring instruction
from a sentence warning against one - it failed on a document that was
explaining the check itself. Prose is where you warn about GPIO6/7, and warning
is the correct thing to do there. Tables and pin definitions are where you
prescribe. This looks only at the prescriptions.

It fails when it finds no wiring tables at all: an empty scan means the
documentation moved and this is checking nothing, which must be loud rather
than green.
"""

import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
CONFIG = REPO / "firmware" / "src" / "config.h"

# PN532 signal -> the macro in config.h that owns the pin it lands on. The
# PN532's TXD is the ESP32's RX, and vice versa: the pair is crossed, which is
# itself a thing people get wrong.
SIGNALS = {"TXD": "PN532_UART_RX", "RXD": "PN532_UART_TX"}


def declared_pins():
    src = CONFIG.read_text()
    pins = {}
    for signal, macro in SIGNALS.items():
        m = re.search(rf"#define\s+{macro}\s+(\d+)", src)
        if not m:
            raise RuntimeError(
                f"{macro} is not defined in {CONFIG.relative_to(REPO)} - the "
                "pin declarations have moved, and this check cannot verify "
                "anything without them")
        pins[signal] = int(m.group(1))
    return pins


def table_rows(text: str):
    """Yield (line number, cells) for every markdown table row."""
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped.startswith("|") or set(stripped) <= set("|- :"):
            continue
        yield lineno, [c.strip() for c in stripped.strip("|").split("|")]


def main() -> int:
    pins = declared_pins()
    docs = [f for f in subprocess.run(["git", "ls-files", "*.md"], cwd=REPO,
                                      capture_output=True, text=True,
                                      check=True).stdout.split()
            if not f.startswith("_internal/")]

    problems = []
    rows_checked = 0

    for name in docs:
        text = (REPO / name).read_text()
        for lineno, cells in table_rows(text):
            joined = " ".join(cells)
            # A wiring row is one that names a PN532 data line and a GPIO.
            for signal, pin in pins.items():
                if not re.search(rf"\b{signal}\b", joined):
                    continue
                found = re.findall(r"(?:GPIO\s*)?\*{0,2}(\d{1,2})\*{0,2}", joined)
                found = [int(x) for x in found if 0 <= int(x) <= 48]
                if not found:
                    continue
                rows_checked += 1
                if pin not in found:
                    problems.append(
                        f"{name}:{lineno}: this row wires PN532 {signal} to "
                        f"GPIO{found[0]}, but config.h declares "
                        f"{SIGNALS[signal]} = {pin}")
                for bad in (6, 7):
                    if bad in found:
                        problems.append(
                            f"{name}:{lineno}: this row wires PN532 {signal} to "
                            f"GPIO{bad}. That is an I2C bus with pull-ups on "
                            "this board: the reader will answer and return "
                            "random UIDs.")

    if rows_checked == 0:
        print("error: found no reader wiring rows in any document - the tables "
              "have moved and this check is checking nothing", file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"checked {rows_checked} wiring rows against config.h "
          f"({', '.join(f'{s}=GPIO{p}' for s, p in pins.items())}), "
          f"{len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
