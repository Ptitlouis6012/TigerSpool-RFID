# Security policy

## Reporting a vulnerability

Report privately, not in a public issue: use
[GitHub's private vulnerability reporting](https://github.com/TigerTag-Project/TigerSpool-RFID/security/advisories/new) on this
repository, or reach a maintainer on [Discord](https://discord.gg/3Qv5TSqnJH).

We will acknowledge within a few days and keep you updated. Please give us
reasonable time to ship a fix before disclosing.

## Scope

TigerSpool handles credentials that matter: Wi-Fi passwords, printer access
codes, and a long-lived token to the owner's TigerTag account. It is a device on
a home network that can reconfigure printers.

In scope: the firmware, the configuration web UI, the pairing flow, the OTA
mechanism, and this repository's build and release automation.

Out of scope here: the TigerTag cloud backend, Tiger Studio, and printer vendors'
own firmware. Report those to their own projects.

## Rules the firmware follows

**No credentials in the repository or in a build.** Not Wi-Fi credentials, not
printer access codes, not API keys with any authority. Everything is supplied at
runtime and stored in NVS. The published binary is byte-identical for every user.

**The account refresh token is stored in encrypted NVS.** It is a long-lived
credential to the owner's account and must not be recoverable by dumping flash.

**TLS certificates are validated.** A pinned root CA bundle, with a plan for
rotating it before it expires.

> **Known gap:** the bench prototype disables certificate verification on every
> HTTPS call. That was acceptable for bench work and is **not acceptable
> shipped**. It is a release blocker, tracked in
> [docs/MIGRATION.md](docs/MIGRATION.md).

**Firmware updates are signature-verified before boot.** A checksum proves the
download was not corrupted; it proves nothing about who produced it. Without a
signature, anyone who can answer for the update host can install arbitrary
firmware on every device in the field. See [docs/OTA.md](docs/OTA.md).

**The setup access point is temporary.** It exists only while the device has no
known-good network, and it goes down as soon as it does.

**Signing out revokes.** "Forget this device" clears the local session *and* is
reflected in the account, so a lost or sold device can be cut off from a phone.
A local-only wipe is not a revocation.

**The device never signs in to a printer vendor's cloud.** Where a vendor session
is required, Tiger Studio obtains it and the account carries it. The firmware
only consumes it.

## Known accepted risks

**The setup access point is open, with no password.** A password would have to be
displayed on the device screen and typed on a phone — exactly the friction the
design exists to remove. The AP is short-lived, carries nothing but a
provisioning form, and disappears once the device is on a network. Anyone within
Wi-Fi range during those minutes could join it. We consider that acceptable;
it is a deliberate trade, not an oversight.

**Printer protocols are unauthenticated or weakly authenticated by design.**
Moonraker has no authentication at all on the LAN. This is a property of the
printers, not of TigerSpool, but it means anyone on your network can already
reconfigure them.

## Supported versions

The project has not released yet. Once it has, security fixes go to the latest
release; there are no long-term support branches.
