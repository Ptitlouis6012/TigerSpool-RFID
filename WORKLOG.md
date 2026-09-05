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

### Changed (same cycle, folded into 1.5.0)

- `theme::WARN` added. Restart orange, factory reset red, and the Settings
  Update row orange when a version is waiting.
- One OTA check twenty seconds after boot, so the menu can show that.
- Update page: `v` prefix, smaller badge (62 px, proportionate to 240 px),
  "Wi-Fi, account and printers are kept" under an available update.
- Wording taken from the scale after reading its live view directly:
  "Installed version", "Your TigerSpool is up to date".

Answers from the TigerScale agent are in `_internal/TIGERSCALE-UI-ANSWERS.md`.
Still open from them: row icons (the scale carries the colour on a 26 px icon
and keeps the label white - we carry it on the label, having no icons), and the
font fallback chain that would restore accents.

- Settings rows now have icons. `frame::row` takes an optional LV_SYMBOL_* and
  a tint; the value's width cap drops from 108 to 84 when there is one, because
  the 28 px icon column was pushing the chevron off the row. `showMenu` takes a
  `MenuState` struct - four adjacent bools as positional arguments is a swap
  waiting to happen.

- Row icons are now drawn from LVGL primitives in `ui/icons.{h,cpp}` rather than
  taken from `LV_SYMBOL_*` where LVGL has no glyph for the thing: a person for
  the account (an envelope says "messages"), a globe for the language (a
  keyboard is not a language), a printer, a sun. The globe uses the
  TigerScale's own 22 px coordinates verbatim. About a kilobyte of code and no
  data - no font to generate, no licence to carry, nothing for a guard to
  police. The remaining four rows keep LV_SYMBOL_*, which is the right answer
  where LVGL already has the shape.
- Colour rule tightened to the scale's: an icon is plain white unless it says
  something worth seeing without reading the row. Three tinted in the healthy
  case, not six.

### Fixed

- The Settings menu showed an empty Wi-Fi network. `WiFi.SSID()` and
  `ttcloud::email()` both return String BY VALUE, and moving the call into a
  MenuState struct meant the temporaries died before the struct was read. It
  never crashed - it just looked like a network problem. Held in named locals.

## 2026-09-05 - the icons, from the scale's actual code

- `CI_USER` rebuilt verbatim: two solid discs, no outline, the shoulders clipped
  by the box. My reconstruction had used outlines and could not have matched.
- Printer and sun redrawn against the scale's silhouette rules.
- Recorded for the font work: the scale's sun is FontAwesome 6.5.2 Free Solid
  U+F185, fetched at generation time from the pinned tag, never committed. It
  rides in the generated Latin face rather than a face of its own -
  `lv_font_conv` takes several `--font` in one call. The FA font files are
  SIL OFL 1.1 and the icons CC BY 4.0; both get cited. Note that the resulting
  `Opts` line carries two `-r`, which is a trap for `check-generated.py`.
- Also from that exchange: a drawn icon costs one lv_obj per stroke in RAM for
  as long as the screen is loaded. Negligible on a settings row that exists
  once; not negligible on a list that can hold twenty. Answers in
  `_internal/TIGERSCALE-UI-ICONS-2.md`.

- The sun is now the real glyph rather than a drawing. `make-icon-font.sh`
  registered with `check-generated.py`, which had to learn that a generator is
  not always Python - it ran everything through `sys.executable` and would have
  reported "failed to run" for the wrong reason. The .ttf is cached under
  `.cache/fonts/`, keyed on the tag, so the guard re-running on every verify
  does not mean a download on every verify.
- Attribution added: Font Awesome fonts are SIL OFL 1.1, the icon artwork
  CC BY 4.0. lv_font_conv extracts outlines from the .ttf, so the OFL is what
  governs the compiled result.
- **When the Latin face is generated, fold this glyph into it and delete the
  script.** One call, several `--font`, no second face and no extra link in the
  fallback chain - and watch that the resulting `Opts` line carries two `-r`,
  which a naive generated-file check reads as one.

- Printer icon reverted to its first geometry. Widening the body and fattening
  the output tray followed the scale's "one dominant form" rule, and on the
  glass it read worse: the heavy block at the bottom took over and the printer
  stopped looking like a printer. A rule about silhouettes is not a substitute
  for looking at the thing.

## 2026-09-05 - orientation, row style, account name, a clickable live view

- `screenRotation` stored in NVS as `rot`, applied after `lvgl_port::begin()`
  so the boot logo and everything after it agree. `lvgl_port::setRotation()`
  keeps the panel in the port; main owns the value.
- `theme::LINE` is now the scale's `0x2E3646` and rows carry a 1 px border of
  it; radius 7 -> 9; row chevron 12 -> 24 px, back chevron 20 -> 24. The value
  cap drops to 74 with an icon, because the icon column and a full-size chevron
  together cost about 55 px of a 240 px row.
- `ttcloud::displayName()` - the name when there is one, the address otherwise.
  `accounts:signInWithPassword` returns `displayName`; the QR pairing path does
  not, so `fetchProfileName()` fills it from `accounts:lookup` on the next sync
  when it is missing. That is what makes it appear on a device that was already
  signed in.
- `/screen` forwards clicks to `/api/tap`, drags over 12 px as swipes.
  Coordinates come from the image's bounding rect, so it works on a phone where
  the image is scaled down.

- Wi-Fi strength on the home header, left of the gear. Four levels by colour;
  the level, not the dBm, goes into the redraw signature.

### Fixed

- Captive portal not opening on Android (reported on a Galaxy S24, iPhone on
  the same firmware was fine). `startBackgroundScan()` moved out of the last
  line of `beginAP()` and into `handlePortal()`. The AP was channel-hopping
  during the exact second Android probes for a portal. Two comments in this
  file already said scanning destabilises the AP; the call site contradicted
  them and nothing connected the two.
- **Not reproduced here** - no S24 on the bench, and the mechanism is a timing
  window that needs a real Android probe. Diagnosed from the code path and the
  iPhone/Android asymmetry. Needs the reporter to confirm.

