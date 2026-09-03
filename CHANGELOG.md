# Changelog

All notable changes to this project are documented here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Repository bootstrap. **No firmware yet** — this release is documentation,
structure and decisions only.

### Added

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
- Identifiers fixed: `tigerspool.local`, `TigerSpool-Setup`, PlatformIO env
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
- **Per-device names**: `tigerspool-XXXX.local` and `TigerSpool-Setup-XXXX`,
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
