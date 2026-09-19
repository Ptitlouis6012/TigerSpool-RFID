#!/usr/bin/env python3
"""The device and the portal must agree on what a signal strength looks like.

The Wi-Fi wave has a scale - how many decibels make one arc - and it is written
twice, in two languages, because the two places that draw it do not share code:

  firmware/src/ui/signal_level.h   C++, drawn on the panel
  firmware/src/net/portal_page.h   JavaScript, drawn in a phone's browser

They agree today because they were changed together. What this guard exists for
is the day somebody adjusts one of them alone - six months from now, for a good
reason, in a hurry. Nothing in a review catches that: both files are correct on
their own, and only a person holding a phone next to the device would notice
that the portal is showing three bars for the network the panel calls two.

It fails loudly on an empty scan as well. If either file stops matching the
patterns below, this is checking nothing, and a guard that silently checks
nothing is worse than no guard: it reports success for a question it never
asked.
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
DEVICE = REPO / "firmware" / "src" / "ui" / "signal_level.h"
PORTAL = REPO / "firmware" / "src" / "net" / "portal_page.h"

# int fromRssi(int rssi) { if (rssi >= -70) return 3; ... }
DEVICE_RE = re.compile(r"rssi\s*>=\s*(-\d+)\s*\)\s*return\s*(\d)")
# function bars(r){return (r>=-70?3:r>=-80?2:r>=-90?1:0)+1}
PORTAL_RE = re.compile(r"r\s*>=\s*(-\d+)\s*\?\s*(\d)")


def scale(path: pathlib.Path, pattern: re.Pattern) -> list[tuple[int, int]]:
    """The (dBm, arcs) pairs a file declares, in the order it declares them."""
    text = path.read_text(encoding="utf-8")
    return [(int(dbm), int(arcs)) for dbm, arcs in pattern.findall(text)]


def main() -> int:
    for path in (DEVICE, PORTAL):
        if not path.exists():
            print(f"error: {path.relative_to(REPO)} is missing - this check is "
                  "checking nothing", file=sys.stderr)
            return 2

    device = scale(DEVICE, DEVICE_RE)
    portal = scale(PORTAL, PORTAL_RE)

    for name, found in (("signal_level.h", device), ("portal_page.h", portal)):
        if len(found) != 3:
            print(f"error: read {len(found)} threshold(s) from {name}, expected "
                  "3 - the scale moved and this check no longer reads it",
                  file=sys.stderr)
            return 2

    if device != portal:
        print("error: the device and the portal disagree about signal strength",
              file=sys.stderr)
        print(f"  firmware/src/ui/ui/signal_level.h : {device}".replace("ui/ui/", "ui/"),
              file=sys.stderr)
        print(f"  firmware/src/net/portal_page.h    : {portal}", file=sys.stderr)
        print("  A network drawn with three bars on the phone and two on the "
              "panel is one network and two answers. Change both.",
              file=sys.stderr)
        return 1

    pairs = ", ".join(f"{dbm} dBm -> {arcs}" for dbm, arcs in device)
    print(f"device and portal agree: {pairs}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
