<h1 align="center">TigerSpool RFID</h1>

<p align="center">
  <strong>Any printer. No setup. Just scan.</strong><br>
  Tap a filament spool on the box, tap a slot on the screen — the filament is loaded into your printer.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue.svg" alt="Platform: ESP32-S3">
  <img src="https://img.shields.io/badge/Build-PlatformIO-orange.svg" alt="Build: PlatformIO">
  <img src="https://img.shields.io/badge/Status-Work%20in%20progress-red.svg" alt="Status: work in progress">
  <a href="https://discord.gg/3Qv5TSqnJH"><img src="https://img.shields.io/badge/Discord-Join-5865F2.svg" alt="Discord"></a>
</p>

> [!WARNING]
> **This repository is documentation and structure only. There is no firmware in it yet.**
>
> The design is settled and a working prototype exists on the bench, but its code
> has not been migrated here. Nothing in this repository builds, flashes or runs
> today. Follow [CHANGELOG.md](CHANGELOG.md) or
> [watch the repository](https://github.com/TigerTag-Project/TigerSpool-RFID/subscription) to know when it does.

---

## You already added your printers once. Why do it again?

Every other spool scanner asks you to flash a board by hand, edit a YAML file,
find your printer's IP address, and dig an access code out of a settings menu —
per printer, on a device with no keyboard.

TigerSpool doesn't. You add your printers **once**, in
[Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager) on
your computer, where you have a keyboard and a real screen. Then you sign
TigerSpool into your TigerTag account by scanning a QR code with your phone, and
**your printers appear on it by themselves** — names, addresses, access codes and
all. Add a printer to your account next month and TigerSpool picks it up on its
own.

That is the whole difference. Everything else in this project follows from it.

## What it is

A small box that sits next to your 3D printers.

<!-- TODO(images): product photo — docs/images/hero.jpg -->

1. Hold a filament spool carrying a [TigerTag](https://tigersystem.io) NFC chip
   against the box.
2. Tap the slot you want it in on the touchscreen.
3. The material, the brand, the exact colour and the temperatures land in your
   printer.

No typing a colour into a printer menu that offers you twelve of them. No
guessing whether slot 3 is the grey PETG or the grey ABS. The spool says what it
is, and the box tells the printer.

## Printer support

**"Any printer" is the goal, not a claim about today.** The honest, current state
of every brand is in **[docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md)**,
which grades each one on three levels — ✅ automatic, ⚙️ one setup step,
🧪 experimental. Read it before buying parts for a specific printer.

Four brands have a protocol implementation proven on real hardware in the
prototype: **Creality**, **FlashForge**, **Bambu Lab** and **Snapmaker**.
**Elegoo** and **Anycubic** are targeted for v1 and have **no implementation at
all yet** — their protocols still have to be reverse-engineered. The compatibility
matrix is the source of truth; this paragraph is a summary of it.

## Hardware

One board, one reader, six wires. The electronics are identical for every printer
brand — only the 3D-printed shell changes.

| Part | Notes |
|---|---|
| Waveshare ESP32-S3-Touch-LCD-2 | 2.0" 240×320 touchscreen, 16 MB flash, 8 MB PSRAM |
| PN532 NFC module | Wired in **HSU (UART)** mode — not I²C, not SPI |
| Six jumper wires | 3V3, GND, and four signal lines |
| A 3D-printed case | [models/](models/) — one per printer brand, plus a desktop stand |

Full parts list: **[hardware/BOM.md](hardware/BOM.md)** ·
Wiring: **[docs/WIRING.md](docs/WIRING.md)** and **[hardware/pinout.md](hardware/pinout.md)**

> The PN532 wiring is not negotiable and not obvious — the reader goes on
> **GPIO43/44**, and GPIO6/7 will silently corrupt every read. Follow
> [docs/WIRING.md](docs/WIRING.md) exactly.

## How you'll set one up

Written out screen by screen in **[docs/ONBOARDING.md](docs/ONBOARDING.md)**. The
short version:

| | |
|---|---|
| **1. Install** | Plug the board into a computer, open the web installer in Chrome or Edge, click Install. No toolchain, no command line. |
| **2. Wi-Fi** | The screen shows a QR code. Your phone joins the box's network in one tap, a page opens by itself, you pick your Wi-Fi and type its password **on your phone's keyboard**. |
| **3. Account** | The screen shows a second QR code. Scan it, approve on your phone, done. Your printers arrive. |
| **4. Scan** | Spool against the box, slot on the screen, sent. |

Nothing is ever typed on the 2.0" screen. There is no keyboard on this device and
there will never be one.

## Documentation

| Document | What's in it |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layers, state machine, printer backend abstraction, transport/protocol split |
| [docs/ONBOARDING.md](docs/ONBOARDING.md) | The user's journey, screen by screen |
| [docs/WIFI-PROVISIONING.md](docs/WIFI-PROVISIONING.md) | Built-in captive portal and the Wi-Fi QR code |
| [docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md) | Signing in with no keyboard — QR pairing and email/password |
| [docs/OTA.md](docs/OTA.md) | Over-the-air updates, partition layout, rollback, release channels |
| [docs/WIRING.md](docs/WIRING.md) | ESP32-S3 ↔ PN532 HSU, complete pinout, what breaks and why |
| [docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md) | Honest three-level support matrix, per brand |
| [docs/MIGRATION.md](docs/MIGRATION.md) | How the bench prototype becomes this firmware |
| [hardware/BOM.md](hardware/BOM.md) | Parts and where to buy them |
| [models/README.md](models/README.md) | 3D-printable cases, and the rule that governs them |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to help |

## Where TigerSpool sits in the ecosystem

```
    ┌──────────────────────┐        you add your printers here, once,
    │     Tiger Studio     │        on a computer with a keyboard
    │  (desktop, MIT)      │
    └──────────┬───────────┘
               │  writes users/{uid}/printers/*
               ▼
    ┌──────────────────────┐
    │   TigerTag account   │        the shared source of truth
    └──────────┬───────────┘
               │  TigerSpool reads it after QR pairing
               ▼
    ┌──────────────────────┐        ┌───────────────────────────┐
    │   TigerSpool RFID    │───────▶│  your printers on the LAN │
    │  (this repository)   │        │  Creality · FlashForge ·  │
    └──────────▲───────────┘        │  Bambu Lab · Snapmaker    │
               │                    └───────────────────────────┘
        NFC    │
    ┌──────────┴───────────┐
    │  a TigerTag'd spool  │        material, brand, colour, temperatures
    └──────────────────────┘
```

Related projects: **[TigerTag-RFID-Guide](https://github.com/TigerTag-Project/TigerTag-RFID-Guide)**
(the tag protocol) · **[Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)**
(desktop printer manager) · **[TigerScale V3](https://github.com/TigerTag-Project/Tiger-Scale-V3)**
(the connected filament scale) · **[TigerSystem-Docs](https://github.com/TigerTag-Project/TigerSystem-Docs)**
(ecosystem source of truth).

## Contributing

The firmware isn't here yet, so the most useful contributions right now are
**not code**:

- **Own an Elegoo or an Anycubic?** Their network protocols are the single
  biggest unknown in this project. A packet capture of the vendor slicer setting
  a filament slot would unblock both. See
  [docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md).
- **Read the architecture** and tell us where it's wrong —
  [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
- **Design a case** for a printer that doesn't have one — [models/](models/).

[CONTRIBUTING.md](CONTRIBUTING.md) has the details. Talk to us on
[Discord](https://discord.gg/3Qv5TSqnJH).

## Credits

The bench prototype that this project is built on — the state machine, the four
printer protocols, and every hard-won hardware fact in
[docs/WIRING.md](docs/WIRING.md) — was written together with
**[RP3D-S](https://github.com/RP3D-S)**. See [AUTHORS.md](AUTHORS.md).

## License

MIT — see [LICENSE](LICENSE). The code is free.

The **TigerTag** and **TigerSpool** names have conditions attached: see
[TRADEMARK.md](TRADEMARK.md). Short version — build and sell them freely, run the
official firmware, keep the name. Third-party dependencies keep their own
licenses: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

<p align="center">
  Built by the <a href="https://tigertag.io">TigerTag</a> community.
</p>
