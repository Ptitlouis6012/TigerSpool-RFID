# CLAUDE.md

Read [AGENTS.md](AGENTS.md) first — layout, conventions, and the decisions that
are settled. This file adds what a Claude session needs on top of it.

## What this is

| | |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-2 — ESP32-S3**R8**, 16 MB flash, 8 MB octal PSRAM |
| Panel | 2.0" 240×320 IPS, ST7789, rotation 2, `invert=true`, `rgb_order=false` |
| Touch | CST816S capacitive, on the same board |
| Reader | PN532 over HSU (UART1) at 115200 — **GPIO44 RX, GPIO43 TX** |
| UI | LVGL 8 over LovyanGFX. DMA draw buffers in internal RAM, LVGL heap in PSRAM |
| Printers | One backend per brand behind `printer.h`; Bambu MQTT, Creality/Snapmaker WebSocket, FlashForge HTTP |
| Account | TigerTag Firebase — email/password, or code-and-QR pairing for Google |
| Build | PlatformIO, one environment: `tigerspool`. `cd firmware && pio run -e tigerspool` |
| Flash | Two 4 MB OTA slots, NVS at `0x9000` — `firmware/partitions.csv` |
| Version | One macro, `TIGERSPOOL_FW_VERSION`, in `firmware/include/version.h` |

## Non-negotiables

| Rule | What happens if it is ignored |
|---|---|
| `board_build.arduino.memory_type = qio_opi` stays. | The octal PSRAM is never initialised. `LV_MEM_CUSTOM_ALLOC` is `heap_caps_malloc(n, MALLOC_CAP_SPIRAM)` (`lv_conf.h:24-28`), so **every** LVGL allocation returns NULL and the first screen build crashes inside LVGL — with a backtrace that names neither PSRAM nor the build flag. |
| `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1` stay. | GPIO43/44 are the chip's `U0TXD`/`U0RXD`. These two flags move the serial console onto native USB, which is the only reason those pins are free for the reader. Drop them and UART0 reclaims the pins: the console and the PN532 talk over each other. |
| The partition table is frozen. | A device cannot install a new partition table over the air. Changing `partitions.csv` after the first public release means every user reflashes over USB or is stranded. |
| If a merged factory image is ever produced, it is **never** flashed at `0x0000` on a provisioned device. | A merged image spans from `0x0` and therefore covers `nvs` at `0x9000`. That region holds the saved Wi-Fi credentials, the TigerTag session and the imported printers. Writing it wipes the user's entire setup with no warning and no undo. |
| `firmware/include/tigertag_db.h` is never hand-edited. | It is generated from `firmware/tools/data_src/*.json`. A hand edit survives exactly until the next regeneration, which reverts it silently — and until then the committed file and its stated source disagree. |
| `bash scripts/verify.sh` passes before you report a code change done. | It is what CI runs, and it compiles. `--quick` skips the build: use it as the fast loop while working, not as the thing you report on. A push is not what should tell you an index drifted. |

## Hardware facts that bite

| Fact | Symptom when it is got wrong |
|---|---|
| **The PN532 is on GPIO43/44, never GPIO6/7.** | GPIO6/7 is an I²C bus on this board, with pull-ups, shared with the IMU and the camera header. A reader wired there powers up, enumerates and answers — and returns random UIDs with failing reads. It looks like a flaky tag or a bad antenna. It is neither. |
| **The compiled font is ASCII only.** | `lv_font_montserrat_*` is built with `-r 0x20-0x7F,0xB0,0x2022` — ASCII, degree sign, bullet. Not Latin-1. Any accented character reaches the panel as a blank box, and LVGL logs nothing when it does. Restoring diacritics needs a generated Latin subset font first, and Polish needs Latin Extended-A on top of that. |
| **The framebuffer capture is not the panel.** | `/screen.bmp` serialises what LVGL drew. Colour, backlight and geometry can differ from the glass. Use it to check layout and text; do not use it to settle a question about colour. |
| **`main.cpp` is the only place that owns state.** | Screens are drawn from state, never the reverse. A screen that mutates state directly produces a device whose display and behaviour disagree after the next redraw. |

## Working in the source

Read → grep → slice → smallest edit.

1. `CODEMAP.md` says which file owns the thing, and what that file must not be
   asked to do. Start there, not with a repository-wide grep.
2. Grep for the symbol to get real line numbers. The map orients; the grep is
   the truth. If they disagree, the map is stale — say so.
3. Read the slice around it, wide enough to see the callers.
4. Make the smallest edit that does the job. Then verify.

Do not read a 900-line file in full to change one function.

## The workflow

```bash
bash scripts/verify.sh --quick   # guards, no compiling — the fast loop
bash scripts/verify.sh           # guards plus a real build — before reporting
cd firmware && pio run -e tigerspool   # just the build
bash scripts/flash.sh --monitor  # build, flash over USB, open the console
```

### Releasing

Releasing is a human decision, so no script makes it for you.

1. `bash scripts/bump-version.sh X.Y.Z` — edits the version macro and scaffolds
   the release notes. It stops there: it does not commit, tag or push.
2. Write the release notes. `WORKLOG.md` is what they are written from.
3. Commit, then tag as `vX.Y.Z`. The `v` prefix is what the release workflow
   matches; the rest must equal the macro exactly or the build fails before
   publishing anything.
4. Push the branch, then the tag. The workflow builds from the tagged source and
   publishes the binaries; nothing binary is ever committed.

**Steps 3 and 4 need asking for, each time.** Committing, tagging and pushing are
three separate permissions and none is implied by the steps above it. The tag is
the one action no later edit undoes.

## When a check goes red

| Symptom | Remedy |
|---|---|
| `TIGERSPOOL_FW_VERSION not found` | The macro in `firmware/include/version.h` was removed or renamed. It is the single source of truth; restore it. |
| `tag vX.Y.Z disagrees with TIGERSPOOL_FW_VERSION` | Which one is wrong depends on whether the tag is public. `git ls-remote --tags origin` says which. **Not on the remote:** delete the local tag and re-tag to match the macro. **Already on the remote:** the macro is right and the tag is wrong — do not move the macro to match it, and do not re-point a published tag. Release the next version instead. Either way the commit and the tag need asking for. |
| `_internal/ is tracked by git` | Someone force-added it. ask, then `git rm -r --cached _internal`. It is French working material and must never publish. |
| `broken link -> …` | A relative link in a tracked `.md` points at a file that does not exist. Fix the link or add the file. |
| `GPIO6/7 presented as a PN532 wiring instruction` | A document gives those pins as reader connections. Warning against them is fine; prescribing them is not. See the pin row under Hardware facts. |
| Build fails only in CI | The PlatformIO cache is keyed on `firmware/platformio.ini`. If dependencies moved, the local `.pio` may be ahead of CI's. Delete `firmware/.pio` and rebuild locally before blaming CI. |

## Hard rules

- **Never commit, tag or push unless asked.** Tags especially: the release
  workflow acts on them, and no later edit takes one back.
- **No AI attribution anywhere.** Not in commit messages, not in pull request
  bodies, not in comments, not in contributor lists.
- **Conversation may be in French. Everything committed is English** — code,
  comments, commit messages, documentation, log lines, user-visible strings.
- **Verify your own work before reporting it.** Ask for a hardware test only for
  what genuinely needs the device in front of someone, and ask once, at the
  moment it is needed — never as a substitute for looking.
- **Report faithfully.** What compiled, and what was actually observed on
  hardware, are two different claims. "Compiles clean; the reader path is
  unverified without the device" is useful. Implying a bench result you do not
  have is not.
- **When a rule here turns out to be wrong, change it in the same session.** A
  file that describes a repository it no longer matches is worse than no file.

## Delegation

Delegate to keep search cost out of the main context, not to go faster.

`.claude/agents/` defines `locator` (read-only; answers "where is the code
that…" with paths and symbols) and `single-edit` (one already-decided,
self-contained change).

**Never delegate:** a change spanning two subsystems, a new subsystem, a
debugging session, the version decision, the guards themselves, the final
read-through, or any git operation.

Fanning out parallel agents is not the default. Each carries its own system
prompt and independently re-reads the same large files, so it multiplies the
bill to keep one context lean. If a block of work is genuinely too large, hand
the whole block to exactly one agent with a self-sufficient prompt.
