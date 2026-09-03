# Over-the-air updates

A box sitting behind a printer must update itself. Asking a non-technical user to
find a USB cable, install a browser flasher and re-flash the device for every bug
fix is not a plan — it means most devices in the field never get fixed.

> **Status: not implemented.** The prototype has no OTA of any kind — no update
> code, no manifest, no version endpoint. This document is the design, and it is
> a v1 blocker, not a later nicety.

---

## Partition layout

OTA needs **two application slots**. `esp_ota_get_next_update_partition()` only
ever returns an `ota_N` partition; a layout with a single `factory` app is not
eligible and every update attempt fails with no free partition.

```
# Name      Type  SubType   Offset      Size      Notes
nvs         data  nvs       0x9000      0x5000    Wi-Fi, account session, printers
otadata     data  ota       0xe000      0x2000    which slot boots
app0        app   ota_0     0x10000     0x400000  4 MB
app1        app   ota_1     0x410000    0x400000  4 MB
littlefs    data  spiffs    0x810000    0x300000  web config UI
coredump    data  coredump  0xFF0000    0x10000
```

16 MB of flash on this board, ~10 MB used, room to grow either slot later.

**`nvs` stays at `0x9000` at that size, permanently.** It holds the saved Wi-Fi
credentials, the account session and the printer list. Moving or resizing it
costs every user their entire setup.

**Changing the partition table itself requires a USB reflash.** A device cannot
install a new partition layout over the air. Getting this right *before* the first
public release is what avoids ever having to ask users to re-flash.

---

## How an update runs

```
   device                            update server
     |                                     |
     |--- GET version.json --------------->|   on boot, then daily
     |<-- version, sha256, url, notes      |
     |                                     |
  [ newer than what's running? ]           |
     |                                     |
  [ tells the user, waits for a tap ]      |
     |                                     |
     |--- GET firmware.bin --------------->|
     |<== streamed into the inactive slot ==|
     |                                     |
  [ verifies sha256 and signature ]        |
  [ marks the new slot pending ]           |
  [ reboots ]                              |
     |                                     |
  [ new firmware boots ]                   |
  [ self-checks, marks itself valid ]      |
     |                                     |
  [ or: fails → bootloader rolls back ]    |
```

### Rollback

The rollback is the part that makes OTA safe enough to enable by default.

A newly flashed slot is marked **pending verification**, not valid. The new
firmware must reach a known-good state — display up, touch responding — and only
then mark itself valid. If it crashes or panics before that, the bootloader
reverts to the previous slot on the next boot.

> **This does not come for free, and it is easy to believe it does.** Two OTA
> partitions make an update *possible*; they do not make it *reversible*. Genuine
> rollback needs `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` in the bootloader **and**
> an explicit `esp_ota_mark_app_valid_cancel_rollback()` call once the new
> firmware has proved itself. Without both, a freshly flashed image that boots and
> then misbehaves is simply kept.
>
> TigerScale V3 has neither, while its source comments assert that the ESP32
> "auto-rolls-back if the new firmware doesn't boot cleanly". It does not.
> TigerSpool must implement this deliberately and test it by shipping a
> deliberately broken build to a bench device.

A device that bricks itself on a bad update is worse than a device that never
updates. A user cannot recover one that lives behind a printer, and they should
never have to.

### Resume and failure

- A download interrupted mid-transfer leaves the *inactive* slot corrupt and the
  running firmware untouched. It simply retries.
- A failed download is never fatal: the device keeps running what it has.
- Power loss during a write is survivable for the same reason — the running slot
  is not the one being written.

---

## Version manifest

One generated file, published by CI. There is exactly one source of truth for the
version — a macro in the firmware source — and CI refuses to publish if the git
tag disagrees with it. Manifests are generated, never hand-edited: the OTA and the
web installer read the same file, which is what keeps them from drifting apart
because someone forgot a step.

```json
{
  "version":       "1.0.0",
  "channel":       "stable",
  "released":      "2026-01-01T00:00:00Z",
  "firmware_url":  "https://tigertag-project.github.io/TigerSpool-RFID/firmware/tigerspool-v1.0.0.bin",
  "firmware_sha256": "…",
  "signature":     "…",
  "min_version":   "1.0.0",
  "notes_url":     "https://github.com/TigerTag-Project/TigerSpool-RFID/releases/tag/v1.0.0"
}
```

`min_version` exists so a future release can refuse to update directly from a
version too old to migrate cleanly — the device is told to go through an
intermediate release rather than ending up with unreadable NVS.

---

## Integrity and authenticity

Two different problems, often confused. TigerSpool has to solve both.

**Integrity** — did the image arrive intact? A SHA-256 over the download answers
this. It catches truncation, corruption and a half-finished transfer.

**Authenticity** — did *we* produce this image? A hash cannot answer this. If an
attacker controls what the device downloads, they control the hash it is compared
against too, unless that hash arrives over a channel the device can authenticate.

### What the ecosystem does today

TigerScale V3 verifies **SHA-256 only, over TLS with certificate validation
disabled**. Its own code says so:

```c
// We use WiFiClientSecure with setInsecure() for TLS — relying on SHA-256
// integrity rather than CA validation. This is acceptable because:
//   — The expected hash comes from Firestore (already auth'd to the user)
//   — A MITM swapping the binary would fail SHA verification -> no install.
```

**That reasoning does not hold, and it is worth being precise about why:** the
expected hash is fetched with `setInsecure()` as well. An attacker positioned to
swap the binary is positioned to swap the hash it is checked against — it is the
same connection, with the same absence of certificate validation. The hash
protects against a corrupted download. It does not protect against an adversary.

There is also no fallback: when no expected hash is supplied, verification is
**skipped** and the image installs anyway.

**There is no TigerTag firmware signing key.** No public key is compiled into V3,
no signing step exists in its release workflow, and its published artifacts carry
only a `SHA256SUMS.txt`. Nothing exists to reuse — whatever TigerSpool does here
is new.

### What TigerSpool does

**Validate TLS certificates.** This is the cheaper half of the fix and it closes
most of the gap on its own: a hash delivered over an authenticated channel is
meaningfully a hash *from us*. A pinned root CA bundle, with a plan for rotating
it before it expires.

**Then verify a signature before boot.** TLS authenticates the *server*; a
signature authenticates the *image*, and keeps holding if the host is
compromised, if a CDN is poisoned, or if a certificate is mis-issued. For a
device that lives behind a printer and updates itself unattended, that is the
property worth having.

- The signing key lives in GitHub Actions secrets. Never in the repository.
- The **public** key is compiled into the firmware.
- **Never skip verification when a signature is absent.** An unsigned image is
  refused, not installed. A verification step that can be bypassed by omitting
  the thing being verified is not a verification step.

The ESP32's own secure boot is a separate, later decision — it is irreversible
per device and must not be enabled casually.

## Channels

| Channel | Who it is for |
|---|---|
| `stable` | Everyone. The default. Only tagged releases. |
| `beta` | Opt-in from the settings screen. Gets releases early. |

Two channels, chosen from the device screen. A user on `beta` can go back to
`stable` and will simply stop receiving updates until stable catches up.

---

## On the device

**Automatic checks, explicit installs.** The device checks for updates on its own
and tells the user one is available. It does not install it silently — an update
takes the device offline for a minute, and it should not choose that moment for
the user.

**Progress is visible.** A percentage and a phase — downloading, verifying,
restarting. A 2.0" screen showing nothing for ninety seconds is indistinguishable
from a crash, and the user's instinct will be to pull the power at exactly the
wrong moment.

**Never mid-scan.** No update starts while a tag is being read or an assignment
is in flight.

---

## What CI publishes

Every tagged release, from the [release workflow](../.github/workflows/release.yml):

| Artifact | For |
|---|---|
| `tigerspool-v<version>.bin` | OTA |
| `tigerspool-v<version>.factory.bin` | first flash |
| `bootloader.bin`, `partitions.bin`, `boot_app0.bin` | writing a blank board from the browser |
| `littlefs.bin` | the web config UI |
| `SHA256SUMS.txt` | verification |
| `version.json` | published to GitHub Pages; the device polls it |

---

## Decisions settled before the first release

None of this is built — the installer does not exist and `pages.yml` is a
placeholder that fails on purpose. The decisions are recorded now because after
the first public release they stop being decisions: devices in the field read
the shape they left with, and an old unit coming back online after months
expects it unchanged.

### The manifest is generated, never committed

A committed copy and a generated one drift, and the drift is invisible until a
fleet acts on it. It is produced during deployment and exists only in the
deployed artefact.

### It is assembled around the latest *release*, never around what is live

Ask the GitHub release API for the latest release and describe that. Then the
same run produces the same correct output whether it happens before or after a
release is published, and ordering stops being something anyone has to reason
about. Assembling it from "whatever is currently deployed" makes the result
depend on when the workflow happened to run.

### Binaries are never rebuilt when assembling it

Publish the release assets themselves. The SHA-256 in the manifest must be the
hash of the exact bytes the device downloads. A rebuilt binary differs — build
paths, timestamps — and a device that verifies the hash before switching boot
partitions will reject it. Correctly, and very confusingly.

### The key layout must be safe to grow

Devices in the field parse this file for years. So:

- **Parse through a filter, ignoring unknown keys.** A device must read a
  manifest written by a later version without failing. This is the property that
  makes every future addition free.
- **Never remove a key, never repurpose one.** Adding `min_version_note` beside
  `min_version` is safe. Changing what `min_version` means is not, because the
  units that already shipped will keep applying the old meaning.
- **Never change a key's type.** A string that becomes an object breaks every
  parser that shipped.
- `version`, `firmware_url` and `firmware_sha256` are the three a device cannot
  work without. Everything else must have a sane default when absent.

### Exactly one workflow may deploy the site

**This is the failure with the worst blast radius in the reference project's
history.** Two workflows deployed for the same commit. GitHub reported both
successful, marked the older deployment inactive, and went on serving it. The
manifest it served advertised the previous version, so every device in the field
reported "up to date" against a release that had already shipped. Nothing was
red anywhere. The only way to see it was to fetch the site and compare it to the
release.

So: one workflow, named in this file, with a `concurrency` group. Any second
path to deployment is a bug regardless of whether it currently works.

### `scripts/verify-published-site.py` must exist alongside it

Fetch the **live manifest** and the **latest release**, and require that they
describe the same firmware byte for byte:

| Compare | Why |
|---|---|
| manifest `version` ↔ release tag | the whole failure mode above |
| manifest `firmware_sha256` ↔ SHA-256 of the release asset | proves the URL serves the bytes the manifest promises |
| manifest `firmware_url` reachable, and its body hashes to that value | catches a deploy that published the metadata but not the binary |

Run it hourly on a schedule so a lost deployment repairs itself, and make it
runnable by hand any time there is doubt about what devices can see. **Verify
the published result, not the workflow's green check** — a deployment can report
success and serve the previous build.

### Release notes cannot be forgotten

`verify.sh` must fail when the current version has no notes, or when they still
hold scaffold text, and the release workflow must refuse to publish without
them. `bump-version.sh` scaffolds them alongside the macro edit.

Notes written at tag time are written from memory. `WORKLOG.md` exists so they
do not have to be. Unlike everything else in this section, this one is
buildable today — it needs no installer.

## Open questions

- **Does the LittleFS image update over the air, or only the app?** The web config
  UI lives in the filesystem partition. Updating both means two images to keep in
  step and two ways to half-fail. Undecided.
- **What counts as "booted successfully"** for the rollback check. Display and
  touch are obvious. Whether Wi-Fi association should be required — which would
  roll back a perfectly good build when a router is down — is not.
