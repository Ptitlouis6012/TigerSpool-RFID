#pragma once
#include <Arduino.h>

// Over-the-air firmware update.
//
// Two slots exist in partitions.csv precisely so this can work:
// Update.begin(U_FLASH) asks esp_ota_get_next_update_partition() for a spare app
// slot, and that call only ever returns an ota_N partition. A layout with a lone
// `factory` partition fails every attempt with the baffling "free=0".
//
// The published manifest is fetched, compared against the built-in version, and
// the image is streamed straight into the spare slot while its SHA-256 is
// computed on the way past. Nothing is written to the running slot, so a failure
// at any point leaves the device exactly as it was — the worst case is a wasted
// download.
//
// The connection is verified against the root CA store (net/tls.h), so the
// device knows who it is talking to. What it still does not know is who
// produced the image: that is a signature, and it is a separate decision with
// its own condition — see docs/OTA.md.
namespace ota {

enum State : uint8_t {
    IDLE,          // nothing happening
    CHECKING,      // fetching the manifest
    UP_TO_DATE,    // checked, nothing newer
    AVAILABLE,     // checked, a newer version is published
    DOWNLOADING,   // streaming into the spare slot
    DONE,          // written and verified; the device restarts next
    FAILED,        // see message()
};

// Call once at boot, after Wi-Fi. Confirms the running image so the bootloader
// stops treating it as a candidate.
void begin();

// Start a manifest check on its own task. Returns false if one is already
// running. Never blocks the UI: the check is a TLS handshake and can take
// seconds.
bool checkAsync();

// Start downloading the version the last check found. Returns false if there is
// nothing to install, or a job is already running.
bool applyAsync();

State state();
int  percent();              // 0-100 while DOWNLOADING
const char* latestVersion(); // "" until a check has succeeded
const char* message();       // a short reason when FAILED

// True when there is a spare app slot to write into. False means the partition
// table has no second slot, and no amount of retrying will help.
bool slotAvailable();

}  // namespace ota
