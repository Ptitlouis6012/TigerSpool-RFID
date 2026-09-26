#!/usr/bin/env python3
"""The printable cases follow one layout, carry plain names and leak no paths.

Model3D/ holds `0.Desktop/` - the universal stand - and one numbered directory
per brand (`1.BambuLab/` ...), each holding one directory per printer model.
Many people will add to it, from many slicers, so three things are checked
rather than hoped for:

- NAMES. No spaces anywhere: a space breaks every link to the file in a
  README. Directories are written for reading - `BambuStudio`, `OriginalFiles`
  - and may use capitals; files are lower case and hyphens, because a file name
  that differs only by case works on a Mac and fails on Linux. README.md and
  TEMPLATE.md are the exceptions.
- LAYOUT. A top-level directory is `0.Desktop` or one of the brands the
  firmware knows, and every model directory - `0.Desktop/` or
  `<brand>/<model>/` - has a README.md.
- 3MF METADATA. A slicer keeps the full path each object was imported from,
  which on a public repository is somebody's user name and folder layout. It
  must have been cut to a file name (scripts/clean-3mf.py does it).
- PICTURE METADATA. A phone photo carries EXIF: the phone, the time and the
  GPS position it was taken at - somebody's home, on a public repository. A
  picture here carries none (Model3D/README.md has the command).

Files not yet committed are checked too: this is meant to fail before the
commit that would publish them, not after. It exits 2 when Model3D/ holds no
model at all - an empty scan means it is checking nothing.
"""

import pathlib
import re
import subprocess
import sys
import zipfile

REPO = pathlib.Path(__file__).resolve().parent.parent
ROOT = "Model3D"
DESKTOP = "0.Desktop"
BRANDS = {"1.BambuLab", "2.Creality", "3.Snapmaker", "4.FlashForge", "5.Elegoo", "6.Anycubic"}
EXEMPT = {"README.md", "TEMPLATE.md"}
PLAIN = re.compile(r"^[a-z0-9]+(?:[-.][a-z0-9]+)*$")        # files
READABLE = re.compile(r"^[A-Za-z0-9]+(?:[-.][A-Za-z0-9]+)*$")  # directories
ALLOWED = {".md", ".3mf", ".stl", ".step", ".stp", ".f3d", ".png", ".jpg", ".jpeg", ".webp"}
SOURCE = re.compile(r'key="source_file" value="([^"]*)"')


def files():
    out = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", ROOT],
        cwd=REPO, capture_output=True, check=True).stdout.decode()
    return [f for f in out.split("\0") if f and (REPO / f).exists()]


def main() -> int:
    paths = files()
    problems = []
    models = set()
    for f in paths:
        parts = f.split("/")[1:]
        if len(parts) == 1:
            if parts[0] not in EXEMPT:
                problems.append(f"{f}: only README.md and TEMPLATE.md sit at the top of {ROOT}/")
            continue
        top = parts[0]
        if top != DESKTOP and top not in BRANDS:
            problems.append(f"{f}: '{top}' is neither {DESKTOP}/ nor a brand ({', '.join(sorted(BRANDS))})")
            continue
        if top != DESKTOP and len(parts) == 2 and parts[1] != "README.md":
            problems.append(f"{f}: a brand directory holds its README and model directories, not files")
        if top == DESKTOP:
            models.add(f"{ROOT}/{DESKTOP}")
        elif len(parts) >= 3:
            models.add(f"{ROOT}/{top}/{parts[1]}")
        for depth, name in enumerate(parts):
            if name in EXEMPT:
                continue
            if depth < len(parts) - 1:
                if not READABLE.match(name):
                    problems.append(f"{f}: directory '{name}' - letters, digits, dots and hyphens, no spaces")
                    break
            elif not PLAIN.match(name.rsplit(".", 1)[0]):
                problems.append(f"{f}: file '{name}' - lower case, digits and hyphens, no spaces")
        ext = pathlib.PurePath(f).suffix.lower()
        if ext not in ALLOWED:
            problems.append(f"{f}: '{ext}' is not a model, a picture or a document")
        if ext in (".jpg", ".jpeg", ".png", ".webp"):
            data = (REPO / f).read_bytes()
            if b"Exif\x00\x00" in data[:65536] or b"eXIf" in data[:65536]:
                problems.append(f"{f}: carries EXIF metadata (phone, date, GPS) - strip it before committing")
        if ext == ".3mf":
            try:
                with zipfile.ZipFile(REPO / f) as z:
                    for n in z.namelist():
                        if not n.endswith(".config"):
                            continue
                        for v in SOURCE.findall(z.read(n).decode("utf-8", "replace")):
                            if "/" in v or "\\" in v:
                                problems.append(f"{f}: {n} keeps the path '{v}' - "
                                                "run scripts/clean-3mf.py on it")
                                break
            except zipfile.BadZipFile:
                problems.append(f"{f}: not a readable 3MF")
    for m in sorted(models):
        if f"{m}/README.md" not in paths:
            problems.append(f"{m}/: a model directory needs a README.md - start from {ROOT}/TEMPLATE.md")
    if not models:
        print(f"no model found under {ROOT}/ - checking nothing")
        return 2
    for p in problems:
        print(p)
    print(f"checked {len(models)} model(s), {len(paths)} file(s), {len(problems)} violation(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
