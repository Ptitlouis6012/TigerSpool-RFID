#!/usr/bin/env bash
# Point git at the hooks this repository versions.
#
#   bash scripts/install-hooks.sh
#
# It sets core.hooksPath to scripts/hooks/ rather than copying files into
# .git/hooks. Copies go stale the moment the hook changes, and nobody re-runs an
# installer they ran once a year ago. This way a fresh clone that runs this once
# picks up every later change for free.
#
# Undo with: git config --unset core.hooksPath

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

chmod +x scripts/hooks/*
git config core.hooksPath scripts/hooks

echo "core.hooksPath -> scripts/hooks"
echo
echo "pre-commit now runs 'verify.sh --quick' - guards only, no compiling."
echo "The full build stays in 'verify.sh' and in CI."
