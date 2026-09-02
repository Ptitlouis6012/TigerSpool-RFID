# Printer compatibility

**"Any printer" is the ambition. This page is the truth.**

Marketing gets one line on the front page; this table gets the rest. If a printer
is not on it, TigerSpool does not support it. If it is on it at 🧪, that means
someone has to do work before it does.

> **Status of this page:** the firmware has not been migrated to this repository
> yet. Everything marked 🟢 *proven* below was proven **in the bench prototype**,
> on real printers, and is being carried over. Nothing here is claimed on the
> basis of documentation alone.

---

## The three levels

| | Level | What it means for you |
|---|---|---|
| ✅ | **Automatic** | Add the printer once in Tiger Studio. TigerSpool finds it, connects, and works. Nothing to configure on the device. |
| ⚙️ | **One setup step** | Works, but the printer needs one thing switched on or read off its own screen — usually LAN mode, sometimes an access code. Once. |
| 🧪 | **Experimental** | Not finished. May be partly implemented, may be unimplemented, may be broken by a printer firmware update. Do not buy hardware for this. |

---

## Support matrix

| Brand | Models | Level | Transport | Prototype status | What's needed |
|---|---|---|---|---|---|
| **Creality** | K2, K2 Plus | ⚙️ | WebSocket `:9999` | 🟢 proven on hardware | **LAN mode must be on** in the printer's network settings, or port 9999 refuses connections. |
| **FlashForge** | Creator 5 / 5 Pro | ⚙️ | HTTP `:8898` | 🟢 proven on hardware | Serial number **and check code**, both from the printer's network info screen. Imported automatically if the printer is in your TigerTag account. |
| **Bambu Lab** | A1, A1 mini, A2L | ⚙️ | MQTT/TLS `:8883` | 🟢 proven on hardware | **LAN mode on**, plus the serial and the 8-character access code from the printer screen. Imported automatically from your account. |
| **Snapmaker** | Artisan, J1, J1s, U1 | ⚙️ | Moonraker WebSocket `:7125` | 🟢 proven on hardware | Nothing — Moonraker needs no authentication on the LAN. The printer's IP is all it takes. |
| **Bambu Lab (cloud)** | X1, P1, and any printer not on your LAN | 🧪 | MQTT/TLS to Bambu's broker | 🟡 partly implemented | Depends on a Bambu session token that **Tiger Studio** obtains and stores in your account; the device never signs in to Bambu itself. The token expires roughly every three months and has to be renewed in Studio. Not a finished experience. |
| **Elegoo** | — | 🧪 | unknown | 🔴 **nothing implemented** | The protocol has not been investigated. See below. |
| **Anycubic** | — | 🧪 | unknown | 🔴 **nothing implemented** | The protocol has not been investigated. See below. |

**Legend for prototype status:** 🟢 implemented and verified against a physical
printer · 🟡 implemented, not fully verified · 🔴 not implemented.

---

## Elegoo and Anycubic

Both are **targeted for v1** and **neither has a single line of code today**.
This is the largest open risk in the project, and it is stated plainly rather
than buried.

Nothing is known yet about how either brand exposes filament slot configuration
over the network — whether there is a LAN API at all, whether it is authenticated,
whether it can be written to rather than only read.

**What would unblock them,** in descending order of usefulness:

1. **A packet capture** of the vendor's own slicer or app setting a filament type
   and colour on a slot. That single capture is usually enough to identify the
   transport, the authentication and the command shape.
2. **Existing reverse-engineering work.** Both brands have active communities;
   published protocol notes would save weeks.
3. **Physical access to a printer** for iteration once a first guess exists.

If you own one of these printers and can help, open an issue. It is the most
valuable contribution available to this project right now.

---

## What "supported" does and does not mean

**It means:** TigerSpool can write a material name, a colour and nozzle/bed
temperatures into a numbered slot, and can read back what is currently in each
slot to show it on screen.

**It does not mean:**

- **Loading or unloading filament.** TigerSpool sets metadata. It does not drive
  the extruder. No supported printer exposes load/unload the way it exposes
  slot configuration.
- **Any colour you like.** FlashForge accepts only its own 24-colour palette;
  a tag's colour is snapped to the nearest one. Bambu Lab accepts exact RGB.
- **Any material name you like.** Each printer has a fixed vocabulary. Materials
  the printer does not know are mapped to the closest one it does.
- **Working during a print.** Changing slot configuration mid-print is not
  supported and may be refused by the printer.

---

## Why a printer that "should work" may not

**Printer firmware updates.** Every protocol here except Snapmaker's is
reverse-engineered from a vendor's private API. Vendors change them without
notice and owe nobody compatibility. A printer that worked last month can stop.

**LAN mode.** Creality, FlashForge and Bambu Lab all gate their local APIs behind
a mode that is off by default. This is the single most common reason a correctly
configured printer shows as offline.

**Network segmentation.** Guest networks, VLANs and client isolation on the
access point will all prevent TigerSpool from reaching a printer that is
otherwise perfectly configured. They must be on the same network segment.

**Silent acknowledgements.** At least one printer answers "success" to commands it
did not understand. Where the protocol allows it, TigerSpool re-reads slot state
after writing to confirm the change actually landed — but confirmation is not
possible on every protocol.

---

## Adding a printer brand

The [architecture](ARCHITECTURE.md#printer-abstraction) is built so that a new
brand is one class implementing one interface, and touches nothing else. What
that class has to provide:

1. **Connect and stay connected** over whatever transport the printer speaks,
   recovering on its own when the network drops.
2. **Report slots** — how many, what they are called, and what is currently in
   each one.
3. **Accept an assignment** — translate a decoded TigerTag into the printer's own
   material vocabulary and colour constraints, and send it.
4. **Tell the truth about failure** — including detecting the case where the
   printer claims success and does nothing.

See [CONTRIBUTING.md](../CONTRIBUTING.md).
