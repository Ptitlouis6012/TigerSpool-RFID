#pragma once
#include "printer.h"

// Settings, and the printer picker that lives inside it.
namespace screen_settings {

// The menu. Entries are fixed; what each one shows on the right is its current
// value, so the list answers "what is it set to" without opening anything.
enum Entry {
    E_NONE = -1,
    E_PRINTERS = 0,
    E_WIFI,
    E_ACCOUNT,
    E_SCREEN,
    E_LANGUAGE,
    E_UPDATE,
    E_RESTART,
    E_FACTORY,
    E_COUNT
};

void showMenu(const char* network, const char* account,
              int visiblePrinters, int totalPrinters);
Entry takeEntry();
bool  takeBack();
void  invalidate();

// The printer picker: every printer the account knows about, each with a switch.
//
// Hiding is not deleting. A hidden printer stays in the account and stays
// synced; it simply does not crowd a 2.0" screen belonging to someone who owns
// three machines and cares about one of them today.
void showPrinters(const PrinterCfg* printers, int count);
int  takeToggled();          // index whose switch was flipped, or -1

// ---- the rest of the settings views ---------------------------------------
//
// Each one answers a question and offers at most one action. A settings screen
// that lists five things you could do is a screen nobody reads.
enum Action { A_NONE = 0, A_CHANGE_WIFI, A_SIGN_OUT, A_RESTART, A_FACTORY, A_CHECK_UPDATE };
Action takeAction();

void showWifi(const char* ssid, const char* ip, const char* mac, bool connected);
void showAccount(const char* email, int printers, bool linked);
void showScreen(uint8_t brightness, int sleepSeconds);
int  takeBrightness();       // new percentage, or -1
int  takeSleep();            // new timeout in seconds, or -1
void showUpdate(const char* version, const char* channel,
                int otaState, const char* latest, int percent);
void showRestart();
void showFactory(int holdPercent);   // -1 = not holding
bool factoryHolding();               // true while the finger is down

}  // namespace screen_settings
