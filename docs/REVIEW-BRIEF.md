# Review brief

A standing brief so a review can be asked for in one sentence and still come
back comparable to the last one.

> "Review the account import against `docs/REVIEW-BRIEF.md`."

## Reviews are read-only

**A reviewer that starts fixing things stops looking.** The findings further down
the file never happen, because attention went into the first one. Read, record,
hand back. Someone acts on it afterwards — possibly you, in a different session,
with the whole list in front of you and able to rank it.

The one exception is a typo in the report you are writing.

## When to run one

- Before a release, over whatever changed since the last one.
- After a subsystem lands, over that subsystem.
- When something misbehaves in a way nobody can reproduce — review the paths
  that could produce it, rather than instrumenting blind.
- Never as a substitute for the guards. `verify.sh` answers "did this drift".
  A review answers "is this right", which no script can.

## Scope and where to start

Say the scope in the report's header and hold to it. A review of everything is a
review of nothing.

Start with `CODEMAP.md`, and read its Landmines rows for anything in scope
before reading the code — several of them describe behaviour that looks like a
bug and is not, and rediscovering that costs the session.

## Axes

Each with what it means *here*, not in general.

**Correctness.** State machines that can wedge: `main.cpp` owns every transition,
and a state entered with no path out is a device that needs a power cycle. And
results that depend on which of two code paths won a race — the firmware has two
FreeRTOS tasks beside a superloop and no mutex anywhere, so "who wrote last"
is a real question, not a theoretical one.

**Memory.** Internal RAM is the scarce resource, not flash. LVGL's draw buffers
must be in internal DMA-capable RAM, mbedTLS wants a large stack, and the Bambu
status report can exceed 50 KB. Running out does not fail where it was spent: it
kills the device somewhere unrelated, with a backtrace naming neither the file
nor the feature that starved it. Look at what allocates, not at what crashed.

**Silent failure modes.** The ones this codebase actually produces: a fetch that
returns a default on error and looks like an empty account; a send with no retry
that reports success because the socket accepted it; a guard that stands down
without saying so. **A wrong answer nobody is told about is worse than an
error.** Ask of every failure path: does anything reach a human?

**Hardware truth.** What the schematic says, what the vendor demo says and what
the bench does are three different things — GPIO6/7 is the standing example.
Distinguish what you verified on a device from what you read. `/screen.bmp`
makes the panel checkable from here; use it before asking anyone to look.

**First-try UX.** The user has a 2.0" screen, no keyboard, and has never seen
this product. Every screen should say what to do next in one sentence, in the
language they chose. Count the taps to the outcome.

**Token cost.** A file that has to be read in full to change one function, a
map that sends you to the wrong place, a guard whose output cannot be skimmed.
These are real costs paid every session.

## Output

One file, `docs/reviews/YYYY-MM-DD-<scope>.md`.

1. **Header** — date, scope, and method: what you read, what you ran, what you
   looked at on hardware.
2. **Quick wins**, as a table, first. Each scoped to about an hour. This is where
   the next session starts, so it goes at the top even though it is the least
   severe material. Write "None" rather than padding it.
3. **Findings**, most severe first. Each names the file, the function, what
   breaks, and the concrete input or state that triggers it.
4. **What was not looked at**, explicitly. A reader must be able to tell absence
   of a finding from absence of a look.

**A finding without a failure scenario is an opinion.** "This could race" is an
opinion. "A sync landing between two of the six key reads gives a printer whose
host came from the new import and whose name from the old" is a finding. If you
cannot write the second kind, either dig until you can or file it as a question.

## Reports are permanent, and annotated

Never delete or overwrite a report. After acting on one, mark **every** finding
in the report itself:

- **FIXED** — with what changed, and whether a guard now prevents it returning.
- **DEFERRED** — with the reason. "Later" is not a reason; "needs a storage
  migration that has to be settled deliberately" is.
- **REJECTED** — the finding was wrong. Say why, in the report. That is worth
  more than quietly dropping it.

An unannotated report reads to the next reviewer as though nothing was ever
done, and the same findings get re-litigated from scratch — which costs more
than the review did.
