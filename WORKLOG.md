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
