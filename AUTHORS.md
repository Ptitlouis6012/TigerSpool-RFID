# Authors and credits

## Maintainer

**Benoit Michaut** — [TigerTag Project](https://github.com/TigerTag-Project)

## The prototype

TigerSpool is built on a working bench prototype, written together with
**[RP3D-S](https://github.com/RP3D-S)**.

That prototype is where essentially everything this repository knows came from:

- The four printer protocols — **Creality**, **FlashForge**, **Bambu Lab** and
  **Snapmaker** — each reverse-engineered and verified against a physical
  printer, including the parts that are not in any documentation: FlashForge's
  24-colour palette and its habit of acknowledging commands it ignored;
  Creality's seven-digit colour format; Bambu Lab's dynamic slot discovery;
  Snapmaker's space-delimited G-code arguments.
- The **`PrinterBackend` abstraction**, which held across all four protocols
  without changing — the design this firmware still uses
  ([docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)).
- Every **hardware fact** in [docs/WIRING.md](docs/WIRING.md): the reader on
  GPIO43/44 rather than 6/7, the wake-up preamble these modules need before every
  command, the page ranges and retry strategy that make a marginal HSU link
  usable.
- The state machine, the TigerTag decoder, the account import, and the built-in
  captive portal.

The firmware here is a restructuring of that work, not a replacement for it.

## Ecosystem

TigerSpool is part of the [TigerTag](https://tigertag.io) ecosystem and builds on:

- **[TigerTag-RFID-Guide](https://github.com/TigerTag-Project/TigerTag-RFID-Guide)**
  — the open tag protocol and the public registry.
- **[Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)**
  — the desktop printer manager. TigerSpool's whole premise is that your printers
  are already in there.
- **[TigerScale V3](https://github.com/TigerTag-Project/Tiger-Scale-V3)** — the
  connected filament scale, and the reference for how a TigerTag firmware
  repository is structured, from account pairing to release automation.

## Third-party

Open-source libraries this project depends on, and their licenses:
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

Protocol reverse-engineering by the wider community made several of the printer
backends possible. Attributions are recorded next to each backend in the source.

---

Contributed something? Open a PR adding yourself.
