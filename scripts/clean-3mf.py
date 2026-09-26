#!/usr/bin/env python3
"""Make a slicer's 3MF fit to publish: no local paths, names read by a human.

A 3MF saved by Bambu Studio, Creality Print or OrcaSlicer records, for every
object, the full path of the file it was imported from - `C:\\Users\\<name>\\
Desktop\\...` - so that "reload from disk" can find it again. On a public
repository that is a user name and a folder layout. It also keeps every object's
name exactly as it was typed while designing, and nobody rereads those. Bambu
Studio also stamps the designer's account number (`DesignerUserId`), which is
the same number as that person's Bambu cloud user name.

This rewrites each `source_file` down to its bare file name, which is all the
slicer needs to show and nothing a stranger can use, empties DesignerUserId,
and prints every object
and part name so they are read before they are published. Geometry, plates and
print settings are copied byte for byte.

    python3 scripts/clean-3mf.py Model3D/<brand>/<model>/<slicer>/*.3mf

scripts/check-models.py refuses a committed 3MF that still carries a path.
"""

import os
import re
import sys
import zipfile

SOURCE = re.compile(r'(key="source_file" value=")([^"]*)(")')
NAME = re.compile(r'key="name" value="([^"]*)"')
DESIGNER = re.compile(r'(<metadata name="DesignerUserId">)[^<]*(</metadata>)')


def clean(path: str) -> None:
    with zipfile.ZipFile(path) as z:
        items = [(info, z.read(info.filename)) for info in z.infolist()]
    changed, names = 0, []
    out = []
    for info, data in items:
        if info.filename.endswith(".config"):
            text = data.decode("utf-8")

            def strip(m):
                nonlocal changed
                base = re.split(r"[\\/]", m.group(2))[-1]
                if base != m.group(2):
                    changed += 1
                return m.group(1) + base + m.group(3)

            text = SOURCE.sub(strip, text)
            if info.filename.endswith("model_settings.config"):
                names += NAME.findall(text)
            data = text.encode("utf-8")
        elif info.filename == "3D/3dmodel.model":
            text = data.decode("utf-8")
            new = DESIGNER.sub(r"\1\2", text)
            if new != text:
                changed += 1
                data = new.encode("utf-8")
        out.append((info, data))
    tmp = path + ".tmp"
    with zipfile.ZipFile(tmp, "w") as z:
        for info, data in out:
            z.writestr(info, data)
    os.replace(tmp, path)
    print(f"{path}: {changed} path(s) or account number(s) removed")
    for n in sorted(set(names)):
        print(f"    object: {n}")


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.strip().splitlines()[0])
        print("usage: clean-3mf.py FILE.3mf [...]")
        return 2
    for path in sys.argv[1:]:
        clean(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
