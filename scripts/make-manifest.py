#!/usr/bin/env python3
"""Generate the published manifest for a release.

    python3 scripts/make-manifest.py --version 0.2.0 --repo owner/name \
        --dist dist --out pages/version.json

Read by two clients that must never disagree about which firmware is current:
the device in ota.cpp, and the web installer when it exists. Generating one file
for both is what stops them offering different versions.

THREE RULES, each of which has cost somebody a release elsewhere.

**Never committed.** A committed copy and a generated one drift, and the drift is
invisible until a fleet acts on it.

**Built around the release, never around what is live.** The version and the
assets come from the tag being published, so this produces the same correct
output whether it runs before or after the release appears, and ordering stops
being something anyone reasons about.

**The hash is of the exact bytes the device downloads.** These are taken from the
release artefacts themselves, never from a rebuild: build paths and timestamps
differ, and a device that verifies before switching boot partitions will reject
a rebuilt image - correctly, and very confusingly.

The key layout is meant to grow. Devices in the field parse this through a
filter and ignore what they do not know, so keys may be ADDED beside the
existing ones forever. No key may be removed, repurposed, or change type: units
that have been unplugged for months come back and read exactly the three flat
keys below.
"""

import argparse
import datetime
import hashlib
import json
import pathlib
import sys

# Kept in step with partitions.csv. The web installer needs these to place each
# image on a blank board, and getting one wrong bricks it, so they are written
# once here rather than repeated in the installer's own page.
OFFSETS = {
    "bootloader.bin": 0x0000,
    "partitions.bin": 0x8000,
    "boot_app0.bin":  0xE000,
    "firmware.bin":   0x10000,
}


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--repo", required=True, help="owner/name")
    ap.add_argument("--dist", required=True, help="directory holding the release artefacts")
    ap.add_argument("--out", required=True)
    ap.add_argument("--released", default=None,
                    help="ISO 8601 timestamp; defaults to now, in UTC")
    a = ap.parse_args()

    dist = pathlib.Path(a.dist)
    owner, name = a.repo.split("/", 1)
    base = f"https://github.com/{a.repo}/releases/download/v{a.version}"

    firmware = dist / f"tigerspool-v{a.version}.bin"
    if not firmware.exists():
        print(f"error: {firmware} is missing - nothing to publish", file=sys.stderr)
        return 1

    manifest = {
        # The three flat keys the device reads. Their position and meaning are
        # frozen: every unit in the field looks for exactly these.
        "version": a.version,
        "firmware_url": f"{base}/{firmware.name}",
        "firmware_sha256": sha256(firmware),

        # Everything below is for the installer and for humans. A device that
        # does not know these keys skips them.
        "released": a.released or datetime.datetime.now(
            datetime.timezone.utc).replace(microsecond=0).isoformat(),
        "notes_url": f"https://github.com/{a.repo}/releases/tag/v{a.version}",
        "assets": [],
    }

    for item, offset in sorted(OFFSETS.items(), key=lambda kv: kv[1]):
        path = dist / item
        if not path.exists():
            continue
        manifest["assets"].append({
            "name": item,
            "url": f"{base}/{item}",
            "offset": offset,
            "sha256": sha256(path),
        })

    out = pathlib.Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"OK -> {out}  (version {a.version}, {len(manifest['assets'])} assets)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
