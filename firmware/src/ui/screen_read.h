#pragma once
#include "reader.h"

// Reader mode: put a spool on the pad and see what is on it.
//
// Not the NFC tester. That screen exists to check a chip field by field - every
// id, the raw pages, the stamp - and it is a bench instrument that happens to
// live in Settings. This one answers the question somebody holding a spool
// actually has: what is this, and what do I set the printer to?
//
// So it shows six things and hides thirty: the colour, the material and its
// finish, the brand, the two temperature windows, the diameter, what is left on
// the spool, and a quiet mark saying the chip proved itself genuine. Everything
// else is still one tap away in the tester.
namespace screen_read {

// Waiting for a spool. Cheap to re-call: it draws once.
void showWaiting();

// A spool has been read. Cheap to re-call with the same tag.
void showTag(const TagInfo& tag);

// The back arrow was pressed.
bool takeBack();

// Forget what is on screen, so the next show() rebuilds it.
void invalidate();

}  // namespace screen_read
