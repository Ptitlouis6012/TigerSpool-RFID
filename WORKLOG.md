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

## 2026-09-05 - the update screen, and the header everywhere

### Changed

- The update page checks on entry. The button stays for offline and for retry.
- A 152 px progress ring, percentage inside, no header and no exit while the
  image is being written.
- The header is the ground plus a rule, not a filled bar. One style, so every
  screen changed together.
- Installed version as a settings row; the state as a glyph in a coloured ring.
- Removed the channel row and the now-dead `S_CHANNEL` key.

### Fixed

- `scripts/flash.sh` was broken without `--port`: empty array under `set -u`.

Verified on hardware: home, settings, update (checking / up to date), Wi-Fi
setup and Wi-Fi settings, driven over `/api/tap` and read back from
`/screen.bmp`.

