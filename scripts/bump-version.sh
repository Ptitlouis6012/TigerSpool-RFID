#!/usr/bin/env bash
# Set the firmware version, and only ever from here.
#
#   scripts/bump-version.sh 0.2.0
#
# It edits the one macro, writes a CHANGELOG heading for the release, and tells
# you the two commands that publish it. It does not commit, tag or push: those
# are decisions, and a script that makes them for you is a script that
# eventually makes them by accident.
set -euo pipefail

NEW="${1:-}"
if ! printf '%s' "$NEW" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "usage: $0 MAJOR.MINOR.PATCH" >&2
  exit 1
fi

HEADER="firmware/include/version.h"
OLD=$(grep -oE '"[^"]+"' "$HEADER" | tr -d '"')
[ "$OLD" = "$NEW" ] && { echo "already $NEW"; exit 0; }

sed -i.bak "s/\"$OLD\"/\"$NEW\"/" "$HEADER" && rm -f "$HEADER.bak"
echo "firmware/include/version.h: $OLD -> $NEW"

TODAY=$(date +%Y-%m-%d)
if ! grep -q "^## \[$NEW\]" CHANGELOG.md; then
  python3 - "$NEW" "$TODAY" <<'PY'
import sys, pathlib
new, today = sys.argv[1], sys.argv[2]
p = pathlib.Path("CHANGELOG.md")
s = p.read_text()
marker = "## [Unreleased]"
if marker not in s:
    raise SystemExit("CHANGELOG.md has no [Unreleased] section")
# Everything under Unreleased becomes the release; Unreleased starts empty
# again. Writing release notes at tag time means writing them from memory.
head, rest = s.split(marker, 1)
p.write_text(f"{head}{marker}\n\n## [{new}] - {today}\n{rest}")
print(f"CHANGELOG.md: moved Unreleased into [{new}]")
PY
fi

cat <<MSG

Next, if that is what you want:

  git commit -am "Release $NEW"
  git tag v$NEW && git push origin main --tags

The release workflow refuses to publish if the tag and the macro disagree.
MSG
