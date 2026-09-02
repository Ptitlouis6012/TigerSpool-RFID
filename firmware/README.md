# firmware/

> **Empty on purpose.** The directory layout below is the target described in
> [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md). No source code has been
> migrated yet — see [../docs/MIGRATION.md](../docs/MIGRATION.md) for the plan and
> its order of work.
>
> **Nothing here builds today.**

## Target layout

```
firmware/
├── platformio.ini          env: tigerspool
├── partitions.csv          two OTA slots — docs/OTA.md
├── include/                board headers, build-time configuration
├── lib/
│   └── PN532/              vendored reader driver — ONE copy, patches documented
├── data/                   web config UI, uploaded to LittleFS
└── src/
    ├── main.cpp            setup(), loop(), nothing else
    ├── app/                state machine, orchestration
    ├── ui/                 screens, rendering, touch handling
    ├── reader/             PN532 use, TigerTag decoding
    ├── printers/
    │   ├── backend.h       the PrinterBackend interface
    │   ├── registry.*      brand → backend, no switch in the app
    │   ├── creality/
    │   ├── flashforge/
    │   ├── bambulab/
    │   └── snapmaker/
    ├── account/            pairing, session, printer import
    ├── net/                Wi-Fi, captive portal, web config, mDNS, OTA
    └── platform/           NVS, display, touch, logging
```

## Rules for this tree

**No credentials, ever.** Not Wi-Fi, not printer access codes, not API keys with
authority. Everything comes from NVS at runtime. The published binary is the same
for every user. See [../SECURITY.md](../SECURITY.md).

**One copy of the vendored library.** The prototype carries three, which have
already drifted into different behaviour —
[../docs/MIGRATION.md](../docs/MIGRATION.md#the-vendored-pn532-library). Patches
to a vendored library carry a comment saying what breaks without them.

**English only.** Comments, identifiers, log messages, commit messages.
User-facing strings go through the i18n layer.

**Hardware facts get a comment explaining why.** Every constant that looks
arbitrary — a delay, a page range, a retry count — is arbitrary-looking because
it was expensive to find. Say what breaks if it changes.

## Build (once there is something to build)

```bash
pio run -e tigerspool                 # compile
pio run -e tigerspool -t upload       # flash over USB
pio run -e tigerspool -t uploadfs     # web UI to LittleFS
pio device monitor -b 115200          # serial console
```

Most users will never do this — they use the browser installer at
[../installer/](../installer/).
