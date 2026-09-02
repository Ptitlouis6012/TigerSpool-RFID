# Bill of materials

Everything needed to build one TigerSpool. **The electronics are identical for
every printer brand** — only the printed shell changes ([models/](../models/)).

> **Prices are indicative** and were not re-checked at the time of writing.
> No vendor links are given yet: they will be added once a reference build is
> assembled and verified. **TODO — to confirm with Benoit** whether affiliate
> links are used here, as they are in [Tiger-Scale](https://github.com/TigerTag-Project/Tiger-Scale/blob/main/hardware/BOM.md).

## Electronics

| Part | Why this one | ~Price |
|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-2** | 2.0" 240×320 IPS with CST816S capacitive touch, ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM. The screen, the touch panel and the MCU are one board — no separate display wiring. 16 MB is what makes two OTA slots comfortable. | ~25 € |
| **PN532 NFC module** (red V3 breakout) | Reads NTAG21x. Must support **HSU/UART** mode — the DIP-switch variants do. | ~6 € |
| **6 × Dupont jumper wires**, female–female | 3V3, GND, TX, RX, RST, and one spare | ~2 € |
| **USB-C cable and a 5 V supply** | Powers the board; also used for the first flash | ~5 € |
| **3D-printed case** | [models/](../models/) | ~30 g of filament |

**Indicative total: ~40 €** plus filament.

## What is deliberately not on this list

**No separate display, no microcontroller board, no touch controller.** The
Waveshare board is all three. It costs more than a bare ESP32-S3 and removes the
entire class of display wiring problems, which on a product aimed at
non-technical users is the right trade.

**No level shifters.** The PN532 runs at 3V3 — the same as the ESP32-S3. Do not
power it from 5 V. See [../docs/WIRING.md](../docs/WIRING.md).

**No battery.** TigerSpool sits next to a printer that is already plugged in.

## The tags

TigerSpool reads **TigerTag** NFC chips, which are not included:

- **TigerTag store** — <https://tigertag.io>
- Filament from partner brands ships with them already attached.

The tag protocol is open and documented:
[TigerTag-RFID-Guide](https://github.com/TigerTag-Project/TigerTag-RFID-Guide).

## Board variants — read this before ordering

**Waveshare sells several similar boards.** `ESP32-S3-Touch-LCD-2` is a 2.0"
240×320 panel. The 1.28", 1.69" and 3.5" boards in the same family have different
panel controllers and different pin assignments, and this firmware will not run
on them correctly. Check the silkscreen, not the listing title.

**PN532 clones vary.** The DIP switch mode table is silkscreened on the module
itself and the mapping is not always the same between clones. Read your own
module rather than trusting a table found online — including the one in
[../docs/WIRING.md](../docs/WIRING.md).

## Assembly

1. Wire the PN532 to the board exactly as in [../docs/WIRING.md](../docs/WIRING.md).
   **Six wires, two of them crossed.**
2. Set both DIP switches on the PN532 to `0` / OFF.
3. Flash the firmware from the browser — [../installer/](../installer/).
4. Verify the reader responds *before* closing the case: bring-up checklist in
   [../docs/WIRING.md](../docs/WIRING.md#bringing-a-new-build-up).
5. Mount the reader antenna away from the display's metal back and any ground
   plane. Read range is only 2–4 cm and both will eat it.
6. Close the case.

<!-- TODO(images): assembled build photo — docs/images/ -->
