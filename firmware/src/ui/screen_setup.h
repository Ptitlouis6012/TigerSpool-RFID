#pragma once
#include <lvgl.h>

// The first-boot journey: language, then Wi-Fi, then the TigerTag account.
//
// It runs once, and it is the only part of the product a user meets before it
// works. Two rules shape all three screens:
//
//   Nothing is ever typed here. There is no keyboard on a 2.0" screen and there
//   will not be one. Text comes from the phone, through a QR code.
//
//   Every screen says what to do next in one sentence, in the language chosen on
//   the first one - which is why language comes first.
namespace screen_setup {

// ---- 1. Language ----------------------------------------------------------
// Nine locales, matching TigerScale. A scrolling list rather than a grid: the
// prototype fit four because its font was ASCII-only and it paginated, and a
// user hunting for their language across pages is a bad first impression.
void showLanguage(bool force = false);
int  takeLanguage();          // index into the locale table, or -1

// ---- 2. Wi-Fi -------------------------------------------------------------
// A standard Wi-Fi join QR: the phone camera offers "Join TigerSpool-Setup" as
// a tap, the captive portal opens by itself, and the password is typed on the
// phone with its own password manager available.
//
// Wi-Fi Easy Connect (DPP) would let the phone push its own credentials with no
// network to pick at all, and it is deliberately not used: no prebuilt
// Arduino-ESP32 SDK ships it (CONFIG_WPA_DPP_SUPPORT is unset in every one),
// enabling it means building Arduino as an ESP-IDF component, and iOS does not
// support DPP for third-party devices anyway. The captive portal is the only
// path that works for every phone.
void showWifi(const char* apSsid, int clientsConnected);
void showWifiConnecting(const char* ssid, int secondsLeft);
void showWifiFailed(const char* ssid);

// ---- 3. Account -----------------------------------------------------------
// The pairing QR carries the code in its URL, so scanning it is the whole
// interaction - nothing is read off one screen and typed into another. The
// short code underneath is for a phone that will not scan, and to be read
// aloud when someone is helping over the phone.
void showAccountIntro();
void showPairing(const char* verifyUrl, const char* code, int secondsLeft);
void showPairFailed(const char* reason);
bool takeStartPairing();

void hide();                  // release the LVGL objects
bool active();

}  // namespace screen_setup
