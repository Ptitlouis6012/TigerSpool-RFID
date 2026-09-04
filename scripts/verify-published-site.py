#!/usr/bin/env python3
"""The published manifest must describe the latest release, byte for byte.

    python3 scripts/verify-published-site.py --url https://owner.github.io/repo/

**Verify the published result, not the workflow's green check.** A deployment can
report success and serve the previous build.

That is not hypothetical. In the sibling project two workflows deployed for the
same commit; GitHub reported both successful, marked the older one inactive, and
kept serving it. The manifest it served advertised the previous version, so every
device in the field reported "up to date" against a release that had already
shipped. Nothing was red anywhere. The only way to see it was to fetch the site
and compare it to the release - which is this script.

It is run at the end of a deployment, and on a schedule, so a lost deployment
repairs itself rather than waiting for somebody to notice.
"""

import argparse
import hashlib
import json
import subprocess
import sys
import urllib.request


def fetch(url: str, binary: bool = False):
    with urllib.request.urlopen(url, timeout=60) as r:
        data = r.read()
    return data if binary else data.decode()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True, help="the Pages site root")
    a = ap.parse_args()

    site = a.url.rstrip("/") + "/version.json"
    try:
        manifest = json.loads(fetch(site))
    except Exception as e:
        print(f"error: cannot read the published manifest at {site}: {e}", file=sys.stderr)
        return 1

    tag = subprocess.run(["gh", "release", "view", "--json", "tagName", "-q", ".tagName"],
                         capture_output=True, text=True, check=True).stdout.strip()
    expected = tag.lstrip("v")
    published = manifest.get("version", "")

    problems = []
    if published != expected:
        problems.append(
            f"the site advertises {published!r} while the latest release is "
            f"{expected!r} - every device checking for updates is being told it "
            "is current when it is not")

    url = manifest.get("firmware_url", "")
    declared = (manifest.get("firmware_sha256") or "").lower()
    if not url or not declared:
        problems.append("the manifest carries no firmware url or checksum")
    else:
        try:
            actual = hashlib.sha256(fetch(url, binary=True)).hexdigest()
        except Exception as e:
            problems.append(f"the firmware it points at is not downloadable: {e}")
        else:
            if actual != declared:
                problems.append(
                    "the firmware served does not hash to what the manifest "
                    f"declares\n    declared {declared}\n    served   {actual}\n"
                    "    every device will download it and refuse to install it")

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    if problems:
        return 1

    print(f"published manifest matches release {tag}: version {published}, "
          f"firmware checksum verified against the bytes actually served")
    return 0


if __name__ == "__main__":
    sys.exit(main())
