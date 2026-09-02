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
firmware must reach a known-good state — display up, touch responding, Wi-Fi
associated — and only then mark itself valid. If it crashes or panics before
that, the bootloader reverts to the previous slot on the next boot.

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

## Signing

**The firmware verifies a signature before it boots a downloaded image.** A
SHA-256 checksum only proves the download was not corrupted; it proves nothing
about who produced it. Without a signature, anyone who can answer for the update
host — a hijacked DNS answer, a captive portal, a compromised CDN — can install
arbitrary firmware on every device in the field.

- Signing key lives in GitHub Actions secrets. Never in the repository.
- The **public** key is compiled into the firmware.
- The bootloader's own secure boot is a separate, later decision — it is
  irreversible per device and must not be enabled casually.

**TODO — to confirm with Benoit:** whether TigerTag has an existing firmware
signing key and process to reuse, or whether one is created for TigerSpool.

---

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

## Open questions

- **Does the LittleFS image update over the air, or only the app?** The web config
  UI lives in the filesystem partition. Updating both means two images to keep in
  step and two ways to half-fail. Undecided.
- **What counts as "booted successfully"** for the rollback check. Display and
  touch are obvious. Whether Wi-Fi association should be required — which would
  roll back a perfectly good build when a router is down — is not.
