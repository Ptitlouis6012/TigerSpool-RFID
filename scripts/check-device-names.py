#!/usr/bin/env python3
"""Documented device names must match the ones the firmware actually builds.

The device derives both of its names from its own station MAC, in webcfg.cpp:

    snprintf(HOSTNAME_BUF, ..., "tigerspool-%02x%02x", mac[4], mac[5]);
    snprintf(AP_SSID_BUF,  ..., "TigerSpool-Setup-%02X%02X", mac[4], mac[5]);

The suffix exists because two of these on one network could not otherwise be
told apart. The case difference is not cosmetic: the hostname is lowercase and
the access point is uppercase, and they are not interchangeable.

The incident: six documents kept describing the pre-suffix names -
"tigerspool.local" and "TigerSpool-Setup" - long after the firmware stopped
answering to either. One of them offered a Wi-Fi QR payload the device does not
emit. Every one of those is an instruction that fails in the way a broken
device fails, so the reader blames the hardware.

Fixing them by hand is what produced the problem: a manual pass is not a guard.
So the format strings are read from the source and the documentation is checked
against them. When the naming changes, the code changes and this follows.

Documentation may use XXXX or xxxx where a real device has hex, in the matching
case.

WHAT THIS DOES NOT CATCH. It reads the stem from the source, so it checks the
suffix and the case of names built on the current stem. It will not notice
documentation still using an entirely superseded name: the migration documents
legitimately cite the prototype's `tigertag.local`, so "every .local must be
ours" would be wrong. That gap needs a list of retired names, which is more
machinery than it is worth today - and a guard is more useful with its limit
written down than trusted past it.
"""

import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
WEBCFG = REPO / "firmware" / "src" / "webcfg.cpp"


def formats():
    """The two name formats, read from the code that builds them."""
    src = WEBCFG.read_text()
    host = re.search(r'HOSTNAME_BUF[^;]*?"([^"]*%0[^"]*)"', src, re.S)
    ssid = re.search(r'AP_SSID_BUF[^;]*?"([^"]*%0[^"]*)"', src, re.S)
    if not host or not ssid:
        raise RuntimeError(
            "could not read the hostname/SSID formats from "
            f"{WEBCFG.relative_to(REPO)} - they have moved or changed shape, "
            "and this check cannot verify anything without them")
    return host.group(1), ssid.group(1)


def to_regex(fmt: str) -> str:
    """"tigerspool-%02x%02x" -> a pattern accepting hex or the XXXX placeholder."""
    out, placeholder = "", ""
    for part in re.split(r"(%0\d[xX])", fmt):
        if re.fullmatch(r"%0\d[xX]", part):
            width = int(part[2])
            if part.endswith("x"):
                out += f"[0-9a-f]{{{width}}}"
                placeholder += "x" * width
            else:
                out += f"[0-9A-F]{{{width}}}"
                placeholder += "X" * width
        else:
            out += re.escape(part)
    return f"(?:{out}|{re.escape(fmt.split('%')[0] + placeholder)})"


def main() -> int:
    host_fmt, ssid_fmt = formats()
    host_ok = re.compile(to_regex(host_fmt))
    ssid_ok = re.compile(to_regex(ssid_fmt))
    host_stem = host_fmt.split("%")[0].rstrip("-")      # "tigerspool"
    ssid_stem = ssid_fmt.split("%")[0].rstrip("-")      # "TigerSpool-Setup"

    docs = [f for f in subprocess.run(["git", "ls-files", "*.md"], cwd=REPO,
                                      capture_output=True, text=True,
                                      check=True).stdout.split()
            if not f.startswith("_internal/")]
    if not docs:
        print("error: found no documentation to check", file=sys.stderr)
        return 2

    problems = []
    for name in docs:
        for lineno, line in enumerate((REPO / name).read_text().splitlines(), 1):
            # A hostname is any mention of the stem that resolves over mDNS.
            for m in re.finditer(rf"{re.escape(host_stem)}[\w-]*\.local", line):
                if not host_ok.match(m.group(0)):
                    problems.append(
                        f"{name}:{lineno}: '{m.group(0)}' is not a name this "
                        f"device answers to - the firmware builds "
                        f"'{host_fmt}' from its MAC")
            for m in re.finditer(rf"{re.escape(ssid_stem)}[\w-]*", line):
                if not ssid_ok.match(m.group(0)):
                    problems.append(
                        f"{name}:{lineno}: '{m.group(0)}' is not an SSID this "
                        f"device raises - the firmware builds '{ssid_fmt}' "
                        "from its MAC, in upper case")

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"scanned {len(docs)} documents against '{host_fmt}' and "
          f"'{ssid_fmt}', {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
