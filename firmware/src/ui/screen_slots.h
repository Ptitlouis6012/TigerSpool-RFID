#pragma once
#include "printer.h"

// The slot grid of the selected printer.
//
// It scrolls. The prototype paginated six at a time, which was already wrong for
// a Bambu X1 reporting seventeen trays and is worse for an Anycubic Kobra X
// reporting twenty - and its page arrows were a 2 mm target for moving past a
// 12 mm one.
namespace screen_slots {

void show(const char* printerName, PrinterBackend* backend,
          int selected, bool readerReady);
void invalidate();                 // force a rebuild on the next show()
int  takeTappedSlot();             // slot index, or -1
bool takeBack();

}  // namespace screen_slots
