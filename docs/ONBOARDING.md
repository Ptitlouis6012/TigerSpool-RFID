# Onboarding

The user's journey, screen by screen, from a sealed box to a scanned spool.

**The target:** a person who has never flashed a microcontroller gets to their
first successful scan without reading anything, without a keyboard on the device,
and without knowing what an IP address is.

---

## Before the box

The user has already done one thing on a computer: added their printers to
**[Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)**.
That is where a keyboard exists and where typing an access code is reasonable.

Everything below assumes that. TigerSpool's job is to make the rest require no
typing at all.

---

## 1 · Install the firmware

**Browser, USB cable, one click.** The web installer at
`https://tigertag-project.github.io/TigerSpool-RFID/` uses ESP Web Tools in
Chrome or Edge: plug the board in, pick the serial port, click Install.

No PlatformIO. No Arduino IDE. No command line. See [installer/](../installer/).

*(Anyone who already owns an assembled TigerSpool skips this entirely — it ships
flashed and updates itself. See [OTA.md](OTA.md).)*

---

## 2 · First boot — language

```
        ┌────────────────────────────┐
        │   Choose your language     │
        │                            │
        │   ┌──────────────────┐     │
        │   │     English      │     │
        │   ├──────────────────┤     │
        │   │     Français     │     │
        │   ├──────────────────┤     │
        │   │     Deutsch      │     │
        │   ├──────────────────┤     │
        │   │     Español      │     │
        │   └──────────────────┘     │
        │          ●  ○              │
        └────────────────────────────┘
```

Asked once, remembered. It comes first because every screen after it is written
in the language chosen here — including the error messages.

---

## 3 · Wi-Fi

```
        ┌────────────────────────────┐
        │  Wi-Fi setup               │
        │                            │
        │      ███  ██ ████ ██       │
        │      █ ██████  ████        │
        │      ████ ██ ███  █        │
        │      ██  ████ █████        │
        │                            │
        │  Scan with your phone      │
        │  camera, or join           │
        │                            │
        │   TigerSpool-Setup-XXXX    │
        │                            │
        │  Waiting for a phone…      │
        └────────────────────────────┘
```

1. The user points their phone camera at the QR. It offers *"Join TigerSpool-Setup-XXXX"*.
2. One tap and the phone is on the box's network.
3. A page opens by itself, listing every Wi-Fi network the box can see.
4. The user taps theirs and types the password **on their phone**.
5. The box reboots onto the network.

Full behaviour, including what happens when the password is wrong:
[WIFI-PROVISIONING.md](WIFI-PROVISIONING.md).

---

## 4 · TigerTag account

```
        ┌────────────────────────────┐
        │  Link your account         │
        │                            │
        │      ███ █  ████ ███       │
        │      █ ████████  ██        │
        │      ███ ██  ███████       │
        │      ██ ████ █  ███        │
        │                            │
        │       K7QF-3M2P            │
        │                            │
        │  Scan to link, or go to    │
        │  tigersystem.io/pair       │
        │                            │
        │  Waiting…            9:47  │
        └────────────────────────────┘
```

1. The user scans the QR with their phone.
2. A page opens naming **this box** and asks them to approve.
3. They approve — usually already signed in, so it is one tap.
4. The box is linked.

The short code is printed under the QR for a phone that will not scan, and it is
readable aloud over the phone if someone is helping remotely.

Users whose account has a password can instead sign in from the box's web page at
`http://tigerspool-xxxx.local`, typing on their phone. Both paths:
[ACCOUNT-PAIRING.md](ACCOUNT-PAIRING.md).

---

## 5 · Printers appear

```
        ┌────────────────────────────┐
        │  Printers                  │
        │                            │
        │   ┌──────────────────┐     │
        │   │ Bambu A1     ● │ ●│     │  green = reachable
        │   ├──────────────────┤     │
        │   │ K2 Plus      ● │  │     │
        │   ├──────────────────┤     │
        │   │ Creator 5    ○ │  │     │  grey = offline
        │   └──────────────────┘     │
        │          ●  ○              │
        │  benoit@example.com        │
        └────────────────────────────┘
```

**Nothing was typed.** The printers came from the account, with their addresses
and access codes. This is the moment the product justifies itself, and it should
feel like it — the list populating on its own is the demo.

A printer showing offline gets a reason, not a grey dot alone: *"Turn on LAN mode
in the printer's network settings"* is the answer more than half the time. See
[PRINTER-COMPATIBILITY.md](PRINTER-COMPATIBILITY.md).

---

## 6 · The everyday loop

This is what a user does a hundred times. Everything above happens once.

### Pick a slot

```
        ┌────────────────────────────┐
        │ ‹ back   Bambu A1     ● ●  │
        │                            │
        │    ╭────╮      ╭────╮      │
        │    │ ●  │      │ ●  │      │   the circle is the
        │    │ A  │      │ B  │      │   colour that's loaded
        │    │PETG│      │PLA │      │
        │    ╰────╯      ╰────╯      │
        │    ╭────╮      ╭────╮      │
        │    │ ○  │      │ ●  │      │
        │    │ C  │      │ D  │      │
        │    │ –– │      │ABS │      │
        │    ╰────╯      ╰────╯      │
        │     tap a slot             │
        └────────────────────────────┘
```

Live from the printer: the actual colours and materials currently in each slot.
A user recognises their own spools by colour without reading a word.

### Present the spool

```
        ┌────────────────────────────┐
        │ ‹ back   Slot C            │
        │                            │
        │         ·  ·  ·            │
        │      ·           ·         │
        │    ·    ╭─────╮    ·       │
        │    ·    │     │    ·       │
        │    ·    ╰─────╯    ·       │
        │      ·           ·         │
        │         ·  ·  ·            │
        │                            │
        │   Hold the spool           │
        │   against the box          │
        │                            │
        │      [    Cancel    ]      │
        └────────────────────────────┘
```

Range is 2–4 cm ([WIRING.md](WIRING.md)), so the case has to make the right
distance the natural one. Failures name a cause: *"Move the spool closer"*, not
*"read error"*.

### Confirm

```
        ┌────────────────────────────┐
        │ ‹ back   → Slot C          │
        │  ┌──────────────────────┐  │
        │  │                      │  │   the actual colour,
        │  │                      │  │   big
        │  └──────────────────────┘  │
        │          PETG              │
        │        Polymaker           │
        │         #1A5C0C            │
        │      Nozzle 230–250 °C     │
        │      Bed      70–80 °C     │
        │                            │
        │   [   No   ] [   Send   ]  │
        └────────────────────────────┘
```

One confirmation, because writing to the wrong slot is annoying to undo. The
colour is shown as a large swatch rather than a hex code — that is what the user
actually recognises.

### Done

```
        ┌────────────────────────────┐
        │                            │
        │            ✓               │
        │                            │
        │        Slot C updated      │
        │                            │
        │        PETG · Polymaker    │
        │                            │
        │      tap to continue       │
        └────────────────────────────┘
```

Back to the grid after a couple of seconds, with slot C now showing the new
colour — read back from the printer, not assumed. On some printers a command is
acknowledged and ignored ([PRINTER-COMPATIBILITY.md](PRINTER-COMPATIBILITY.md)),
and a success screen that lied is worse than an error.

**When the printer changed what it was given, say so.** Several printers accept
only their own fixed palette and material list, so the colour that lands may not
be the colour on the spool — verified on a FlashForge, where `#DC123F`
became `#F82D29` and "PLA High Speed" became "PLA". That is the printer's limit,
not a fault, but a result screen that reports plain success teaches the user to
distrust the device the first time they walk over and look at it. A line — *"colour
adapted to the printer's palette"* — costs nothing and prevents a bug report.

---

## Principles

**Nothing is typed on the device.** No keyboard, no character picker, no
scroll-wheel text entry. Every value either comes from the account, from a tag,
or from the phone.

**Every error names a cause and an action.** "Printer offline" is a status.
"Printer offline — check LAN mode is on in its network settings" is a message.

**The screen never freezes.** Anything slow gets progress or runs in the
background. A still 2.0" screen reads as a crash.

**The scan loop is three taps and never more.** Slot, spool, send. Everything
else is setup, and setup happens once.

**It recovers on its own.** Router reboots, printers moving to new addresses,
brief outages — none of these should send a user back to setup.

---

## Not designed yet

- **Factory reset from the device.** Needed before anyone sells or gives one away.
  What it wipes, and how it is protected against a curious tap, is undecided.
- **Multiple people, one box.** A shared box in a workshop is linked to exactly
  one account today.
- **What a blank or non-TigerTag tag should say.** It is a common case and
  deserves a real message, not a read failure.
