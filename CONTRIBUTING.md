# Contributing

Thanks for being here. TigerSpool is early — the design is settled, the firmware
is not written yet — so the most valuable contributions right now are not code.

Talk to us on **[Discord](https://discord.gg/3Qv5TSqnJH)**.

---

## What would help most, in order

### 1. Elegoo and Anycubic protocols

**The largest unknown in the project.** Both are targeted for v1 and neither has
any implementation. Nothing is known yet about how either brand exposes filament
slot configuration over the network.

Both printers are on the maintainer's bench, so this is scheduled work rather
than a search for hardware. What would still shorten it:

- **Existing reverse-engineering notes** for either brand.
- **Captures from models we do not have** — a protocol that holds across a
  brand's range is worth much more than one that fits a single printer.
- **Testing** once a first implementation exists.

See [docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md).

### 2. Read the architecture and argue with it

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) describes firmware that has not been
written. That is the cheapest possible moment to find out it is wrong. Open an
issue if something does not hold up.

### 3. Design a case

[models/](models/) has the rules and empty directories. The electronics are
identical for every model — only the shell changes. A case that has been printed
and used beats one that has been designed.

### 4. Test on a printer we support

Once there is firmware. A report saying "Creality K2, firmware 1.1.3.13, works"
or "stopped working after the September update" is worth a lot: every protocol
here except Snapmaker's is reverse-engineered and vendors change them without
notice.

---

## Firmware contributions

The firmware is not migrated yet. Until it is, please open an issue before
writing code — you would be building against a tree that is about to arrive.

When it is here:

**Read [docs/MIGRATION.md](docs/MIGRATION.md) first.** It lists the hardware facts
that must not be lost. Most of them look like arbitrary constants.

**Rules that are not negotiable:**

- **No credentials in the repository.** Not Wi-Fi, not access codes, not API keys
  with authority. Everything comes from NVS at runtime. See [SECURITY.md](SECURITY.md).
- **English everywhere** — comments, identifiers, log messages, commits.
  User-facing text goes through i18n.
- **No text entry on the device.** There is no keyboard and there will not be one.
  A feature that needs typing needs a phone. See [docs/ONBOARDING.md](docs/ONBOARDING.md).
- **A hardware constant gets a comment saying what breaks without it.**
- **One copy of a vendored library**, with its patches documented.

**A new printer brand** is one class implementing `PrinterBackend` plus a
registration line. It touches nothing else. If your change needs to modify the UI
or the state machine, say so in the PR — it may mean the interface is wrong,
which is worth knowing.

---

## Pull requests

1. Fork, branch from `main`.
2. One concern per PR. A rename and a behaviour change in one diff cannot be
   reviewed properly.
3. Say **what you tested it on**. For firmware, that means a real board and,
   for a backend, a real printer with its model and firmware version.
4. Keep it building. CI runs on every PR.

Commits: a short imperative subject line, and a body that says *why* when the
reason is not obvious from the diff.

---

## Issues

**Bugs** — the templates ask for board, firmware version, printer model and
printer firmware version. All four matter: most failures are one of a bad PN532
link, a vendor protocol change, or LAN mode being off.

**Before opening a reader bug**, walk the bring-up checklist in
[docs/WIRING.md](docs/WIRING.md#bringing-a-new-build-up). It resolves most of them,
and it tells us something either way.

**Printer support requests** — say which brand, which model, and whether you can
capture traffic. Without a capture or existing protocol notes, a request is a
wish rather than something anyone can act on.

---

## Translations

The firmware targets the same nine languages as
[TigerScale V3](https://github.com/TigerTag-Project/Tiger-Scale-V3). Once the
locale files exist, translating is a matter of editing JSON — no build tools
needed, and it is a genuinely useful first contribution.

Two constraints: the device screen is 240 px wide, so a string that is twice as
long in your language will be cut off; and the fonts are generated from the
character set actually used, so a new script means regenerating them.

---

## Licensing

Contributions are MIT-licensed like the rest of the repository
([LICENSE](LICENSE)). Do not contribute code, geometry or documentation you do
not have the right to relicense — including anything decompiled from a vendor's
software or derived from proprietary CAD.

The **TigerTag** and **TigerSpool** names have conditions:
[TRADEMARK.md](TRADEMARK.md). Forks are welcome under a different name.

---

## Code of conduct

[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Be decent to each other.
