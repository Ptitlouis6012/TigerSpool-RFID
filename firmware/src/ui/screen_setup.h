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
void showWifi(const char* apSsid);
void showWifiConnecting(const char* ssid, int secondsLeft);
void showWifiFailed(const char* ssid);

// ---- 3. Account -----------------------------------------------------------
// The pairing QR carries the code in its URL, so scanning it is the whole
// interaction - nothing is read off one screen and typed into another. The
// short code underneath is for a phone that will not scan, and to be read
// aloud when someone is helping over the phone.
// Two ways in, and the device cannot guess which. An account made with Google
// has no password to type; one made with an email has no Google to fall back
// on. Asking is one tap and removes a dead end.
void showSignInChoice();
int  takeSignInChoice();      // 0 = email + password, 1 = Google, -1 = none

// The email route: a QR of the device's own address, so the form opens on the
// phone where there is a keyboard and a password manager.
void showEmailPairing(const char* deviceUrl);

// Shown while the pairing code is being fetched. No QR: rendering a code that
// is not the real one invites someone to scan it, and swapping it underneath
// them a second later is worse than making them wait.
void showPreparing();

void showAccountIntro();
void showPairing(const char* verifyUrl, const char* code, int secondsLeft);
void showPairFailed(const char* reason);
bool takeStartPairing();

// Every screen reached from the sign-in choice offers a way back to it. A
// route chosen by mistake must not be a dead end - and on a device with no
// keyboard, the way out of the wrong route cannot be "start over".
bool takeBack();

void hide();                  // release the LVGL objects
bool active();

}  // namespace screen_setup
