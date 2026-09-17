# Moving printer traffic off the interface loop

Step 1 of three: **the boundary**, written before any code moves. Steps 2 and 3
are issues; this file is what they are built against.

Nothing here changes behaviour. It is an inventory of who writes what, who reads
it, and where the line has to be drawn so that two threads never touch the same
bytes.

---

## Why

One loop does everything: talk to each printer, draw the screen, read the touch
panel, repeat. A connection to a printer that is switched off blocks inside the
TLS handshake, and while it blocks nothing is drawn and no touch is read.

Measured on the bench board, six printers imported and several of them off:

| | |
|---|---|
| Screen blind, total | 7.9 s out of every 80 |
| Typical gap | 56 ms |
| Worst gap | **1 324 ms** — a TLS handshake |

The worst gaps are not slow code; they are waiting. Buffering, batching and
frame pacing (all done in 1.55.0) shorten the gaps caused by printers that
answer. They cannot shorten a wait for a printer that does not.

---

## What the split has to protect

Two threads writing and reading the same bytes is the hardest class of fault to
diagnose: not reproducible, dependent on the exact instant the two cross, and
usually surfacing hours later as a reboot with no explanation. Today it cannot
happen, because there is one thread. Every line below exists to keep that true
once there are two.

### Who owns what, today

| Data | Written by | Read by | When |
|---|---|---|---|
| `SlotState slots_[]`, `nSlots_` | the backend, from its own report handler | `screen_slots.cpp` (`slot(i)`, `slotCount()`, `slotLabel(i)`), `main.cpp` after a send | every draw of the slot grid |
| `connected_`, `status_` | the backend | `main.cpp` (`tickLink`, the dialler), `screen_slots` via `linkState` | every pass |
| sockets (`WiFiClientSecure`, `PubSubClient`, `WebSocketsClient`, `HTTPClient`) | the backend | nobody else | — |
| `PrinterCfg printers[]` | `main.cpp` (`loadCfg`, account sync) | backends at `begin()`, every screen | sync, and every draw |
| `Link links[]`, `s_dialer` | `main.cpp` | `main.cpp`, `screen_slots` indirectly | every pass |
| `selectedPrinter`, `selSlot`, `state` | `main.cpp` | every screen | every pass |

The one that matters is the first row. **`screen_slots::show()` takes a
`PrinterBackend*` and reads live backend memory while it draws.** That is fine
with one thread and is exactly what breaks with two: a report arriving mid-draw
would rewrite a colour, a type or a slot count under the code reading it. The
count is the dangerous one - `nSlots_` changes at runtime, when a Bambu reports
its AMS topology, and a grid drawn against a count that shrinks reads past the
end of the array.

### The rule

> **A screen never reads a backend. It reads the last snapshot the backend
> published.**

A snapshot is a plain value - slot count, labels, states, connected flag -
copied out by the backend when it has finished updating itself, and handed over
whole. The interface thread reads the copy it was given, which cannot change
while it is being read. Nothing is locked while drawing, because the thing being
drawn is not the thing being written.

```
  printer task                        interface loop
  ------------                        --------------
  read the socket                     take the published snapshot (atomic swap)
  update slots_[]                     draw from it
  publish a snapshot  ----------->    the next draw sees the next snapshot
  handle queued commands  <--------   push assign / refresh / stop / foreground
```

Two directions, two mechanisms, both one-way:

- **State, task → loop.** A double buffer plus an atomic pointer swap. The
  publisher fills the spare copy and swaps; the reader takes the pointer once
  per draw. No mutex on the drawing path.
- **Commands, loop → task.** A FreeRTOS queue of small structs (`ASSIGN slot
  tag`, `REFRESH`, `STOP`, `FOREGROUND on/off`). Results come back as another
  snapshot, or as a queued event for the screens that wait for one (`Send`).

`assign()` is the only call that is synchronous today and must stop being so:
it writes to the printer and answers. It becomes a command plus a result event,
and the result screen waits for the event instead of a return value.

---

## What moves and what does not

**Moves to the printer task:** every socket, every backend `loop()`, every
connect, `assign()`, `refresh()`, and the reachability probe that already runs
on a task of its own.

**Stays on the interface loop, and must:**

- **Everything LVGL.** It is not thread-safe and the drawing buffers are shared
  with the panel's DMA. The rule already written in `CODEMAP.md` for the web
  server applies here unchanged.
- **The web server and `/screen.bmp`.** It reads the canvas sprite, which is
  written by the drawing code; it is safe only because both run in the same
  loop.
- **NVS writes.** `Preferences` is not reentrant across tasks in this codebase's
  usage; the account sync already respects this.
- **`printers[]` and the account sync.** The task reads it; only the loop
  writes it, and only between a `STOP` and a `BEGIN` for the affected printer.

---

## The hazards, named

| Hazard | Why it bites | What the design does about it |
|---|---|---|
| A report arriving mid-draw | `nSlots_` shrinks and the grid reads past the array | screens read a snapshot, never the backend |
| `PubSubClient` callbacks | they run inside `mqtt_.loop()`, so on the printer task | the callback only ever touches backend-owned memory |
| Heap | six TLS sessions plus a task stack, in internal RAM, already fragmented enough to have broken the account sync at 16 372 free bytes | the task's stack is sized once and measured with `uxTaskGetStackHighWaterMark`; the existing stand-down keeps its role |
| The watchdog | the loop feeds it once a pass; a task that blocks for a minute must not | the printer task subscribes to the watchdog itself, or is deliberately not subscribed and says so |
| Two writers of `printers[]` | account sync rewriting a printer a backend is using | a printer is stopped before its config is replaced - already true, and it must stay true |
| Cloud Bambu's shared session | one TLS session shared by several printers, currently pumped from the loop | it moves with the others, onto the same task, so it keeps one owner |

---

## Staged, so each stage can be judged

**Step 2 - the connects.** Only the dialling connect moves to the task; the
rest stays. It is the 1.3 s gaps, the highest value and the smallest surface: a
connect touches sockets and a `connected_` flag, and no screen reads a socket.
It needs no snapshot yet.

**Step 3 - the traffic.** Backend `loop()`, `assign()` and `refresh()` move,
the snapshot and the command queue appear, and `screen_slots::show()` stops
taking a `PrinterBackend*`.

### How each stage is judged

The same measurement, on the same bench, with the same six printers: the gap
between two LVGL frames, logged when it exceeds 40 ms.

| | Before 1.55.0 | 1.55.0 | Step 2 target | Step 3 target |
|---|---|---|---|---|
| Blind total | 8.9 s / 75 s | 7.9 s / 80 s | under 3 s / 80 s | under 1 s / 80 s |
| Worst gap | 1 318 ms | 1 324 ms | under 150 ms | under 60 ms |

A stage that does not move its number is a stage that did not work, whatever
the code looks like.
