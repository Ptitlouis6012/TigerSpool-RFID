# Changelog

All notable changes to this project are documented here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.4.0] - 2026-09-04

### Fixed

- **The boot screen was gone before it could be read.** The device comes up
  faster than the eye, so LVGL began repainting over the logo within a fraction
  of a second — which looks exactly like an image that does not fit the screen.
  It is now held for one second, and the second is not wasted: the deadline is
  set where the image is drawn and only waited out at the end of startup, so
  LVGL, the language table, the storage read and the reader handshake all happen
  inside it. A board slower than a second to boot waits for nothing.

### Added

- `?preview=splash` draws the boot screen on demand, so it can be checked from a
  desk. It cannot otherwise be captured: it is shown before the web server
  exists, which is the whole point of drawing it that early.


## [1.3.0] - 2026-09-04

### Added

- **A boot screen.** Pushed to the panel from flash immediately after the display
  is initialised and before LVGL exists, so everything that follows — LVGL
  starting, the language table, the NVS read, the reader handshake — happens with
  the logo already up instead of behind a black screen. No timer holds it: it
  stays until the first real screen replaces it, so the device is never slower
  than it needs to be in order to look considered. 150 KB of flash, taking the
  app partition from 36.5% to 40.3%.

### Changed

- The installer's shopping list carries only what is actually bought. The jumper
  wires come in the PN532's box and the case comes off your own printer; both are
  in the bill of materials instead.


## [1.2.0] - 2026-09-04

### Fixed

- **The sign-in QR led to the Wi-Fi picker.** It carried the device's root URL,
  and that page opens with the network selector and puts the account form some
  forty lines below it — so a phone scanning it to sign in landed on a Wi-Fi list.
  The first control under the finger was also a save-and-restart for the network
  settings, on a device that had just joined a network. The QR now points at
  `/login`, a page with one job. Reported by a user.

### Added

- **A web installer**, at
  [tigertag-project.github.io/TigerSpool-RFID](https://tigertag-project.github.io/TigerSpool-RFID/) —
  plug a board in, press Install. Built on the same design system as the
  TigerScale installer, with the parts list, their links and the two things that
  otherwise cost an evening: Erase wipes the saved Wi-Fi and account, and a board
  offering no serial port is usually a charge-only cable.
- The installer speaks **nine languages**, one more than the device: Chinese
  renders in a browser and would reach the panel as empty boxes.

### Changed

- **The README starts where people actually get stuck.** A TigerSpool reads its
  printers from a TigerTag account, so an empty printer list is an account with
  no printers in it — not a fault. Installing Tiger Studio Manager, creating the
  account and adding the printers is now the section above the quick start.
- **The bill of materials is three things**, with links: the board, the PN532 and
  a USB cable that carries data.
- **Four wires, not six.** `config.h` declares the UART and nothing else, so the
  reset line the wiring guide described as "optional, recommended" was recommended
  by nothing — no code reads or writes it. Both the guide and the pinout say it is
  not connected, and the four wires come with the PN532.


## [1.1.0] - 2026-09-04

**First official release.** A TigerTag NFC chip on a spool, read by the box, and
the filament it names written into a printer's slot — material, brand, colour
and temperatures — without typing anything on the device.

### The device

- **First boot end to end**: language in eight locales, Wi-Fi over a QR code and
  a captive portal that joins without rebooting, then linking a TigerTag account
  by email or by Google.
- **Every screen is LVGL**, over a display port that keeps its DMA draw buffers
  in internal RAM and the LVGL heap in PSRAM.
- **Settings**: printers, Wi-Fi, account, screen, language, update, restart and
  factory reset. The factory reset is a two-second hold and clears all four NVS
  namespaces, so it cannot quietly undo itself.
- **A printer picker**, because an account can hold ten printers while the
  machine next to the box is one of them. Hiding is not deleting.
- **The screen sleeps** — dim, dark, wake on touch. The waking touch is
  consumed, so reaching for a sleeping device cannot send filament to a slot.
- **Per-device names**, `tigerspool-xxxx.local` and `TigerSpool-Setup-XXXX`, so
  two of these can share a network.
- **Slot names match the printers'**: `Ext.` plus `1A`–`1D` on Creality and
  FlashForge, `A1`–`A4` then `B1`–`B4` on Bambu, `E1`–`E4` on Snapmaker.

### Over-the-air update

- Fetch the published manifest, compare versions, stream the image into the
  spare OTA slot while hashing it, refuse it unless the checksum matches, and
  restart into it. Nothing touches the running slot, so a failure costs a
  download and nothing else.
- The manifest is generated at release time from the release's own artefacts,
  never committed and never rebuilt, and published by exactly one workflow —
  which then verifies what is actually being served.
- **The image is not signed.** The connection is verified; who produced the
  firmware is not proven. See [docs/OTA.md](docs/OTA.md).

### Security

- **Certificates are verified on every call that leaves the network** — the
  account sign-in, the token refresh, the pairing, the Firestore import, the
  manifest and the firmware. All of them previously ran without checking who
  answered.
- The Bambu backend is the one deliberate exception: a printer on the local
  network presenting a self-signed certificate, trusted through the access code.

### The working contract

- `AGENTS.md`, `CLAUDE.md`, `CODEMAP.md` and `WORKLOG.md`, plus eleven guards
  behind one command, `scripts/verify.sh`, which CI runs rather than its own
  copy. They cover file format, generated files against their generators,
  translation tables against their enums, every drawn string against the
  compiled font, that committed text is English and comes from the translation
  table, that documented device names and reader wiring match the code, and that
  a release has notes.
- `/screen.bmp` and `/api/tap` make the panel readable and drivable from a desk.

### Documentation

- Product definition and positioning ([README.md](README.md)).
- Target firmware architecture: layers, state machine, printer backend
  abstraction, transport/protocol split ([docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)).
- User journey, screen by screen ([docs/ONBOARDING.md](docs/ONBOARDING.md)).
- Wi-Fi provisioning design — built-in captive portal, Wi-Fi join QR code
  ([docs/WIFI-PROVISIONING.md](docs/WIFI-PROVISIONING.md)).
- Account pairing design — QR pairing for Google accounts, email/password from
  the phone, with the endpoints that need confirming
  ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md)).
- OTA design — two-slot partition layout, rollback, signing, channels
  ([docs/OTA.md](docs/OTA.md)).
- Complete verified wiring and the failure modes behind it
  ([docs/WIRING.md](docs/WIRING.md), [hardware/pinout.md](hardware/pinout.md)).
- Honest three-level printer compatibility matrix
  ([docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md)).
- Migration plan from the bench prototype, including the hardware facts that must
  not be lost ([docs/MIGRATION.md](docs/MIGRATION.md)).
- Bill of materials ([hardware/BOM.md](hardware/BOM.md)).
- 3D model directory structure and the rule that only the shell changes
  ([models/README.md](models/README.md)).
- The account data model — what the device reads from a TigerTag account and
  what it writes back after a scan ([docs/ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md)).
- Web installer design ([installer/README.md](installer/README.md)).
- CI workflow placeholders, issue and PR templates.
- MIT license, trademark policy, security policy, code of conduct, contributor
  credits.

### Decided

- **Account pairing keeps both email/password and Google**, as TigerScale does.
  RFC 8628 was examined and rejected — the QR cannot carry the code, polling
  needs a `client_secret` a public binary must not ship, and it is Google-only.
  ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md#why-not-rfc-8628))
- **The reader is on GPIO43/44**, never GPIO6/7 — bench-verified, and it
  contradicts the prototype's own draft README.
  ([docs/WIRING.md](docs/WIRING.md))
- **The captive portal is a state in the main firmware**, never a separate binary
  to flash. ([docs/WIFI-PROVISIONING.md](docs/WIFI-PROVISIONING.md))
- **Nine languages**, aligned with TigerScale V3.
- **Elegoo and Anycubic are targeted for v1 as a port, not a
  reverse-engineering exercise.** Both protocols are documented and working in
  Tiger Studio from live slicer captures: Elegoo is MQTT over plain TCP on 1883,
  Anycubic is MQTT/TLS on 9883. No firmware backend exists for either yet.
  ([docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md))
- **An Anycubic printer must have been paired in AnycubicSlicerNext once.** Its
  broker credentials exist nowhere else and cannot be derived from the printer.
  Tiger Studio reads them into the account; TigerSpool imports them. This is the
  product working as designed, and it is documented rather than left to surface
  as a failure.
- **The partition table is set before the first public release**, because
  changing it afterwards costs every user a USB reflash.
  ([docs/OTA.md](docs/OTA.md))
- **Pairing targets the deployed Cloud Functions** (`pairStart` / `pairPoll`),
  which is what TigerScale V3 calls in shipped firmware. The `/api/device/pair/*`
  path in V3's documentation is not implemented anywhere and is treated as a
  future surface. ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md#endpoints))
- **OTA images are signed, and TLS certificates are validated.** No TigerTag
  signing key exists to reuse: V3 verifies a SHA-256 fetched over the same
  unauthenticated TLS connection as the image itself, and skips verification
  entirely when no hash is supplied. TigerSpool refuses an unsigned image rather
  than installing it. ([docs/OTA.md](docs/OTA.md#integrity-and-authenticity))
- **The device writes slot changes back to the account.** A confirmed assignment
  updates the slot's material, colour, vendor and the scanned tag's UID, so a
  spool scanned in the workshop is visible in Tiger Studio and on a phone. Never
  for cloud-mode printers, whose state the vendor's cloud already owns, and never
  before the printer confirms.
  ([docs/ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md#writing-back))
- **Credentials are a named bag, not a fixed struct.** Six brands use six
  different credential vocabularies in the account, and a printer may be
  cloud-only with no local address at all.
  ([docs/ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md#credentials-are-not-one-shape))
- **Rollback is implemented deliberately, not assumed.** Two OTA partitions make
  an update possible, not reversible; real rollback needs the bootloader option
  *and* an explicit validity call. V3 has neither while its comments claim
  otherwise. ([docs/OTA.md](docs/OTA.md#rollback))
- Identifiers fixed: `tigerspool-xxxx.local`, `TigerSpool-Setup-XXXX`, PlatformIO env
  `tigerspool`, NVS namespace `tigerspool`.

### Verified on hardware

Recorded because they were measured rather than assumed.

- **End-to-end assignment on a FlashForge AD5X** — a model the prototype's
  FlashForge backend was not written for. The tag was read on the first attempt,
  the assignment was sent, and the change was **confirmed by re-reading the
  printer's own slot state**, not by trusting its acknowledgement.
- **Fidelity loss is real and user-visible.** `PLA High Speed / #DC123F` arrived
  as `PLA / #F82D29`. The result screen has to say the colour was adapted.
- **The reader works on the first try at GPIO43/44**, with no retry and no
  rejected reads, on a second board.
- **Anycubic's broker does not require TLS 1.2 or a client certificate**, at
  least on a Kobra X — contradicting the ecosystem's own protocol notes and
  removing the largest stated risk for that backend.

### Added since the bootstrap

The firmware landed on `phase-2/firmware-import` and the UI was rebuilt on
LVGL. Highlights, in the order they matter to someone holding the device:

- **First boot works end to end** — language in eight locales with their
  accents, Wi-Fi over a QR and a captive portal that joins without
  rebooting, then linking a TigerTag account by email or by Google.
- **Settings**, eight entries: printers, Wi-Fi, account, screen, language,
  update, restart, factory reset. Each shows its current value on the row.
- **A printer picker.** An account can hold ten printers while the machine
  next to the box is one of them. Hiding is not deleting, and visibility
  belongs to the user rather than the account.
- **The screen sleeps** — dim, dark, wake on touch. Only the light stops.
- **Per-device names**: `tigerspool-xxxx.local` and `TigerSpool-Setup-XXXX`,
  because two devices could not previously coexist on one network.
- **Two OTA partitions** on a 16 MB layout. The prototype declared no table
  and built against an 8 MB default, leaving half the flash unreachable.

### Not yet done

- No OTA. The partitions are ready; the update code is not.
- No web installer page.
- No 3D models.
- No web installer page.
- No Elegoo or Anycubic firmware backend, though both protocols are documented.

[Unreleased]: https://github.com/TigerTag-Project/TigerSpool-RFID/commits/main
