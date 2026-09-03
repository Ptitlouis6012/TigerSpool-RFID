#!/usr/bin/env bash
# The firmware version lives in exactly one place: firmware/include/version.h.
#
# Everything else derives from it - the release workflow, the OTA manifest, the
# footer of the setup portal. Two places to change a version is one place to
# forget, and a device reporting a version it is not is a support thread that
# never converges.
#
# On a tag build this also checks the tag agrees with the macro, so a mistyped
# tag fails before anything is published rather than after.
set -euo pipefail

HEADER="firmware/include/version.h"
[ -f "$HEADER" ] || { echo "::error::$HEADER is missing"; exit 1; }

MACRO=$(grep -oE '#define[[:space:]]+TIGERSPOOL_FW_VERSION[[:space:]]+"[^"]+"' "$HEADER" \
        | sed 's/.*"\(.*\)"/\1/')

if [ -z "$MACRO" ]; then
  echo "::error::TIGERSPOOL_FW_VERSION not found in $HEADER"
  exit 1
fi

if ! printf '%s' "$MACRO" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "::error::version '$MACRO' is not MAJOR.MINOR.PATCH"
  exit 1
fi

echo "firmware version: $MACRO"

REF="${GITHUB_REF_NAME:-}"
case "$REF" in
  v*)
    TAG="${REF#v}"
    if [ "$TAG" != "$MACRO" ]; then
      echo "::error::tag v$TAG disagrees with TIGERSPOOL_FW_VERSION=$MACRO"
      echo "         run: scripts/bump-version.sh $TAG"
      exit 1
    fi
    echo "tag $REF matches"
    ;;
esac
