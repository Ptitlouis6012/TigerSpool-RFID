#!/usr/bin/env bash
# Build and flash the firmware over USB.
#
#   bash scripts/flash.sh                 build, then flash
#   bash scripts/flash.sh --monitor       ... and open the serial console
#   bash scripts/flash.sh --port /dev/... when auto-detection picks the wrong one
#   bash scripts/flash.sh --fs            also upload the LittleFS image
#   bash scripts/flash.sh --erase         wipe the whole chip first - see below
#
# Everything runs through PlatformIO, with no hard-coded toolchain paths, so it
# behaves the same on macOS, Linux and Git Bash.
#
# WHAT GETS WRITTEN, AND WHY YOUR WI-FI SURVIVES
#
# An ordinary flash writes three images at three offsets:
#
#     0x0000    bootloader.bin
#     0x8000    partitions.bin
#     0x10000   firmware.bin       <- the app slot, ota_0
#
# The NVS partition lives at 0x9000 and is 0x5000 long. Nothing above touches
# it, which is why the saved Wi-Fi credentials, the TigerTag session and the
# imported printers are all still there after you reflash. That is not luck; it
# is the partition layout doing its job.
#
# --erase removes that guarantee on purpose: it wipes the entire chip, NVS
# included, so the device comes up as if it had never been configured. Use it to
# reproduce a first-boot experience, and expect to provision the board again.
#
# The same arithmetic is the reason CLAUDE.md forbids writing a merged factory
# image at 0x0000 on a provisioned device: a merged image spans from zero and
# therefore covers 0x9000, wiping the user's setup with no warning and no undo.

set -euo pipefail
cd "$(dirname "$0")/.."

ENV=tigerspool
PORT=""
MONITOR=0
FS=0
ERASE=0

while [ $# -gt 0 ]; do
  case "$1" in
    --port)    PORT="${2:?--port needs a device path}"; shift 2 ;;
    --monitor) MONITOR=1; shift ;;
    --fs)      FS=1; shift ;;
    --erase)   ERASE=1; shift ;;
    -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
    *) echo "unknown option: $1"; sed -n '2,10p' "$0"; exit 2 ;;
  esac
done

command -v pio >/dev/null 2>&1 || {
  echo "error: pio not on PATH. Install PlatformIO, or use its own shim:"
  echo "       ~/.platformio/penv/bin/pio"
  exit 2
}

cd firmware
PORT_ARG=()
[ -n "$PORT" ] && PORT_ARG=(--upload-port "$PORT")

if [ "$ERASE" = 1 ]; then
  echo "== erasing the whole chip - NVS included, so Wi-Fi and the account go too"
  pio run -e "$ENV" -t erase "${PORT_ARG[@]}"
fi

echo "== building and flashing $ENV"
pio run -e "$ENV" -t upload "${PORT_ARG[@]}"

if [ "$FS" = 1 ]; then
  echo "== uploading the LittleFS image"
  pio run -e "$ENV" -t uploadfs "${PORT_ARG[@]}"
fi

if [ "$MONITOR" = 1 ]; then
  echo "== serial console - ctrl-c twice to leave"
  pio device monitor -e "$ENV" "${PORT_ARG[@]/--upload-port/--port}"
fi
