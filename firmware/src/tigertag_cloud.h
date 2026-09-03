#pragma once
#include <Arduino.h>

// The TigerTag account: sign in, and import the user's printers.
//
// This is the premise of the product. Printers are added once in Tiger Studio,
// on a computer with a keyboard; the device reads them from the account and
// configures itself. Nothing here is ever typed on a 2.0" screen.
//
// Two ways in, both without a device keyboard:
//   - email and password, typed on the phone through the config page
//   - a QR pairing flow for accounts created with Google, which have no
//     password to type. See docs/ACCOUNT-PAIRING.md.
//
namespace ttcloud {
    void   begin();                       // load the stored session from NVS
    bool   haveSession();
    String email();
    String lastResult();

    // Email and password. Stores the refresh token, never the password.
    bool   signIn(const String& mail, const String& pass, String& err);
    void   forget();                      // sign out and clear the session

    // QR pairing, for accounts created with Google. The URL in the QR already
    // carries the code, so scanning it is the whole interaction - nothing is
    // read off one screen and typed into another.
    //   1) pairStart() -> short code + verify URL to display + poll token
    //   2) pairPoll()  -> repeat until approved, yields a custom token
    //   3) signInWithCustomToken() -> stores the session
    // The uid comes from the JWT claim: signInWithCustomToken does not return
    // localId in the body.
    bool   pairStart(String& code, String& verifyUrl, String& pollToken,
                     int& intervalS, String& err);
    int    pairPoll(const String& pollToken, String& customToken,
                    String& emailOut, String& err);   // <0 error, 0 pending, 1 approved, 2 denied, 3 expired
    bool   signInWithCustomToken(const String& customToken, const String& emailHint,
                                 String& err);

    bool   due();                         // is a re-sync due?
    bool   syncNow(String& summary);      // blocking; prefer startAsyncSync()
    bool   consumeChanged();              // true once, if the last sync changed anything

    // Runs the sync on its own task. The home screen is where the user comes
    // back constantly and it must never wait on the network: the list is drawn
    // from NVS immediately, and this updates it underneath.
    bool   startAsyncSync();              // false se ja houver uma a correr
    bool   asyncBusy();
    bool   asyncTake(String& summary);    // true once, when it finishes

    // pairStart on its own task. It is a blocking HTTPS round trip of a second
    // or two, and it happens while the screen is showing a spinner - a spinner
    // that freezes for the whole wait is worse than no spinner, because it
    // reads as a crash.
    bool   startPairAsync();
    bool   pairAsyncBusy();
    // 0 while running, 1 when the code is ready, -1 on failure.
    int    pairAsyncTake(String& code, String& verifyUrl, String& pollToken,
                         int& intervalS, String& err);
}
