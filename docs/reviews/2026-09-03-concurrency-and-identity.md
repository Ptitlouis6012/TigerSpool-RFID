# Review — shared state and printer identity

**Date:** 2026-09-03
**Scope:** `firmware/src/main.cpp`, `firmware/src/tigertag_cloud.cpp` — the
boundary between the UI loop and the two background tasks, and how a printer is
identified in storage.
**Method:** read-only. Found while mapping the code, not while looking for bugs.

Both findings are **deferred**, with reasons below. They are recorded here rather
than in `CODEMAP.md` because a landmine row explains a hazard to the next agent
but does not track it — and a documented bug reads as an accepted one.

## Quick wins

None. Neither of these is an hour's work; that is why they are deferred rather
than done.

## Findings

### 1. Shared state crosses a task boundary with no serialisation — deferred

**Where:** `tigertag_cloud.cpp` (`ttSync`, `ttPair`), `main.cpp` (`loadCfg`).

**What breaks.** `ttSync` (16 KB stack) and `ttPair` (12 KB) are the only threads
in an otherwise single-loop firmware. They are pinned to core 1 at priority 1 and
signal completion through bare `volatile bool`. There is no mutex, semaphore or
critical section anywhere in `firmware/src/`.

`ttSync` writes the `tigerspool` NVS namespace. The UI loop reads that same
namespace through `loadCfg()`, which reads six string keys per printer
(`p{i}t`, `p{i}n`, `p{i}h`, `p{i}s`, `p{i}c`, `p{i}v`) in sequence, with no
atomicity across the six.

**Failure scenario.** The user is on the home screen when a scheduled sync lands
— it runs every five minutes, so this needs no unusual timing. `ttSync` rewrites
printer 2 from an account whose entry for that slot has changed. `loadCfg()` is
midway through printer 2: it has already read the old `p2n` and now reads the new
`p2h`. The home screen renders a printer whose name is the old machine and whose
address is the new one. Touching it sends filament to a printer the user did not
select. Nothing logs an error, because from each side nothing went wrong.

`volatile` orders nothing across cores and is not a memory barrier; it only stops
the compiler caching the read.

**Why deferred.** The fix is a design decision, not a patch. Either NVS access is
funnelled through one owner, or the tasks stop writing storage and hand a result
back for the loop to commit. Both change how the account import works, and
choosing between them is not a Phase 3 question.

### 2. A printer's identity is its array index — deferred

**Where:** `main.cpp` — `loadCfg()`, `saveCfg` paths, `savePrinterVisible()`;
`tigertag_cloud.cpp` — the import merge.

**What breaks.** Every stored field is keyed by position: `p0h`, `p1h`, `p2h`.
There is no stable identifier — not the serial, not a UUID. The array slot *is*
the identity. Two consequences compound:

- The merge that preserves locally-corrected values is gated on
  `if (i < n && nt == ct)` — same index **and** same type. When the type at an
  index changes, name, host, serial and access code are all taken from the import
  wholesale.
- `visible` is never written by sync at all, so it stays attached to a slot
  rather than to a printer.

**Failure scenario.** A user with eight printers hides seven and keeps the
FlashForge at index 3. They then delete a Bambu from their TigerTag account in
Tiger Studio. The next sync returns seven printers; everything after the deleted
one shifts up by one. Index 3 is now a Creality. Its type differs, so the merge
overwrites the FlashForge's host and access code with the Creality's, and the
`visible` flag stays on slot 3 — which is now a different machine. The user's
hand-corrected address is gone and the wrong printer is the only one on screen.
No error is reported; the sync did what it was written to do.

**Why deferred.** Fixing it means introducing a stable key and a migration for
devices that already store the positional form, which is exactly the kind of
storage change that has to be settled deliberately. It is also entangled with
finding 1: both are about who owns the printer list.

## What was not looked at

- The four printer backends, beyond the interface they share.
- The LVGL screens, except where they read state.
- The captive portal and the legacy web page.
- The reader and the NFC decode path.
- Anything on hardware. Both findings are read from the source; neither has been
  reproduced on a device, and the timing of finding 1 in particular has not been
  measured.
