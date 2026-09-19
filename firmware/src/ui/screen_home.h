#pragma once
#include "printer.h"

// The home screen: the printers imported from the TigerTag account.
//
// This is the screen a user comes back to constantly, so it does two things the
// prototype's version could not. It SCROLLS instead of paginating - a Bambu with
// four AMS units and an 11-printer account both overflow four rows, and the
// pagination arrows were a smaller target than the rows they scrolled. And it
// never waits on the network: the list is drawn from NVS immediately, and the
// account sync updates it from a background task.
namespace screen_home {

// What a printer's dot says. Connecting is its own answer: red for a machine
// the device is actively dialling is a lie that lasts several seconds, and
// it is the several seconds during which somebody is watching.
enum Dot : uint8_t { DOT_OFF = 0, DOT_UP = 1, DOT_TRYING = 2 };


// Builds the screen on first call, refreshes it afterwards. Cheap to re-call.
// `wifiRssi` is dBm, or 0 when there is no connection. It is bucketed into
// four levels before it reaches the screen's redraw signature: raw dBm moves by
// a few points every second on a still desk, and a screen that rebuilds itself
// on that loses the scroll position while someone is reading it.
// `account` is ttcloud::health(): 2 reachable, 1 signed in but unreachable,
// 0 not signed in. It replaced a dot that reported whether a sync happened to
// be running - true for a second every five minutes, and grey the rest of the
// time, which is not something anyone can act on.
// The first screen, and a choice rather than a list: the printers, or the
// reader. Same header as the list below it - the two are one screen wearing
// two faces, so the account and Wi-Fi icons do not move when you go in and out.
//
// `printersUp` and `printersTotal` are shown beside the first row: what a
// person wants to know before pressing it is whether anything is connected.
void showMain(int printersUp, int printersTotal, bool readerReady,
              int wifiRssi, int account);
bool takeGoPrinters();
bool takeGoReader();
// The chevron on the printer list, which only that face shows: it goes back to
// the choice above.
bool takeBack();

void show(const PrinterCfg* printers, int count,
          int selected, const uint8_t* state, bool syncing, int wifiRssi,
          int account);

// True while this screen owns the display, so the legacy raw-drawn screens know
// to leave the canvas alone.
bool active();
void leave();

// Set by the screen when the user taps something. -1 means nothing pending.
int  takeTappedPrinter();   // index into printers[], or -1
bool takeSettingsTap();
// The button on the empty list: take me to the printer picker. It is the only
// way out of an empty home screen that does not start with "open Settings".
bool takePickTap();

}  // namespace screen_home
