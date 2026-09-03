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

}  // namespace screen_settings
