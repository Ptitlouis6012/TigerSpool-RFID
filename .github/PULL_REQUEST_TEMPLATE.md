## What this changes

<!-- And why. If the reason is obvious from the diff, one line is enough. -->

## What you tested it on

<!--
Documentation change? Say "docs only".

Firmware change? We need:
  - the board it ran on
  - for a printer backend: the printer model AND its firmware version
  - for a reader change: that a tag still reads reliably, not once
-->

## Checklist

- [ ] One concern. A rename and a behaviour change in one diff cannot be reviewed.
- [ ] English — comments, identifiers, log messages.
- [ ] No credentials, keys or tokens added anywhere.
- [ ] Any hardware constant carries a comment saying what breaks without it.
- [ ] Docs updated if behaviour changed.
- [ ] `CHANGELOG.md` updated for anything user-visible.

<!--
New printer brand? It should be one class implementing PrinterBackend plus a
registration line, touching nothing else. If it needed changes to the UI or the
state machine, say so — that probably means the interface is wrong, which is
worth knowing.
-->
