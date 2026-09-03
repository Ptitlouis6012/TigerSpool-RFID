# Work log

Everything done since the last commit, in Keep a Changelog's headings, so a
release entry is synthesised from this file rather than re-derived from a diff.

**Append the moment a change is done**, not in a batch at the end. Written at the
end, this file is reconstructed from the diff — which is the exact thing it
exists to prevent.

**Describe the end state, not the journey.** An "Added X" and a later "Fixed X"
from the same cycle collapse into one entry. Anything reverted disappears
entirely: it never shipped.

At each checkpoint, synthesise this file into one line, use that as the commit
message, and reset it to this header.

---

## Unreleased

### Added

- `WORKLOG.md`, `AGENTS.md`, `CLAUDE.md`, `LOCAL.md` — the working contract an
  agent session reads before touching anything. `AGENTS.md` is tool-agnostic and
  carries the layout, the conventions and the settled decisions; `CLAUDE.md` is a
  superset holding the non-negotiables, the hardware facts, the four-command
  workflow and the symptom-to-fix table. `LOCAL.md` is gitignored and holds the
  bench facts no committed file may carry.

  Every non-negotiable names a consequence that is demonstrable from the sources
  rather than asserted, and the release procedure is written out step by step
  with its approval gate, so `CLAUDE.md` answers on its own what board this is,
  how to build it, what to run before reporting, how a release is cut and what
  must not be attempted.

- `CODEMAP.md` — what each source file owns and what it must not be asked to do,
  and a Landmines table: 19 things the code does not say about itself, each
  one something a session already paid for, grouped by the subsystem you would be
  editing when it matters. No line numbers are recorded, because nothing verifies
  them yet, and no row restates something a `grep` would have produced.

- `docs/reviews/`, opened with `2026-09-03-concurrency-and-identity.md`: two
  findings that are bugs rather than hazards — shared state crossing a task
  boundary with no serialisation, and a printer's identity being its array index.
  Both carry a concrete failure scenario and both are marked deferred with the
  reason. Reports are kept permanently and annotated once acted on.

- `scripts/verify.sh`, the one command CI and the bench both run, and eight
  guards behind it: file format, generated files against their generator,
  translation tables against their enums, every drawn string against the
  compiled font, committed text being English, documented device names against
  the formats the firmware builds, documented reader wiring against the pin
  macros, and documentation hygiene. Each names the file and line, each fails
  loudly rather than silently when its input set is empty, and each was verified
  by breaking its input on purpose.
- `scripts/font_range.py` and `scripts/cxx_scan.py`, shared by more than one
  guard so a fact is read once: the font's real character range comes from
  LVGL's generated font source, and C++ is parsed by one state machine rather
  than by a regular expression per line.
- `scripts/hooks/pre-commit` with `scripts/install-hooks.sh`, which points
  `core.hooksPath` at the versioned hooks so a fresh clone picks up later
  changes without re-running anything. The hook runs the guards and never
  compiles.
- `scripts/flash.sh` — build and flash over USB through PlatformIO, with
  `--port`, `--fs`, `--erase` and `--monitor`, documenting which image is written
  at which offset and therefore why saved Wi-Fi survives an ordinary flash.

- `docs/REVIEW-BRIEF.md` — the standing review brief: read-only, six axes
  written for this codebase, and reports that are permanent and annotated.
- `.claude/agents/locator.md` and `single-edit.md`, both narrow by construction,
  with `.gitignore` narrowed to `settings.local.json` so the definitions ship.
- `CLAUDE.md` gains a path-to-consequence table, and names the one case where a
  green bench does not guarantee a green CI.
- `docs/OTA.md` records the release decisions that stop being decisions after
  the first public release: manifest generated not committed, assembled around
  the latest release, binaries never rebuilt, a key layout safe to grow, one
  workflow allowed to deploy, and what `verify-published-site.py` must compare.
- `docs/reviews/2026-09-03` names the deadline three items share — the partition
  table, the manifest keys, and printer identity being an array index — and says
  explicitly that the mutex finding does not share it.

- `scripts/check-release-notes.py` — the eleventh guard, and this file's
  consumer. A release cannot publish without notes, and scaffold text counts as
  none.

### Fixed

- The reference-data generator could not run at all, wrote a Portuguese banner,
  had no shebang, and validated nothing. Two brand names carried a no-break
  space that reached the panel as a blank box.
- Sixty-six pieces of committed text were not English, including serial logs, four
  backends' status strings, an unknown brand rendering as `marca#1234`, and a
  printer type list beginning with `Nenhuma`.
- Six documents described device names the firmware stopped answering to, one of
  them offering a Wi-Fi join QR payload the device does not emit.
- `i18n.cpp` and `i18n.h` claimed the compiled font covers Latin-1. It covers
  ASCII plus degree and bullet.
- Three files had no final newline.

### Changed

- `.gitignore` ignores `LOCAL.md`.
- `docs/ARCHITECTURE.md` no longer carries a source tree. It described twelve
  directories under `src/`, two of which exist, and `main.cpp` as "setup(),
  loop(), and nothing else" — a file of nearly a thousand lines holding the state
  machine. The layout is described in `CODEMAP.md` alone; the document keeps the
  layering and the design rules, which are not the same content.

### Known

- The GPIO6/7 documentation guard cannot tell a prescription from a description:
  it matched a row in `CLAUDE.md` whose subject was the guard's own error
  message. It also passes silently on an empty input set and walks untracked
  build directories. It is rewritten in `scripts/` with those three faults fixed.
