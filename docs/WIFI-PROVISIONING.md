# Wi-Fi provisioning

Getting a device with no keyboard onto a network whose password is 20 random
characters.

---

## The rule

**One firmware. The setup portal is a state, never a separate binary to flash.**

If a user has to flash a "setup firmware", configure it, then flash the real one,
the product has failed before it started. TigerSpool's captive portal is compiled
into the same firmware and is entered automatically.

---

## When the portal opens

The portal is entered when the device cannot get onto a network, which covers
every case that matters:

- **First boot** — NVS has no credentials at all.
- **The saved network is gone** — moved house, new router, changed password.
- **Connection times out** — 30 seconds of trying, then the portal opens rather
  than retrying forever against a network that will never answer.
- **The user asks for it** — a "Change Wi-Fi" action on the settings screen,
  because a user who is switching networks deliberately should not have to break
  the old one first.

There is no button combination to hold at boot, and no serial command. Those are
developer affordances; the device has to recover on its own.

---

## What the user sees

### On the device screen

```
        ┌────────────────────────────┐
        │  Wi-Fi setup               │
        │                            │
        │      ███  ██ ████ ██       │
        │      █ ██████  ████        │   a QR code that joins
        │      ████ ██ ███  █        │   the setup network
        │      ██  ████ █████        │
        │                            │
        │  Scan with your phone      │
        │  camera, or join           │
        │                            │
        │      TigerSpool-Setup      │
        │                            │
        │  Waiting for a phone…      │
        └────────────────────────────┘
```

### The QR code

The screen shows a **standard Wi-Fi join QR**, which iOS and Android cameras
recognise natively:

```
WIFI:S:TigerSpool-Setup;T:nopass;;
```

The phone offers "Join network TigerSpool-Setup" as a tap. No app, no typing an
SSID, no hunting through a Wi-Fi settings list for a name the user has to read
off a small screen first.

The access point is **open**. That is a deliberate trade: a password on the setup
AP would have to be shown on screen and typed on the phone, which is exactly the
friction being removed, and the AP exists for two minutes on a network with
nothing on it but a form. What *is* required is that the AP stop existing once
provisioning succeeds.

### On the phone

Joining the network triggers the captive portal automatically — iOS, Android and
Windows each probe a known URL after associating, and the device answers all of
them so the setup page opens by itself rather than waiting for the user to guess
a URL.

The page shows:

- **The networks it can see**, strongest first, deduplicated, with a lock icon on
  the protected ones. The user picks from a list rather than typing an SSID.
- **A password field** — filled in with the **phone's** keyboard, with the phone's
  password manager available.
- **A rescan button**, because a network that was not there ten seconds ago
  sometimes is.
- **Language** matching the device's.

On save, the device stores the credentials and reboots into the network.

---

## Behaviour that has to be right

**Scanning requires the station interface.** Listing networks while running an
access point means putting the radio into combined AP+STA mode for the scan, then
returning it to AP-only so the phone's connection stays stable. Getting this
wrong drops the phone off the setup network mid-form.

**A wrong password must say so.** Saving credentials and rebooting into a failed
connection, which reopens the portal with no explanation, is a loop the user
cannot escape or understand. The device should verify the association before
committing, and if it cannot, the portal must reopen with *"Couldn't connect to
`<network>` — the password may be wrong"* rather than a blank form.

**The portal must not be a permanent access point.** Once the device is on a
network, the AP is down and the configuration UI lives at `http://tigerspool.local`
on the LAN instead.

**Reconnection is automatic and silent.** A router reboot, a brief outage, or a
device that wakes before the access point does must not drop the user back into
provisioning. The portal is for *no known-good network*, not for *not connected
right now*.

**Errors are written for someone who does not know what an SSID is.** "Association
failed" is not a message. "Couldn't join that network — check the password" is.

---

## After provisioning

The configuration UI stays reachable on the local network at
**`http://tigerspool.local`** (mDNS) or the device's IP address, which the device
shows on screen. From there a user can change networks, correct an imported
printer, sign in to their TigerTag account with an email and password, or reset
the device.

The same page is served in both modes. One form, two entry points — the AP portal
and the LAN address — so there is one implementation to keep correct.

---

## Identifiers

| | Value |
|---|---|
| Setup AP SSID | `TigerSpool-Setup` |
| AP address | `192.168.4.1` |
| mDNS hostname | `tigerspool.local` |
| NVS namespace | `tigerspool` |

The prototype used `K2-TigerTag-XXXX` / `TigerTag-Setup` and `tigertag.local`;
those names belong to a Creality-only ancestor and are replaced. See
[MIGRATION.md](MIGRATION.md).

---

## Open questions

- **Should the setup SSID carry a per-device suffix** (`TigerSpool-Setup-A1B2`)?
  A bare name is friendlier to read and put in a QR; a suffix disambiguates two
  devices being set up in the same room. Undecided.
- **Pre-connection validation.** Confirming a password before committing it and
  rebooting is clearly better UX; whether the association can be tested reliably
  while the AP is still up, without dropping the phone, needs bench work.
